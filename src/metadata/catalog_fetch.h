#pragma once

#include "../core/result.h"

#include <QString>
#include <QStringList>
#include <QUrl>

namespace remustwo {

struct CatalogFetchOptions {
    /// Remustwo internal name (e.g. "PlayStation") or Redump slug (e.g. "psx").
    /// Empty with empty fromDir means fetch all mapped Redump systems.
    QString system;
    /// Ingest every *.dat under this directory (No-Intro / local DATs). Skips HTTP.
    QString fromDir;
    bool dryRun = false;
    bool ingest = true;
};

struct CatalogFetchStats {
    int downloaded = 0;
    int extracted = 0;
    int ingested = 0;
    int skipped = 0;
    QStringList datPaths;
    QStringList messages;
};

/// User data dir for cached Redump DAT downloads (~/.local/share/remustwo/dats).
QString datCacheDir();

/// Map remustwo system name / Redump slug → Redump URL slug. Empty if unknown.
QString redumpSlugForSystem(const QString &systemOrSlug);

/// http://redump.org/datfile/<slug>/
QUrl redumpDatUrl(const QString &slug);

/// Download and/or ingest DATs into the catalog database.
Result<CatalogFetchStats> fetchCatalog(const QString &catalogDbPath, const CatalogFetchOptions &options);

/// Extract the first .dat member from a Redump zip into destDir. Returns the .dat path.
Result<QString> extractDatFromZip(const QString &zipPath, const QString &destDir);

} // namespace remustwo
