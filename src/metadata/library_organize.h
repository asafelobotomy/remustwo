#pragma once

#include "../core/result.h"
#include "../core/rom_bundler.h"

#include <QString>
#include <QStringList>

namespace remustwo {

struct OrganizeLibraryOptions {
    bool dryRun = false;
    bool bundle = false;
    bool includeArt = false;
    bool online = false;
    BundleConvertMode convert = BundleConvertMode::Auto;
};

struct OrganizeLibraryStats {
    int organized = 0;
    int failed = 0;
    int skipped = 0;
    int skippedUnmatched = 0;
    int playlists = 0;
    QStringList archiveEntries;
};

Result<OrganizeLibraryStats> organizeLibrary(const QString &catalogDbPath, const QString &libraryDbPath,
    const QString &dest, const OrganizeLibraryOptions &options = {});

} // namespace remustwo
