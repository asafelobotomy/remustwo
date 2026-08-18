#include "archive_extractor.h"

#include <QDir>
#include <QFileInfo>

namespace remustwo {

ArchiveExtractor::ArchiveExtractor(QObject *parent)
    : QObject(parent) { }

QMap<ArchiveFormat, bool> ArchiveExtractor::getAvailableTools() const {
    return { };
}

bool ArchiveExtractor::canExtract(ArchiveFormat) const {
#ifdef REMUSTWO_HAS_LIBARCHIVE
    return true;
#else
    return false;
#endif
}

bool ArchiveExtractor::canExtract(const QString &) const {
    return canExtract(ArchiveFormat::ZIP);
}

ArchiveInfo ArchiveExtractor::getArchiveInfo(const QString &path) {
    ArchiveInfo info;
    info.path = path;
    info.format = detectFormat(path);
    return info;
}

ArchiveFormat ArchiveExtractor::detectFormat(const QString &path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String("zip"))
        return ArchiveFormat::ZIP;
    if (ext == QLatin1String("7z"))
        return ArchiveFormat::SevenZip;
    if (ext == QLatin1String("rar"))
        return ArchiveFormat::RAR;
    return ArchiveFormat::Unknown;
}

QString ArchiveExtractor::normalizeArchiveMemberPath(const QString &path) {
    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    normalized = QDir::fromNativeSeparators(normalized).trimmed();
    if (normalized.isEmpty())
        return { };
    if (normalized.contains(QStringLiteral("..")))
        return { };
    return normalized;
}

ExtractionResult ArchiveExtractor::extract(const QString &archivePath, const QString &, bool) {
    ExtractionResult result;
    result.archivePath = archivePath;
#ifndef REMUSTWO_HAS_LIBARCHIVE
    result.error = QStringLiteral("Archive extraction requires libarchive (build with -DREMUSTWO_ENABLE_LIBARCHIVE=ON)");
#endif
    return result;
}

ExtractionResult ArchiveExtractor::extractFile(const QString &archivePath, const QString &, const QString &) {
    return extract(archivePath);
}

QList<ExtractionResult> ArchiveExtractor::batchExtract(const QStringList &, const QString &, bool) {
    return { };
}

QByteArray ArchiveExtractor::readMemberPrefix(const QString &, const QString &, qint64) {
    return { };
}

ExtractionResult ArchiveExtractor::extractToDir(const QString &archivePath, const QString &, const QString &) {
    return extract(archivePath);
}

} // namespace remustwo
