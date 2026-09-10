#include "hasheous_provider.h"

#include "../catalog/catalog.h"
#include "../catalog/sql_pragmas.h"
#include "../core/constants/api.h"
#include "../core/constants/network.h"
#include "../core/constants/settings.h"
#include "hasheous_igdb.h"
#include "http_client.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>
#include <QUrlQuery>

namespace remustwo {

namespace {

    QString normalizedHash(const QString &hash) {
        return hash.trimmed().toLower();
    }

    QString resolveHasheousApiKey() {
        const QByteArray env = qgetenv("REMUSTWO_HASHEOUS_API_KEY");
        if (!env.isEmpty())
            return QString::fromUtf8(env);
        QSettings settings;
        return settings.value(QString::fromLatin1(Constants::Settings::Providers::HASHEOUS_CLIENT_API_KEY))
            .toString()
            .trimmed();
    }

    void upsertMetadataAssets(QSqlDatabase &database, const QString &gameId, const GameMetadata &metadata) {
        auto upsert = [&](const QString &type, const QString &url, const QString &source) {
            if (url.isEmpty())
                return;
            QSqlQuery asset(database);
            asset.prepare(QStringLiteral(
                "INSERT INTO game_assets (game_id, asset_type, url, source, updated_at) "
                "VALUES (?, ?, ?, ?, datetime('now')) "
                "ON CONFLICT(game_id, asset_type) DO NOTHING"));
            asset.addBindValue(gameId);
            asset.addBindValue(type);
            asset.addBindValue(url);
            asset.addBindValue(source);
            asset.exec();
        };
        upsert(QStringLiteral("boxart"), metadata.boxArtUrl, QStringLiteral("hasheous"));
        if (!metadata.screenshotUrls.isEmpty())
            upsert(QStringLiteral("snap"), metadata.screenshotUrls.first(), QStringLiteral("igdb"));
        upsert(QStringLiteral("hero"), metadata.externalIds.value(QStringLiteral("hero_url")), QStringLiteral("igdb"));
        upsert(QStringLiteral("banner"), metadata.externalIds.value(QStringLiteral("banner_url")), QStringLiteral("igdb"));
    }

    GameMetadata parseHasheousGame(const QJsonObject &obj) {
        GameMetadata metadata;
        metadata.id = obj.value(QStringLiteral("hasheousId")).toVariant().toString();
        if (metadata.id.isEmpty())
            metadata.id = obj.value(QStringLiteral("Id")).toVariant().toString();
        metadata.title = obj.value(QStringLiteral("name")).toString();
        if (metadata.title.isEmpty())
            metadata.title = obj.value(QStringLiteral("gameTitle")).toString();
        if (metadata.title.isEmpty())
            metadata.title = obj.value(QStringLiteral("Name")).toString();
        metadata.description = obj.value(QStringLiteral("description")).toString();
        if (metadata.description.isEmpty())
            metadata.description = obj.value(QStringLiteral("summary")).toString();
        metadata.boxArtUrl = obj.value(QStringLiteral("coverUrl")).toString();
        if (metadata.boxArtUrl.isEmpty())
            metadata.boxArtUrl = obj.value(QStringLiteral("cover_url")).toString();
        metadata.providerId = QStringLiteral("hasheous");
        metadata.matchMethod = QStringLiteral("hash");
        metadata.matchScore = 1.0f;

        const QString igdbId = obj.value(QStringLiteral("igdb_id")).toVariant().toString();
        if (!igdbId.isEmpty())
            metadata.externalIds.insert(QStringLiteral("igdb"), igdbId);

        const QJsonObject igdb = obj.value(QStringLiteral("igdb")).toObject();
        if (!igdb.isEmpty())
            metadata = HasheousProvider::mergeMetadata(metadata, parseIgdbGameObject(igdb));

        const QJsonArray metaList = obj.value(QStringLiteral("metadata")).toArray();
        for (const QJsonValue &entry : metaList) {
            if (!entry.isObject())
                continue;
            const QJsonObject meta = entry.toObject();
            if (meta.value(QStringLiteral("source")).toString().compare(QStringLiteral("IGDB"), Qt::CaseInsensitive) != 0)
                continue;
            const QString id = meta.value(QStringLiteral("id")).toVariant().toString();
            if (!id.isEmpty())
                metadata.externalIds.insert(QStringLiteral("igdb"), id);
            if (metadata.title.isEmpty())
                metadata.title = meta.value(QStringLiteral("name")).toString();
            if (metadata.description.isEmpty())
                metadata.description = meta.value(QStringLiteral("description")).toString();
            if (metadata.boxArtUrl.isEmpty())
                metadata.boxArtUrl = meta.value(QStringLiteral("coverUrl")).toString();
        }

        const QJsonObject metadataObj = obj.value(QStringLiteral("metadata")).toObject();
        if (!metadataObj.isEmpty()) {
            if (metadata.title.isEmpty())
                metadata.title = metadataObj.value(QStringLiteral("name")).toString();
            if (metadata.description.isEmpty())
                metadata.description = metadataObj.value(QStringLiteral("description")).toString();
            if (metadata.boxArtUrl.isEmpty())
                metadata.boxArtUrl = metadataObj.value(QStringLiteral("coverUrl")).toString();
        }
        return metadata;
    }

    Result<bool> updateGameMetadata(
        QSqlDatabase &database, const QString &hashType, const QString &hashValue, const GameMetadata &metadata) {
        QSqlQuery find(database);
        find.prepare(QStringLiteral("SELECT g.game_id FROM games g "
                                    "JOIN game_signatures gs ON gs.game_id = g.game_id "
                                    "WHERE gs.hash_type = ? AND lower(gs.hash_value) = ? LIMIT 1"));
        find.addBindValue(hashType);
        find.addBindValue(normalizedHash(hashValue));
        if (!find.exec()) {
            return Result<bool>::fail(find.lastError().text());
        }
        if (!find.next()) {
            return Result<bool>::ok(false);
        }

        const QString gameId = find.value(0).toString();
        QSqlQuery update(database);
        update.prepare(QStringLiteral(
            "UPDATE games SET canonical_title = COALESCE(NULLIF(?, ''), canonical_title), "
            "cover_url = COALESCE(NULLIF(?, ''), cover_url), "
            "description = COALESCE(NULLIF(?, ''), description), "
            "developer = COALESCE(NULLIF(?, ''), developer), "
            "publisher = COALESCE(NULLIF(?, ''), publisher), "
            "release_date = COALESCE(NULLIF(?, ''), release_date), "
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
        if (!update.exec()) {
            return Result<bool>::fail(update.lastError().text());
        }

        upsertMetadataAssets(database, gameId, metadata);
        return Result<bool>::ok(true);
    }

} // namespace

HasheousProvider::HasheousProvider(const QString &baseUrl)
    : m_apiKey(resolveHasheousApiKey()) {
    if (!baseUrl.isEmpty()) {
        m_baseUrl = baseUrl;
        return;
    }
    QSettings settings;
    const QString configured = settings.value(QString::fromLatin1(Constants::Settings::Providers::HASHEOUS_BASE_URL))
                                   .toString()
                                   .trimmed();
    m_baseUrl = configured.isEmpty() ? QString::fromLatin1(Constants::API::HASHEOUS_BASE_URL) : configured;
}

bool HasheousProvider::hasApiKey() const {
    return !m_apiKey.isEmpty();
}

GameMetadata HasheousProvider::mergeMetadata(const GameMetadata &base, const GameMetadata &overlay) {
    GameMetadata merged = base;
    auto fill = [](QString &dest, const QString &src) {
        if (dest.trimmed().isEmpty() && !src.trimmed().isEmpty())
            dest = src;
    };

    fill(merged.title, overlay.title);
    fill(merged.description, overlay.description);
    fill(merged.developer, overlay.developer);
    fill(merged.publisher, overlay.publisher);
    fill(merged.releaseDate, overlay.releaseDate);
    fill(merged.boxArtUrl, overlay.boxArtUrl);

    if (merged.genres.isEmpty())
        merged.genres = overlay.genres;
    if (merged.rating <= 0.0f && overlay.rating > 0.0f) {
        merged.rating = overlay.rating;
        merged.ratingSource = overlay.ratingSource;
    }
    if (merged.screenshotUrls.isEmpty())
        merged.screenshotUrls = overlay.screenshotUrls;

    for (auto it = overlay.externalIds.constBegin(); it != overlay.externalIds.constEnd(); ++it) {
        if (!merged.externalIds.contains(it.key()) || merged.externalIds.value(it.key()).isEmpty())
            merged.externalIds.insert(it.key(), it.value());
    }
    return merged;
}

QByteArray HasheousProvider::lookupJson(const QString &crc32, const QString &md5, const QString &sha1) {
    QJsonArray payload;
    QJsonObject entry;
    if (!crc32.isEmpty())
        entry.insert(QStringLiteral("crc"), normalizedHash(crc32));
    if (!md5.isEmpty())
        entry.insert(QStringLiteral("mD5"), normalizedHash(md5));
    if (!sha1.isEmpty())
        entry.insert(QStringLiteral("shA1"), normalizedHash(sha1));
    if (entry.isEmpty())
        return {};
    payload.append(entry);
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

GameMetadata HasheousProvider::lookupByHashes(const QString &crc32, const QString &md5, const QString &sha1) const {
    const QByteArray payload = lookupJson(crc32, md5, sha1);
    if (payload.isEmpty())
        return {};

    const QUrl url(m_baseUrl + QString::fromLatin1(Constants::API::HASHEOUS_LOOKUP_ENDPOINT));
    HttpClient client;
    const HttpResponse response = client.postJson(url, payload, Constants::Network::HASHEOUS_TIMEOUT_MS);
    if (!response.error.isEmpty() || response.body.isEmpty())
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(response.body);
    QJsonObject obj;
    if (doc.isArray() && !doc.array().isEmpty())
        obj = doc.array().first().toObject();
    else if (doc.isObject())
        obj = doc.object();
    if (obj.isEmpty())
        return {};
    return parseHasheousGame(obj);
}

GameMetadata HasheousProvider::fetchIgdbGame(int igdbId) const {
    if (igdbId <= 0 || m_apiKey.isEmpty())
        return {};

    QUrl url(m_baseUrl + QString::fromLatin1(Constants::API::HASHEOUS_IGDB_GAME_PROXY));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("Id"), QString::number(igdbId));
    query.addQueryItem(QStringLiteral("expandColumns"),
        QStringLiteral("cover,genres,involved_companies,screenshots,artworks"));
    url.setQuery(query);

    QHash<QString, QString> headers;
    headers.insert(QStringLiteral("X-Client-API-Key"), m_apiKey);

    HttpClient client;
    const HttpResponse response = client.get(url, Constants::Network::METADATA_TIMEOUT_MS, headers);
    if (!response.error.isEmpty() || response.body.isEmpty())
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(response.body);
    if (!doc.isObject())
        return {};
    return parseIgdbGameObject(doc.object());
}

Result<int> importHasheousJson(const QString &catalogDbPath, const QString &jsonPath) {
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return Result<int>::fail(file.errorString());
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray entries;
    if (doc.isArray()) {
        entries = doc.array();
    } else if (doc.isObject()) {
        entries = doc.object().value(QStringLiteral("games")).toArray();
    }
    if (entries.isEmpty()) {
        return Result<int>::fail(QStringLiteral("No Hasheous entries found in JSON"));
    }

    auto dbResult = catalog::open(catalogDbPath, QStringLiteral("hasheous_import"));
    if (!dbResult) {
        return Result<int>::fail(dbResult.error());
    }
    QSqlDatabase database = *dbResult;
    CatalogSql::applyWritePragmas(database);

    int updated = 0;
    for (const QJsonValue &value : entries) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        const GameMetadata metadata = parseHasheousGame(obj);

        const QString md5 = obj.value(QStringLiteral("md5")).toString();
        const QString sha1 = obj.value(QStringLiteral("sha1")).toString();
        const QString crc = obj.value(QStringLiteral("crc32")).toString();
        if (!md5.isEmpty()) {
            auto result = updateGameMetadata(database, QStringLiteral("md5"), md5, metadata);
            if (!result)
                return Result<int>::fail(result.error());
            if (*result)
                ++updated;
            continue;
        }
        if (!sha1.isEmpty()) {
            auto result = updateGameMetadata(database, QStringLiteral("sha1"), sha1, metadata);
            if (!result)
                return Result<int>::fail(result.error());
            if (*result)
                ++updated;
            continue;
        }
        if (!crc.isEmpty()) {
            auto result = updateGameMetadata(database, QStringLiteral("crc32"), crc, metadata);
            if (!result)
                return Result<int>::fail(result.error());
            if (*result)
                ++updated;
        }
    }

    database.close();
    QSqlDatabase::removeDatabase(database.connectionName());
    return Result<int>::ok(updated);
}

} // namespace remustwo
