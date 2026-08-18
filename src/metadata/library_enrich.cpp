#include "library_enrich.h"

#include "../catalog/catalog.h"
#include "../catalog/sql_pragmas.h"
#include "../catalog/thumbnail_url_helper.h"
#include "../core/database.h"
#include "hasheous_provider.h"

#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>

namespace remustwo {

namespace {

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

    bool needsEnrichment(const QString &coverUrl, const QString &description) {
        return coverUrl.trimmed().isEmpty() || description.trimmed().isEmpty();
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

    for (const OwnedGame &ownedGame : ownedGames) {
        QSqlQuery gameQuery(catalog);
        gameQuery.prepare(
            QStringLiteral("SELECT g.cover_url, g.description, g.canonical_title, s.display_name "
                           "FROM games g LEFT JOIN systems s ON s.system_id = g.system_id WHERE g.game_id = ?"));
        gameQuery.addBindValue(ownedGame.gameId);
        if (!gameQuery.exec() || !gameQuery.next())
            continue;

        QString coverUrl = gameQuery.value(0).toString();
        QString description = gameQuery.value(1).toString();
        const QString title = gameQuery.value(2).toString();
        const QString systemName = gameQuery.value(3).toString();

        if (!needsEnrichment(coverUrl, description)) {
            ++stats.complete;
            continue;
        }

        if (coverUrl.trimmed().isEmpty() && !systemName.isEmpty() && !title.isEmpty()) {
            const QStringList urls = Metadata::ThumbnailUrlHelper::generateThumbnailCandidates(
                systemName, title, QStringLiteral("Named_Boxarts"));
            if (!urls.isEmpty()) {
                coverUrl = urls.first();
                ++stats.thumbnailUrls;
                if (!options.dryRun) {
                    QSqlQuery update(catalog);
                    update.prepare(QStringLiteral(
                        "UPDATE games SET cover_url = COALESCE(NULLIF(cover_url, ''), ?) WHERE game_id = ?"));
                    update.addBindValue(coverUrl);
                    update.addBindValue(ownedGame.gameId);
                    update.exec();
                }
            }
        }

        if (!needsEnrichment(coverUrl, description)) {
            ++stats.complete;
            continue;
        }

        const QString key = cacheKeyForHashes(ownedGame.crc32, ownedGame.md5, ownedGame.sha1);
        if (isNegativeCached(library.database(), key)) {
            ++stats.negativeCached;
            continue;
        }

        ++stats.wouldFetch;
        if (options.dryRun || !options.online)
            continue;

        const GameMetadata metadata = lookup(ownedGame.crc32, ownedGame.md5, ownedGame.sha1);
        ++stats.fetched;
        if (metadata.boxArtUrl.isEmpty() && metadata.description.isEmpty() && metadata.title.isEmpty()) {
            rememberMiss(library.database(), key);
            ++stats.negativeCached;
            continue;
        }

        QSqlQuery update(catalog);
        update.prepare(QStringLiteral("UPDATE games SET "
                                      "cover_url = COALESCE(NULLIF(cover_url, ''), ?), "
                                      "description = COALESCE(NULLIF(description, ''), ?) "
                                      "WHERE game_id = ?"));
        update.addBindValue(metadata.boxArtUrl);
        update.addBindValue(metadata.description);
        update.addBindValue(ownedGame.gameId);
        if (!update.exec()) {
            const QString conn = catalog.connectionName();
            catalog.close();
            QSqlDatabase::removeDatabase(conn);
            return Result<EnrichStats>::fail(update.lastError().text());
        }
        if (update.numRowsAffected() > 0)
            ++stats.updated;
    }

    const QString conn = catalog.connectionName();
    catalog.close();
    QSqlDatabase::removeDatabase(conn);
    return Result<EnrichStats>::ok(stats);
}

} // namespace remustwo
