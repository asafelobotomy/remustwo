#include "hasheous_provider.h"

#include "../catalog/catalog.h"
#include "../catalog/sql_pragmas.h"
#include "../core/constants/api.h"
#include "../core/constants/network.h"
#include "http_client.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>

namespace remustwo {

namespace {

    QString normalizedHash(const QString &hash) {
        return hash.trimmed().toLower();
    }

    GameMetadata parseHasheousGame(const QJsonObject &obj) {
        GameMetadata metadata;
        metadata.id = obj.value(QStringLiteral("hasheousId")).toVariant().toString();
        metadata.title = obj.value(QStringLiteral("name")).toString();
        if (metadata.title.isEmpty())
            metadata.title = obj.value(QStringLiteral("gameTitle")).toString();
        metadata.description = obj.value(QStringLiteral("description")).toString();
        metadata.boxArtUrl = obj.value(QStringLiteral("coverUrl")).toString();
        if (metadata.boxArtUrl.isEmpty())
            metadata.boxArtUrl = obj.value(QStringLiteral("cover_url")).toString();
        metadata.providerId = QStringLiteral("hasheous");
        metadata.matchMethod = QStringLiteral("hash");
        metadata.matchScore = 1.0f;

        const QJsonObject igdb = obj.value(QStringLiteral("igdb")).toObject();
        if (!igdb.isEmpty()) {
            metadata.externalIds.insert(
                QStringLiteral("igdb"), igdb.value(QStringLiteral("id")).toVariant().toString());
            if (metadata.title.isEmpty())
                metadata.title = igdb.value(QStringLiteral("name")).toString();
        }
        return metadata;
    }

    Result<bool> updateGameCover(
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
        update.prepare(QStringLiteral("UPDATE games SET canonical_title = COALESCE(NULLIF(?, ''), canonical_title), "
                                      "cover_url = COALESCE(NULLIF(?, ''), cover_url), "
                                      "description = COALESCE(NULLIF(?, ''), description) "
                                      "WHERE game_id = ?"));
        update.addBindValue(metadata.title);
        update.addBindValue(metadata.boxArtUrl);
        update.addBindValue(metadata.description);
        update.addBindValue(gameId);
        if (!update.exec()) {
            return Result<bool>::fail(update.lastError().text());
        }
        return Result<bool>::ok(true);
    }

} // namespace

HasheousProvider::HasheousProvider(const QString &baseUrl)
    : m_baseUrl(baseUrl.isEmpty() ? QString::fromLatin1(Constants::API::HASHEOUS_BASE_URL) : baseUrl) { }

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
            auto result = updateGameCover(database, QStringLiteral("md5"), md5, metadata);
            if (!result)
                return Result<int>::fail(result.error());
            if (*result)
                ++updated;
            continue;
        }
        if (!sha1.isEmpty()) {
            auto result = updateGameCover(database, QStringLiteral("sha1"), sha1, metadata);
            if (!result)
                return Result<int>::fail(result.error());
            if (*result)
                ++updated;
            continue;
        }
        if (!crc.isEmpty()) {
            auto result = updateGameCover(database, QStringLiteral("crc32"), crc, metadata);
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
