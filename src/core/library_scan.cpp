#include "library_scan.h"

#include "archive_extractor.h"
#include "constants/systems.h"
#include "database.h"
#include "disc_set_utils.h"
#include "extended_hashes.h"
#include "hasher.h"
#include "scanner.h"
#include "system_detector.h"

#include <QFileInfo>
#include <QHash>
#include <QTemporaryDir>

namespace remustwo {

namespace {

    QString scanItemKey(const ScanResult &item) {
        if (item.isCompressed && !item.archivePath.isEmpty() && !item.archiveInternalPath.isEmpty())
            return item.archivePath + QStringLiteral("::") + item.archiveInternalPath;
        return item.path;
    }

} // namespace

Result<ScanLibraryStats> scanLibrary(const QString &scanDir, const QString &libraryDbPath) {
    return scanLibrary(scanDir, libraryDbPath, ScanLibraryOptions {});
}

Result<ScanLibraryStats> scanLibrary(
    const QString &scanDir, const QString &libraryDbPath, const ScanLibraryOptions &options) {
    const QFileInfo scanInfo(scanDir);
    if (!scanInfo.exists() || !scanInfo.isDir())
        return Result<ScanLibraryStats>::fail(QStringLiteral("scan: PATH must be a directory"));

    Scanner scanner;
#ifdef REMUSTWO_HAS_LIBARCHIVE
    scanner.setArchiveScanning(!options.noArchives);
#else
    Q_UNUSED(options);
    scanner.setArchiveScanning(false);
#endif
    scanner.setExtensions(Constants::Systems::EXTENSION_TO_SYSTEMS.keys());
    const QList<ScanResult> results = scanner.scan(scanDir);

    Database db;
    if (!db.initialize(libraryDbPath))
        return Result<ScanLibraryStats>::fail(QStringLiteral("Failed to open library database"));

    const int libraryId = db.insertLibrary(scanDir, QStringLiteral("scan"));
    Hasher hasher;
    SystemDetector detector;
    ArchiveExtractor extractor;
    ScanLibraryStats stats;
    stats.scanned = results.size();
    QHash<QString, int> idsByKey;
    for (const ScanResult &item : results) {
        FileRecord record;
        record.libraryId = libraryId;
        record.originalPath = item.path;
        record.currentPath = item.path;
        record.filename = item.filename;
        record.extension = item.extension;
        record.fileSize = item.fileSize;
        record.lastModified = item.lastModified;
        record.isPrimary = item.isPrimary;
        record.isCompressed = item.isCompressed;
        record.archivePath = item.archivePath;
        record.archiveInternalPath = item.archiveInternalPath;
        DiscSetUtils::applyScanDiscMetadata(record, item.detectedSystem);
        if (record.systemId <= 0 && !item.detectedSystem.isEmpty())
            record.systemId = db.getSystemId(item.detectedSystem);
        if (record.systemId <= 0) {
            const QString detectPath
                = item.isCompressed && !item.archiveInternalPath.isEmpty() ? item.archiveInternalPath : item.path;
            const QString detected = detector.detectSystem(item.extension, detectPath);
            if (!detected.isEmpty())
                record.systemId = db.getSystemId(detected);
        }
        const int fileId = db.insertFile(record);
        if (fileId <= 0)
            continue;
        idsByKey.insert(scanItemKey(item), fileId);
        ++stats.stored;

        HashResult hashes;
        QString hashPath;
        QTemporaryDir tempExtract;
        if (item.isCompressed && !item.archivePath.isEmpty() && !item.archiveInternalPath.isEmpty()
            && extractor.canExtract(item.archivePath) && tempExtract.isValid()) {
            const ExtractionResult extracted
                = extractor.extractFile(item.archivePath, item.archiveInternalPath, tempExtract.path());
            if (extracted.success && !extracted.extractedFiles.isEmpty())
                hashPath = extracted.extractedFiles.first();
        } else if (!item.isCompressed) {
            hashPath = item.path;
        }

        if (!hashPath.isEmpty()) {
            hashes = hasher.calculateContentHashes(hashPath);
            if (hashes.success) {
                populateExtendedHashes(hashes, { hashPath, record.systemId, item.extension });
                if (db.updateFileHashes(fileId, hashes.crc32, hashes.md5, hashes.sha1, hashes.raMd5, hashes.chdSha1,
                        hashes.rvzSha1))
                    ++stats.hashed;
            }
        }
    }
    for (const ScanResult &item : results) {
        if (item.parentFilePath.isEmpty() || !idsByKey.contains(scanItemKey(item))
            || !idsByKey.contains(item.parentFilePath)) {
            continue;
        }
        db.updateFileParent(idsByKey.value(scanItemKey(item)), idsByKey.value(item.parentFilePath));
    }
    return Result<ScanLibraryStats>::ok(stats);
}

} // namespace remustwo
