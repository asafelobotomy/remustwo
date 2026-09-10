#include "catalog_fetch.h"

#include "../catalog/catalog_ingest.h"
#include "../core/archive_extractor.h"
#include "../core/constants/api.h"
#include "../core/constants/network.h"
#include "../core/constants/system_ids.h"
#include "../core/constants/systems.h"
#include "http_client.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

// Redump DAT zips: prefer libarchive (REMUSTWO_ENABLE_LIBARCHIVE). unzip is a
// last-resort fallback when the binary was built without libarchive.

namespace remustwo {

namespace {

    using namespace Constants::Systems;

    struct RedumpEntry {
        int systemId;
        const char *slug;
    };

    const QList<RedumpEntry> &redumpTable() {
        static const QList<RedumpEntry> table = {
            { ID_GAMECUBE, "gc" },
            { ID_WII, "wii" },
            { ID_SATURN, "ss" },
            { ID_DREAMCAST, "dc" },
            { ID_PSX, "psx" },
            { ID_PS2, "ps2" },
            { ID_PSP, "psp" },
            { ID_PS3, "ps3" },
            { ID_SEGA_CD, "mcd" },
            { ID_TURBOGRAFX_CD, "pce" },
            { ID_3DO, "3do" },
            { ID_CDI, "cdi" },
            { ID_CD32, "cd32" },
            { ID_NEO_GEO_CD, "ngcd" },
            { ID_PC_FX, "pc-fx" },
            { ID_NAOMI, "naomi" },
            { ID_ATARI_JAGUAR_CD, "ajcd" },
            { ID_XBOX, "xbox" },
            { ID_XBOX360, "xbox360" },
            { ID_IBM_PC, "pc" },
            { ID_MAC, "mac" },
            { ID_FM_TOWNS, "fmt" },
            { ID_PC88, "pc-88" },
            { ID_PC98, "pc-98" },
            { ID_X68000, "x68k" },
            { ID_CDTV, "cdtv" },
            { ID_AMIGA_CD, "acd" },
            { ID_ARCHIMEDES, "arch" },
            { ID_TRIFORCE, "trf" },
            { ID_CHIHIRO, "chihiro" },
            { ID_TANDY_VIS, "vis" },
            { ID_LINDBERGH, "lindbergh" },
            { ID_PALM_OS, "palm" },
            { ID_PLAYDIA, "qis" },
        };
        return table;
    }

    QString slugForSystemId(int systemId) {
        for (const RedumpEntry &entry : redumpTable()) {
            if (entry.systemId == systemId)
                return QString::fromLatin1(entry.slug);
        }
        return {};
    }

    Result<QString> findExtractedDat(const QString &destDir) {
        QDirIterator it(destDir, { QStringLiteral("*.dat"), QStringLiteral("*.DAT") }, QDir::Files,
            QDirIterator::Subdirectories);
        if (it.hasNext())
            return Result<QString>::ok(it.next());
        return Result<QString>::fail(QStringLiteral("No .dat found in Redump zip"));
    }

    Result<QString> extractWithUnzip(const QString &zipPath, const QString &destDir) {
        if (!QDir().mkpath(destDir))
            return Result<QString>::fail(QStringLiteral("Failed to create DAT extract directory"));

        QProcess proc;
        proc.setWorkingDirectory(destDir);
        proc.start(QStringLiteral("unzip"),
            { QStringLiteral("-o"), QStringLiteral("-j"), zipPath, QStringLiteral("*.dat"),
                QStringLiteral("*.DAT") });
        if (!proc.waitForStarted(5000)) {
            return Result<QString>::fail(QStringLiteral(
                "Redump DAT extract needs libarchive (rebuild with -DREMUSTWO_ENABLE_LIBARCHIVE=ON) "
                "or an unzip binary on PATH"));
        }
        if (!proc.waitForFinished(120000) || proc.exitCode() != 0) {
            const QString err = QString::fromLocal8Bit(proc.readAllStandardError()).trimmed();
            return Result<QString>::fail(err.isEmpty() ? QStringLiteral("unzip failed") : err);
        }
        return findExtractedDat(destDir);
    }

    Result<QString> downloadRedumpZip(const QString &slug, const QString &zipPath) {
        HttpClient client;
        const HttpResponse response
            = client.get(redumpDatUrl(slug), Constants::Network::DAT_DOWNLOAD_TIMEOUT_MS);
        if (!response.error.isEmpty())
            return Result<QString>::fail(response.error);
        if (response.statusCode != 200 || response.body.isEmpty()) {
            return Result<QString>::fail(
                QStringLiteral("Redump download failed for %1 (HTTP %2)").arg(slug).arg(response.statusCode));
        }
        if (!response.body.startsWith("PK")) {
            return Result<QString>::fail(QStringLiteral("Redump response for %1 is not a zip").arg(slug));
        }

        QFileInfo(zipPath).dir().mkpath(QStringLiteral("."));
        QFile file(zipPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return Result<QString>::fail(file.errorString());
        if (file.write(response.body) != response.body.size())
            return Result<QString>::fail(QStringLiteral("Failed to write Redump zip"));
        file.close();
        return Result<QString>::ok(zipPath);
    }

    Result<int> ingestDatPaths(const QString &catalogDbPath, const QStringList &datPaths, bool dryRun,
        CatalogFetchStats &stats) {
        for (const QString &datPath : datPaths) {
            stats.datPaths.append(datPath);
            if (dryRun) {
                stats.messages.append(QStringLiteral("would ingest %1").arg(datPath));
                ++stats.skipped;
                continue;
            }
            auto ingested = catalog::ingestDat(catalogDbPath, datPath);
            if (!ingested) {
                stats.messages.append(QStringLiteral("%1: %2").arg(datPath, ingested.error()));
                return Result<int>::fail(ingested.error());
            }
            ++stats.ingested;
            stats.messages.append(QStringLiteral("ingested %1").arg(datPath));
        }
        return Result<int>::ok(stats.ingested);
    }

    QStringList resolveSlugs(const QString &systemFilter) {
        if (systemFilter.trimmed().isEmpty()) {
            QStringList slugs;
            for (const RedumpEntry &entry : redumpTable())
                slugs.append(QString::fromLatin1(entry.slug));
            return slugs;
        }
        const QString slug = redumpSlugForSystem(systemFilter);
        if (slug.isEmpty())
            return {};
        return { slug };
    }

} // namespace

QString datCacheDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString dir = base + QStringLiteral("/remustwo/dats");
    QDir().mkpath(dir);
    return dir;
}

QString redumpSlugForSystem(const QString &systemOrSlug) {
    const QString raw = systemOrSlug.trimmed();
    if (raw.isEmpty())
        return {};

    const QString lower = raw.toLower();
    for (const RedumpEntry &entry : redumpTable()) {
        if (QString::fromLatin1(entry.slug).compare(lower, Qt::CaseInsensitive) == 0)
            return QString::fromLatin1(entry.slug);
    }

    const int byName = Constants::Systems::getSystemIdByName(raw);
    if (byName > 0) {
        const QString slug = slugForSystemId(byName);
        if (!slug.isEmpty())
            return slug;
    }

    // Accept common aliases
    if (lower == QStringLiteral("playstation") || lower == QStringLiteral("ps1") || lower == QStringLiteral("psx"))
        return QStringLiteral("psx");
    if (lower == QStringLiteral("playstation 2") || lower == QStringLiteral("ps2"))
        return QStringLiteral("ps2");
    if (lower == QStringLiteral("sega cd") || lower == QStringLiteral("mega cd"))
        return QStringLiteral("mcd");
    if (lower == QStringLiteral("saturn"))
        return QStringLiteral("ss");

    return slugForSystemId(byName);
}

QUrl redumpDatUrl(const QString &slug) {
    return QUrl(QString::fromLatin1(Constants::API::REDUMP_DAT_BASE_URL) + slug + QLatin1Char('/'));
}

Result<QString> extractDatFromZip(const QString &zipPath, const QString &destDir) {
    ArchiveExtractor extractor;
    if (extractor.canExtract(zipPath)) {
        if (!QDir().mkpath(destDir))
            return Result<QString>::fail(QStringLiteral("Failed to create DAT extract directory"));
        const ExtractionResult extracted = extractor.extract(zipPath, destDir, false);
        if (!extracted.success)
            return Result<QString>::fail(extracted.error);
        for (const QString &path : extracted.extractedFiles) {
            if (path.endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive))
                return Result<QString>::ok(path);
        }
        return findExtractedDat(destDir);
    }
    // Built without libarchive — last-resort unzip.
    return extractWithUnzip(zipPath, destDir);
}

Result<CatalogFetchStats> fetchCatalog(const QString &catalogDbPath, const CatalogFetchOptions &options) {
    CatalogFetchStats stats;

    if (!options.fromDir.trimmed().isEmpty()) {
        const QDir dir(options.fromDir);
        if (!dir.exists())
            return Result<CatalogFetchStats>::fail(QStringLiteral("from-dir does not exist: ") + options.fromDir);
        const QFileInfoList files = dir.entryInfoList({ QStringLiteral("*.dat"), QStringLiteral("*.DAT") },
            QDir::Files, QDir::Name);
        if (files.isEmpty())
            return Result<CatalogFetchStats>::fail(QStringLiteral("No .dat files in ") + options.fromDir);

        QStringList paths;
        for (const QFileInfo &info : files)
            paths.append(info.absoluteFilePath());

        if (!options.ingest) {
            stats.datPaths = paths;
            stats.skipped = paths.size();
            stats.messages.append(QStringLiteral("found %1 DAT(s); ingest skipped").arg(paths.size()));
            return Result<CatalogFetchStats>::ok(stats);
        }
        auto ingested = ingestDatPaths(catalogDbPath, paths, options.dryRun, stats);
        if (!ingested)
            return Result<CatalogFetchStats>::fail(ingested.error());
        return Result<CatalogFetchStats>::ok(stats);
    }

    const QStringList slugs = resolveSlugs(options.system);
    if (slugs.isEmpty()) {
        return Result<CatalogFetchStats>::fail(
            QStringLiteral("Unknown Redump system: ") + options.system
            + QStringLiteral(" (use a remustwo system name, Redump slug, or --from-dir)"));
    }

    const QString cacheRoot = datCacheDir() + QStringLiteral("/redump");
    QDir().mkpath(cacheRoot);

    QStringList datPaths;
    for (const QString &slug : slugs) {
        const QString slugDir = cacheRoot + QLatin1Char('/') + slug;
        QDir().mkpath(slugDir);
        const QString zipPath = slugDir + QStringLiteral("/datfile.zip");
        const QString extractDir = slugDir + QStringLiteral("/extract");

        if (options.dryRun) {
            stats.messages.append(QStringLiteral("would fetch %1").arg(redumpDatUrl(slug).toString()));
            ++stats.skipped;
            continue;
        }

        auto downloaded = downloadRedumpZip(slug, zipPath);
        if (!downloaded) {
            stats.messages.append(QStringLiteral("%1: %2").arg(slug, downloaded.error()));
            return Result<CatalogFetchStats>::fail(downloaded.error());
        }
        ++stats.downloaded;

        QDir(extractDir).removeRecursively();
        QDir().mkpath(extractDir);
        auto extracted = extractDatFromZip(zipPath, extractDir);
        if (!extracted) {
            stats.messages.append(QStringLiteral("%1: %2").arg(slug, extracted.error()));
            return Result<CatalogFetchStats>::fail(extracted.error());
        }
        ++stats.extracted;
        datPaths.append(*extracted);
        stats.messages.append(QStringLiteral("fetched %1 -> %2").arg(slug, *extracted));
    }

    if (options.dryRun || !options.ingest)
        return Result<CatalogFetchStats>::ok(stats);

    auto ingested = ingestDatPaths(catalogDbPath, datPaths, false, stats);
    if (!ingested)
        return Result<CatalogFetchStats>::fail(ingested.error());
    return Result<CatalogFetchStats>::ok(stats);
}

} // namespace remustwo
