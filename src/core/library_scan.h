#pragma once

#include "result.h"

#include <QString>

namespace remustwo {

struct ScanLibraryStats {
    int scanned = 0;
    int stored = 0;
    int hashed = 0;
};

Result<ScanLibraryStats> scanLibrary(const QString &scanDir, const QString &libraryDbPath);

} // namespace remustwo
