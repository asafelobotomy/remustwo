#pragma once

#include "../core/matching_engine.h"
#include "../core/result.h"

#include <QString>
#include <functional>

namespace remustwo::catalog {

struct CatalogMatch {
    QString gameId;
    QString title;
    int confidence = 0;
    QString matchedHash;
    int systemId = 0;
};

struct CatalogOnlineHit {
    QString title;
    QString coverUrl;
    QString description;
    QString externalId;
};

struct MatchOptions {
    bool online = false;
    bool dryRun = false;
    std::function<CatalogOnlineHit(const QString &crc32, const QString &md5, const QString &sha1)> lookup;
};

struct MatchLibraryStats {
    int matched = 0;
    int onlineMatched = 0;
    int negativeCached = 0;
    int onlineMisses = 0;
};

Result<CatalogMatch> matchFile(const QString &dbPath, const QString &filePath);
Result<CatalogMatch> matchFile(const QString &dbPath, const QString &filePath, const QString &libraryPath);
Result<int> matchLibrary(const QString &dbPath, const QString &libraryPath);
Result<MatchLibraryStats> matchLibrary(const QString &dbPath, const QString &libraryPath, const MatchOptions &options);

} // namespace remustwo::catalog
