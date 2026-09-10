#include "library_enrich.h"

#include "../catalog/catalog.h"
#include "../catalog/sql_pragmas.h"
#include "../catalog/thumbnail_url_helper.h"
#include "../core/database.h"
#include "../core/disc_set_utils.h"
#include "../core/constants/network.h"
#include "hasheous_provider.h"
#include "http_client.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QThread>
#include <QUrl>

namespace remustwo {

namespace {

    QStringList libretroAssetTypes() {
        return { QStringLiteral("boxart"), QStringLiteral("snap"), QStringLiteral("title"), QStringLiteral("logo") };
    }

    QString cacheKeyForHashes(const QString &crc32, const QString &md5, const QString &sha1) {
        const QString digest = !sha1.isEmpty() ? sha1 : (!md5.isEmpty() ? md5 : crc32);
        return QStringLiteral("enrich:hasheous:%1").arg(digest.trimmed().toLower());
    }

    bool isNegativeCached(QSqlDatabase &library, const QString &key) {
        QSqlQuery query(library);
        query.prepare(QStringLiteral(
            "SELECT 1 FROM cache WHERE cache_key = ? AND (expiry IS NULL OR expiry > datetime('now')) LIMIT 1"));
        query.addBindValue(key);
        return query.exec() && query.next();
    }

    void rememberMiss(QSqlDatabase &library, const QString &key) {
        QSqlQuery query(library);
        query.prepare(QStringLiteral("INSERT OR REPLACE INTO cache (cache_key, cache_value, created_at, expiry) "
                                     "VALUES (?, 'miss', datetime('now'), datetime('now', '+30 days'))"));
        query.addBindValue(key);
        query.exec();
    }

    QSet<QString> existingAssetTypes(QSqlDatabase &catalog, const QString &gameId) {
        QSet<QString> types;
        QSqlQuery query(catalog);
        query.prepare(QStringLiteral("SELECT asset_type FROM game_assets WHERE game_id = ?"));
        query.addBindValue(gameId);
        if (!query.exec())
            return types;
        while (query.next())
            types.insert(query.value(0).toString());
        return types;
    }

    bool needsHasheousFields(const QString &coverUrl, const QString &description, const QString &developer,
        const QString &publisher, const QString &releaseDate) {
        return coverUrl.trimmed().isEmpty() || description.trimmed().isEmpty() || developer.trimmed().isEmpty()
            || publisher.trimmed().isEmpty() || releaseDate.trimmed().isEmpty();
    }

    bool needsDeepFields(const QString &genre, float rating, const QSet<QString> &assets) {
        return genre.trimmed().isEmpty() || rating <= 0.0f || !assets.contains(QStringLiteral("hero"))
            || !assets.contains(QStringLiteral("banner"));
    }

    bool needsLibretroAssets(const QSet<QString> &have) {
        for (const QString &type : libretroAssetTypes()) {
            if (!have.contains(type))
                return true;
        }
        return false;
    }

    bool defaultUrlExists(const QUrl &url) {
        HttpClient client;
        const HttpResponse response = client.head(url, Constants::Network::ARTWORK_PROBE_TIMEOUT_MS);
        return response.error.isEmpty() && response.statusCode >= 200 && response.statusCode < 400;
    }

    QString pickLibretroUrl(const QString &systemName, const QString &title, const QString &assetType,
        bool probe, const std::function<bool(const QUrl &)> &urlExists) {
        const QString folder = Metadata::ThumbnailUrlHelper::libretroFolderForAssetType(assetType);
        if (folder.isEmpty() || systemName.isEmpty() || title.isEmpty())
            return {};

        const QStringList urls
            = Metadata::ThumbnailUrlHelper::generateThumbnailCandidates(systemName, title, folder);
        if (urls.isEmpty())
            return {};
        if (!probe)
            return urls.first();

        for (const QString &url : urls) {
            if (urlExists(QUrl(url)))
                return url;
        }
        return {};
    }

    bool upsertAsset(QSqlDatabase &catalog, const QString &gameId, const QString &assetType, const QString &url,
        const QString &source) {
        QSqlQuery query(catalog);
        query.prepare(QStringLiteral(
            "INSERT INTO game_assets (game_id, asset_type, url, source, updated_at) "
            "VALUES (?, ?, ?, ?, datetime('now')) "
            "ON CONFLICT(game_id, asset_type) DO NOTHING"));
        query.addBindValue(gameId);
        query.addBindValue(assetType);
        query.addBindValue(url);
        query.addBindValue(source);
        return query.exec();
    }

    bool applyHasheousMetadata(QSqlDatabase &catalog, const QString &gameId, const GameMetadata &metadata) {
        QSqlQuery update(catalog);
        update.prepare(QStringLiteral(
            "UPDATE games SET "
            "canonical_title = COALESCE(NULLIF(?, ''), canonical_title), "
            "cover_url = COALESCE(NULLIF(cover_url, ''), ?), "
            "description = COALESCE(NULLIF(description, ''), ?), "
            "developer = COALESCE(NULLIF(developer, ''), ?), "
            "publisher = COALESCE(NULLIF(publisher, ''), ?), "
            "release_date = COALESCE(NULLIF(release_date, ''), ?), "
            "genre = COALESCE(NULLIF(?, ''), genre), "
            "rating = CASE WHEN ? > 0 THEN ? ELSE rating END, "
            "updated_at = datetime('now') "
            "WHERE game_id = ?"));
        update.addBindValue(metadata.title);
        update.addBindValue(metadata.boxArtUrl);
        update.addBindValue(metadata.description);
        update.addBindValue(metadata.developer);
        update.addBindValue(metadata.publisher);
        update.addBindValue(metadata.releaseDate);
        update.addBindValue(metadata.genres.join(QStringLiteral(", ")));
        update.addBindValue(metadata.rating);
        update.addBindValue(metadata.rating);
        update.addBindValue(gameId);
        return update.exec();
    }

    int applyIgdbAssets(QSqlDatabase &catalog, const QString &gameId, const GameMetadata &metadata,
        const QSet<QString> &existing) {
        int written = 0;
        auto upsert = [&](const QString &type, const QString &url, const QString &source) {
            if (url.isEmpty() || existing.contains(type))
                return;
            if (upsertAsset(catalog, gameId, type, url, source))
                ++written;
        };
        if (!metadata.screenshotUrls.isEmpty())
            upsert(QStringLiteral("snap"), metadata.screenshotUrls.first(), QStringLiteral("igdb"));
        upsert(QStringLiteral("hero"), metadata.externalIds.value(QStringLiteral("hero_url")), QStringLiteral("igdb"));
        upsert(
            QStringLiteral("banner"), metadata.externalIds.value(QStringLiteral("banner_url")), QStringLiteral("igdb"));
        return written;
    }

} // namespace

Result<EnrichStats> enrichLibrary(
    const QString &catalogDbPath, const QString &libraryDbPath, const EnrichOptions &options) {
    Database library;
    if (!library.initialize(libraryDbPath)) {
        return Result<EnrichStats>::fail(QStringLiteral("Failed to open library database"));
    }

    QSqlQuery owned(library.database());
    if (!owned.exec(
            QStringLiteral("SELECT DISTINCT catalog_game_id, crc32, md5, sha1 FROM files "
                           "WHERE catalog_game_id IS NOT NULL AND TRIM(catalog_game_id) != '' "
                           "AND id IN (SELECT file_id FROM matches WHERE is_confirmed = 1 AND is_rejected = 0)"))) {
        return Result<EnrichStats>::fail(owned.lastError().text());
    }

    struct OwnedGame {
        QString gameId;
        QString crc32;
        QString md5;
        QString sha1;
    };
    QList<OwnedGame> ownedGames;
    QSet<QString> seen;
    while (owned.next()) {
        OwnedGame game;
        game.gameId = owned.value(0).toString();
        if (seen.contains(game.gameId))
            continue;
        seen.insert(game.gameId);
        game.crc32 = owned.value(1).toString();
        game.md5 = owned.value(2).toString();
        game.sha1 = owned.value(3).toString();
        ownedGames.append(game);
    }

    auto catalogResult = catalog::open(catalogDbPath, QStringLiteral("library_enrich"));
    if (!catalogResult) {
        return Result<EnrichStats>::fail(catalogResult.error());
    }
    QSqlDatabase catalog = std::move(*std::move(catalogResult));
    if (!options.dryRun)
        CatalogSql::applyWritePragmas(catalog);
    else
        CatalogSql::applyReadOnlyPragmas(catalog);

    EnrichStats stats;
    stats.matchedGames = ownedGames.size();
    HasheousProvider provider;
    auto lookup = options.lookup ? options.lookup : [&](const QString &crc, const QString &md5, const QString &sha1) {
        return provider.lookupByHashes(crc, md5, sha1);
    };
    auto fetchIgdb = options.fetchIgdb ? options.fetchIgdb
                                       : [&](int igdbId) { return provider.fetchIgdbGame(igdbId); };
    auto urlExists = options.urlExists ? options.urlExists : defaultUrlExists;
    const bool deepEnabled = options.deep && provider.hasApiKey();
    bool onlineLookupUsed = false;

    for (const OwnedGame &ownedGame : ownedGames) {
        QSqlQuery gameQuery(catalog);
        gameQuery.prepare(QStringLiteral(
            "SELECT g.cover_url, g.description, g.canonical_title, g.developer, g.publisher, g.release_date, "
            "g.genre, g.rating, "
            "COALESCE(NULLIF(s.libretro_name, ''), s.display_name) "
            "FROM games g LEFT JOIN systems s ON s.system_id = g.system_id WHERE g.game_id = ?"));
        gameQuery.addBindValue(ownedGame.gameId);
        if (!gameQuery.exec() || !gameQuery.next())
            continue;

        QString coverUrl = gameQuery.value(0).toString();
        QString description = gameQuery.value(1).toString();
        const QString title = gameQuery.value(2).toString();
        QString developer = gameQuery.value(3).toString();
        QString publisher = gameQuery.value(4).toString();
        QString releaseDate = gameQuery.value(5).toString();
        const QString genre = gameQuery.value(6).toString();
        const float rating = gameQuery.value(7).toFloat();
        const QString systemName = gameQuery.value(8).toString();

        QSet<QString> assets = existingAssetTypes(catalog, ownedGame.gameId);
        if (!coverUrl.trimmed().isEmpty() && !assets.contains(QStringLiteral("boxart"))) {
            if (!options.dryRun) {
                if (upsertAsset(catalog, ownedGame.gameId, QStringLiteral("boxart"), coverUrl,
                        QStringLiteral("cover_url")))
                    ++stats.assetsWritten;
            }
            assets.insert(QStringLiteral("boxart"));
        }
        const QString thumbTitle = DiscSetUtils::extractBaseTitle(title);
        const QString artTitle = thumbTitle.isEmpty() ? title : thumbTitle;
        const bool probe = options.online && !options.dryRun;

        if (needsLibretroAssets(assets) && !systemName.isEmpty() && !artTitle.isEmpty()) {
            for (const QString &assetType : libretroAssetTypes()) {
                if (assets.contains(assetType))
                    continue;
                const QString url = pickLibretroUrl(systemName, artTitle, assetType, probe, urlExists);
                if (url.isEmpty())
                    continue;
                ++stats.thumbnailUrls;
                if (options.dryRun)
                    continue;
                if (!upsertAsset(catalog, ownedGame.gameId, assetType, url, QStringLiteral("libretro")))
                    continue;
                ++stats.assetsWritten;
                assets.insert(assetType);
                if (assetType == QStringLiteral("boxart") && coverUrl.trimmed().isEmpty()) {
                    QSqlQuery update(catalog);
                    update.prepare(QStringLiteral(
                        "UPDATE games SET cover_url = COALESCE(NULLIF(cover_url, ''), ?) WHERE game_id = ?"));
                    update.addBindValue(url);
                    update.addBindValue(ownedGame.gameId);
                    if (update.exec())
                        coverUrl = url;
                }
            }
        }

        const bool metadataComplete
            = !needsHasheousFields(coverUrl, description, developer, publisher, releaseDate);
        const bool deepComplete = !deepEnabled || !needsDeepFields(genre, rating, assets);
        const bool assetsComplete = !needsLibretroAssets(assets);
        if (metadataComplete && deepComplete && assetsComplete) {
            ++stats.complete;
            continue;
        }

        if (metadataComplete && deepComplete) {
            // Assets may still be incomplete after offline URL synthesis; no Hasheous needed.
            continue;
        }

        const bool needsLookup = !metadataComplete || (deepEnabled && !deepComplete);

        const QString key = cacheKeyForHashes(ownedGame.crc32, ownedGame.md5, ownedGame.sha1);
        if (isNegativeCached(library.database(), key)) {
            ++stats.negativeCached;
            continue;
        }

        ++stats.wouldFetch;
        if (options.dryRun || !options.online || !needsLookup)
            continue;

        if (onlineLookupUsed)
            QThread::msleep(Constants::Network::HASHEOUS_RATE_LIMIT_MS);
        onlineLookupUsed = true;

        GameMetadata metadata = lookup(ownedGame.crc32, ownedGame.md5, ownedGame.sha1);
        ++stats.fetched;
        if (metadata.boxArtUrl.isEmpty() && metadata.description.isEmpty() && metadata.title.isEmpty()
            && metadata.developer.isEmpty() && metadata.publisher.isEmpty() && metadata.releaseDate.isEmpty()
            && metadata.externalIds.value(QStringLiteral("igdb")).isEmpty()) {
            rememberMiss(library.database(), key);
            ++stats.negativeCached;
            continue;
        }

        if (deepEnabled) {
            const QString igdbId = metadata.externalIds.value(QStringLiteral("igdb"));
            bool ok = false;
            const int igdbNumeric = igdbId.toInt(&ok);
            if (ok && igdbNumeric > 0) {
                if (onlineLookupUsed)
                    QThread::msleep(Constants::Network::HASHEOUS_RATE_LIMIT_MS);
                onlineLookupUsed = true;
                const GameMetadata proxy = fetchIgdb(igdbNumeric);
                if (!proxy.title.isEmpty() || !proxy.genres.isEmpty() || proxy.rating > 0.0f
                    || !proxy.screenshotUrls.isEmpty()
                    || proxy.externalIds.contains(QStringLiteral("hero_url"))
                    || proxy.externalIds.contains(QStringLiteral("banner_url"))) {
                    metadata = HasheousProvider::mergeMetadata(metadata, proxy);
                    ++stats.proxyFetched;
                }
            }
        }

        if (!applyHasheousMetadata(catalog, ownedGame.gameId, metadata)) {
            const QString conn = catalog.connectionName();
            catalog.close();
            QSqlDatabase::removeDatabase(conn);
            return Result<EnrichStats>::fail(QStringLiteral("Failed to apply Hasheous metadata"));
        }

        if (!metadata.boxArtUrl.isEmpty() && !assets.contains(QStringLiteral("boxart"))) {
            if (upsertAsset(
                    catalog, ownedGame.gameId, QStringLiteral("boxart"), metadata.boxArtUrl, QStringLiteral("hasheous")))
                ++stats.assetsWritten;
        }
        stats.assetsWritten += applyIgdbAssets(catalog, ownedGame.gameId, metadata, assets);
        ++stats.updated;
    }

    const QString conn = catalog.connectionName();
    catalog.close();
    QSqlDatabase::removeDatabase(conn);
    return Result<EnrichStats>::ok(stats);
}

} // namespace remustwo
