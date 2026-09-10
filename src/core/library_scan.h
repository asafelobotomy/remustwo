#pragma once

#include "result.h"

#include <QString>

namespace remustwo {

struct ScanLibraryStats {
    int scanned = 0;
    int stored = 0;
    int hashed = 0;
};

struct ScanLibraryOptions {
    /// When true, never recurse into zip/7z/rar even if built with libarchive.
    bool noArchives = false;
};

Result<ScanLibraryStats> scanLibrary(const QString &scanDir, const QString &libraryDbPath);
Result<ScanLibraryStats> scanLibrary(
    const QString &scanDir, const QString &libraryDbPath, const ScanLibraryOptions &options);

} // namespace remustwo
