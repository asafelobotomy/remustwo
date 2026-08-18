#include "rom_bundler.h"

#include "archive_creator.h"
#include "archive_extractor.h"
#include "chd_converter.h"
#include "constants/files.h"
#include "constants/settings.h"
#include "constants/systems.h"
#include "cso_converter.h"
#include "database.h"
#include "hasher.h"
#include "rvz_converter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace remustwo {

namespace {

QString dottedExtension(const QString &path) {
    const QString suffix = QFileInfo(path).suffix();
    return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix.toLower();
}

bool isAlreadyCompressedContainer(const QString &extension) {
    return Constants::Files::isArchiveExtension(extension)
        || extension == Constants::Files::CHD || extension == Constants::Files::RVZ
        || extension == Constants::Files::CSO;
}

QString plannedConvertedExtension(const QString &sourcePath, const FileRecord &file, BundleConvertMode mode) {
    const QString ext = dottedExtension(sourcePath);
    if (mode == BundleConvertMode::Never || isAlreadyCompressedContainer(ext))
        return ext;
    if (file.systemId == Constants::Systems::ID_PSP && ext == Constants::Files::ISO) {
        CSOConverter converter;
        return converter.isMaxcsoAvailable() ? Constants::Files::CSO : ext;
    }
    if ((file.systemId == Constants::Systems::ID_GAMECUBE || file.systemId == Constants::Systems::ID_WII)
        && (ext == Constants::Files::ISO || ext == Constants::Files::GCM)) {
        RVZConverter converter;
        return converter.isDolphinToolAvailable() ? Constants::Files::RVZ : ext;
    }
    const bool discIso
        = ext == Constants::Files::ISO && Constants::Systems::DISC_SYSTEMS.contains(file.systemId);
    if (ext == Constants::Files::CUE || ext == Constants::Files::GDI || discIso) {
        CHDConverter converter;
        return converter.isChdmanAvailable() ? Constants::Files::CHD : ext;
    }
    return ext;
}

QString convertPayload(const QString &sourcePath, const FileRecord &file, BundleConvertMode mode,
    QString &payloadExtension) {
    payloadExtension = plannedConvertedExtension(sourcePath, file, mode);
    if (payloadExtension == dottedExtension(sourcePath))
        return sourcePath;

    const QString ext = dottedExtension(sourcePath);
    if (payloadExtension == Constants::Files::CSO) {
        CSOConverter converter;
        const ConversionResult converted = converter.convertIsoToCSO(sourcePath);
        if (converted.success)
            return converted.outputPath;
        payloadExtension = ext;
        return sourcePath;
    }
    if (payloadExtension == Constants::Files::RVZ) {
        RVZConverter converter;
        const ConversionResult converted = converter.convertIsoToRVZ(sourcePath);
        if (converted.success)
            return converted.outputPath;
        payloadExtension = ext;
        return sourcePath;
    }
    if (payloadExtension == Constants::Files::CHD) {
        CHDConverter converter;
        ConversionResult converted;
        if (ext == Constants::Files::CUE)
            converted = converter.convertCueToCHD(sourcePath);
        else if (ext == Constants::Files::GDI)
            converted = converter.convertGdiToCHD(sourcePath);
        else
            converted = converter.convertIsoToCHD(sourcePath);
        if (converted.success)
            return converted.outputPath;
        payloadExtension = ext;
        return sourcePath;
    }
    return sourcePath;
}

QString safeFileStem(const GameMetadata &metadata, const FileRecord &file) {
    QString stem = metadata.title.isEmpty() ? file.baseTitle : metadata.title;
    if (stem.isEmpty())
        stem = QFileInfo(file.filename).completeBaseName();
    stem.replace(QLatin1Char('/'), QLatin1Char('_'));
    stem.replace(QLatin1Char('\\'), QLatin1Char('_'));
    return stem;
}

} // namespace

RomBundler::RomBundler(Database &database)
    : m_database(database) { }

bool RomBundler::isAlreadyBundled(const QString &archivePath) {
    if (!QFileInfo::exists(archivePath))
        return false;
    ArchiveExtractor extractor;
    const ArchiveInfo info = extractor.getArchiveInfo(archivePath);
    return info.contents.contains(QString::fromLatin1(Constants::Settings::Files::MARKER_PROCESSED));
}

QString RomBundler::generateMarkerContent(const FileRecord &file, const GameMetadata &metadata) const {
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QString out;
    QTextStream stream(&out);
    stream << "---\n";
    stream << "remus_processed: true\n";
    stream << "processed_at: " << now << "\n";
    stream << "catalog_game_id: " << file.catalogGameId << "\n";
    stream << "crc32: " << file.crc32 << "\n";
    stream << "md5: " << file.md5 << "\n";
    stream << "sha1: " << file.sha1 << "\n";
    stream << "---\n\n";
    const QString title = metadata.title.isEmpty() ? file.baseTitle : metadata.title;
    stream << "# " << title << "\n\n";
    stream << "| Field | Value |\n|---|---|\n";
    stream << "| Title | " << title << " |\n";
    if (!metadata.system.isEmpty())
        stream << "| System | " << metadata.system << " |\n";
    if (!metadata.region.isEmpty())
        stream << "| Region | " << metadata.region << " |\n";
    return out;
}

QString RomBundler::convertIfNeeded(const QString &sourcePath, const FileRecord &file, const BundleConfig &config,
    QString &payloadExtension) const {
    return convertPayload(sourcePath, file, config.convert, payloadExtension);
}

BundleResult RomBundler::bundle(
    const FileRecord &file, const GameMetadata &metadata, const QString &destinationDir, const BundleConfig &config) {
    BundleResult result;
    const QString sourcePath = file.currentPath;
    if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
        result.error = QStringLiteral("Source ROM not found: ") + sourcePath;
        return result;
    }

    if (!file.discSetKey.isEmpty() && m_database.getFilesByDiscSetKey(file.discSetKey).size() >= 2) {
        result.skippedDiscSet = true;
        result.success = true;
        result.outputPath = sourcePath;
        return result;
    }

    const QString extension = dottedExtension(sourcePath);
    if (isAlreadyCompressedContainer(extension) && isAlreadyBundled(sourcePath)) {
        result.skippedAlreadyBundled = true;
        result.success = true;
        result.outputPath = sourcePath;
        return result;
    }
    if (Constants::Files::isArchiveExtension(extension)) {
        result.skippedAlreadyBundled = true;
        result.success = true;
        result.outputPath = sourcePath;
        result.error = QStringLiteral("skip zip-of-zip");
        return result;
    }

    const QString stem = safeFileStem(metadata, file);
    const QString zipName = stem + QStringLiteral(".zip");
    result.outputPath = QDir(destinationDir).absoluteFilePath(zipName);

    QString payloadExtension = plannedConvertedExtension(sourcePath, file, config.convert);
    const QString romName = stem + payloadExtension;
    result.archiveEntries.append(romName);
    result.archiveEntries.append(QString::fromLatin1(Constants::Settings::Files::MARKER_PROCESSED));
    if (config.includeArt && !config.artworkPath.isEmpty() && QFile::exists(config.artworkPath))
        result.archiveEntries.append(QStringLiteral("artwork/boxfront.jpg"));
    result.archiveEntries.sort();

    if (config.dryRun) {
        result.success = true;
        return result;
    }

    const QString payloadPath = convertIfNeeded(sourcePath, file, config, payloadExtension);
    FileRecord markerFile = file;
    if (payloadPath != sourcePath) {
        Hasher hasher;
        const HashResult hashes = hasher.calculateHashes(payloadPath);
        if (hashes.success) {
            markerFile.crc32 = hashes.crc32;
            markerFile.md5 = hashes.md5;
            markerFile.sha1 = hashes.sha1;
        }
    }

    QDir dest(destinationDir);
    if (!dest.exists() && !dest.mkpath(QStringLiteral("."))) {
        result.error = QStringLiteral("Cannot create destination directory");
        return result;
    }

    const QString tempBase
        = dest.absolutePath() + QStringLiteral("/.remustwo_bundle_") + QString::number(QDateTime::currentMSecsSinceEpoch());
    if (!QDir().mkpath(tempBase)) {
        result.error = QStringLiteral("Cannot create staging directory");
        return result;
    }
    auto cleanup = [&]() { QDir(tempBase).removeRecursively(); };

    const QString stagedRom = tempBase + QLatin1Char('/') + stem + payloadExtension;
    if (!QFile::copy(payloadPath, stagedRom)) {
        result.error = QStringLiteral("Failed to stage ROM");
        cleanup();
        return result;
    }
    if (payloadPath != sourcePath)
        QFile::remove(payloadPath);
    QFile marker(tempBase + QLatin1Char('/') + QString::fromLatin1(Constants::Settings::Files::MARKER_PROCESSED));
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.error = QStringLiteral("Failed to write marker");
        cleanup();
        return result;
    }
    marker.write(generateMarkerContent(markerFile, metadata).toUtf8());
    marker.close();

    if (config.includeArt && !config.artworkPath.isEmpty() && QFile::exists(config.artworkPath)) {
        QDir().mkpath(tempBase + QStringLiteral("/artwork"));
        if (!QFile::copy(config.artworkPath, tempBase + QStringLiteral("/artwork/boxfront.jpg"))) {
            result.error = QStringLiteral("Failed to stage artwork");
            cleanup();
            return result;
        }
    }

    const QString tempZip = result.outputPath + QStringLiteral(".tmp");
    ArchiveCreator creator;
    const CompressionResult compressed = creator.compressDirectoryContents(tempBase, tempZip);
    if (!compressed.success) {
        result.error = compressed.error;
        QFile::remove(tempZip);
        cleanup();
        return result;
    }
    if (QFile::exists(result.outputPath))
        QFile::remove(result.outputPath);
    if (!QFile::rename(tempZip, result.outputPath)) {
        result.error = QStringLiteral("Failed to move bundle into place");
        QFile::remove(tempZip);
        cleanup();
        return result;
    }
    cleanup();
    if (!m_database.markFileBundled(file.id, result.outputPath)) {
        result.error = QStringLiteral("Bundle wrote but library update failed");
        return result;
    }
    result.success = true;
    return result;
}

} // namespace remustwo
