#include "artwork_cache.h"

#include "../catalog/catalog.h"
#include "../catalog/sql_pragmas.h"
#include "http_client.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>

namespace remustwo {

QString artworkCacheDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString dir = base + QStringLiteral("/artwork");
    QDir().mkpath(dir);
    return dir;
}

Result<QString> cacheArtwork(const QUrl &url, const QString &gameId, bool online) {
    const QString suffix
        = QFileInfo(url.path()).suffix().isEmpty() ? QStringLiteral("jpg") : QFileInfo(url.path()).suffix();
    const QString destPath = artworkCacheDir() + QLatin1Char('/') + gameId + QLatin1Char('.') + suffix;

    if (QFile::exists(destPath)) {
        return Result<QString>::ok(destPath);
    }
    if (!online) {
        return Result<QString>::fail(QStringLiteral("Artwork cache requires --online"));
    }
    if (!url.isValid() || url.scheme() != QStringLiteral("https")) {
        return Result<QString>::fail(QStringLiteral("Unsupported artwork URL"));
    }

    HttpClient client;
    const HttpResponse response = client.get(url);
    if (!response.error.isEmpty()) {
        return Result<QString>::fail(response.error);
    }
    if (response.body.isEmpty()) {
        return Result<QString>::fail(QStringLiteral("Empty artwork response"));
    }

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return Result<QString>::fail(file.errorString());
    }
    if (file.write(response.body) != response.body.size()) {
        return Result<QString>::fail(QStringLiteral("Failed to write artwork file"));
    }
    file.close();
    return Result<QString>::ok(destPath);
}

QString cachedArtworkPath(const QString &gameId) {
    if (gameId.trimmed().isEmpty())
        return {};
    const QDir dir(artworkCacheDir());
    const QStringList matches = dir.entryList(QStringList { gameId + QStringLiteral(".*") }, QDir::Files);
    if (matches.isEmpty())
        return {};
    return dir.absoluteFilePath(matches.first());
}

QString resolveArtworkPath(const QString &catalogDbPath, const QString &gameId, bool online) {
    const QString existing = cachedArtworkPath(gameId);
    if (!existing.isEmpty())
        return existing;
    if (catalogDbPath.trimmed().isEmpty() || gameId.trimmed().isEmpty())
        return {};

    const QString connectionName
        = QStringLiteral("artwork_cache_%1").arg(QDateTime::currentMSecsSinceEpoch());
    auto catalogResult = catalog::open(catalogDbPath, connectionName);
    if (!catalogResult)
        return {};
    QSqlDatabase catalog = std::move(*catalogResult);
    CatalogSql::applyReadOnlyPragmas(catalog);

    QSqlQuery query(catalog);
    query.prepare(QStringLiteral("SELECT cover_url FROM games WHERE game_id = ?"));
    query.addBindValue(gameId);
    QString coverUrl;
    if (query.exec() && query.next())
        coverUrl = query.value(0).toString().trimmed();

    catalog.close();
    QSqlDatabase::removeDatabase(connectionName);

    if (coverUrl.isEmpty())
        return {};
    const Result<QString> cached = cacheArtwork(QUrl(coverUrl), gameId, online);
    return cached ? cached.value() : QString();
}

} // namespace remustwo
