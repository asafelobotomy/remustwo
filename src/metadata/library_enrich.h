#pragma once

#include "../core/result.h"

#include "metadata_provider.h"

#include <QString>
#include <QUrl>
#include <functional>

namespace remustwo {

struct EnrichStats {
    int matchedGames = 0;
    int complete = 0;
    int negativeCached = 0;
    int wouldFetch = 0;
    int fetched = 0;
    int proxyFetched = 0;
    int updated = 0;
    int thumbnailUrls = 0;
    int assetsWritten = 0;
};

struct EnrichOptions {
    bool online = false;
    bool deep = false;
    bool dryRun = false;
    std::function<GameMetadata(const QString &crc32, const QString &md5, const QString &sha1)> lookup;
    /// Optional IGDB MetadataProxy fetch (defaults to HasheousProvider when deep + API key).
    std::function<GameMetadata(int igdbId)> fetchIgdb;
    /// Optional URL existence probe (defaults to HTTP HEAD when online).
    std::function<bool(const QUrl &url)> urlExists;
};

Result<EnrichStats> enrichLibrary(
    const QString &catalogDbPath, const QString &libraryDbPath, const EnrichOptions &options = {});

} // namespace remustwo
