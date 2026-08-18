#include "library_scan.h"

#include "constants/systems.h"
#include "database.h"
#include "hasher.h"
#include "scanner.h"

#include <QFileInfo>

namespace remustwo {

Result<ScanLibraryStats> scanLibrary(const QString &scanDir, const QString &libraryDbPath) {
    const QFileInfo scanInfo(scanDir);
    if (!scanInfo.exists() || !scanInfo.isDir())
        return Result<ScanLibraryStats>::fail(QStringLiteral("scan: PATH must be a directory"));

    Scanner scanner;
    scanner.setArchiveScanning(false);
    scanner.setExtensions(Constants::Systems::EXTENSION_TO_SYSTEMS.keys());
    const QList<ScanResult> results = scanner.scan(scanDir);

    Database db;
    if (!db.initialize(libraryDbPath))
        return Result<ScanLibraryStats>::fail(QStringLiteral("Failed to open library database"));

    const int libraryId = db.insertLibrary(scanDir, QStringLiteral("scan"));
    Hasher hasher;
    ScanLibraryStats stats;
    stats.scanned = results.size();
    for (const ScanResult &item : results) {
        FileRecord record;
        record.libraryId = libraryId;
        record.originalPath = item.path;
        record.currentPath = item.path;
        record.filename = item.filename;
        record.extension = item.extension;
        record.fileSize = item.fileSize;
        record.lastModified = item.lastModified;
        const int fileId = db.insertFile(record);
        if (fileId <= 0)
            continue;
        ++stats.stored;
        const HashResult hashes = hasher.calculateHashes(item.path);
        if (hashes.success && db.updateFileHashes(fileId, hashes.crc32, hashes.md5, hashes.sha1))
            ++stats.hashed;
    }
    return Result<ScanLibraryStats>::ok(stats);
}

} // namespace remustwo
