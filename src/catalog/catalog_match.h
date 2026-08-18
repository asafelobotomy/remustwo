#pragma once

#include "../core/matching_engine.h"
#include "../core/result.h"

#include <QString>

namespace remustwo::catalog {

struct CatalogMatch {
    QString gameId;
    QString title;
    int confidence = 0;
    QString matchedHash;
    int systemId = 0;
};

Result<CatalogMatch> matchFile(const QString &dbPath, const QString &filePath);
Result<CatalogMatch> matchFile(const QString &dbPath, const QString &filePath, const QString &libraryPath);
Result<int> matchLibrary(const QString &dbPath, const QString &libraryPath);

} // namespace remustwo::catalog
