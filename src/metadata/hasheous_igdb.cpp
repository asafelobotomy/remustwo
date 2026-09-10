#include "hasheous_igdb.h"

#include <QDateTime>
#include <QJsonArray>

namespace remustwo {

namespace {

    QString normalizeIgdbImageUrl(QString url) {
        url = url.trimmed();
        if (url.startsWith(QStringLiteral("//")))
            url = QStringLiteral("https:") + url;
        url.replace(QStringLiteral("t_thumb"), QStringLiteral("t_1080p"));
        return url;
    }

    QString readImageUrl(const QJsonValue &value) {
        if (value.isString())
            return normalizeIgdbImageUrl(value.toString());
        if (value.isObject())
            return normalizeIgdbImageUrl(value.toObject().value(QStringLiteral("url")).toString());
        return {};
    }

    QStringList collectNamedUrls(const QJsonValue &value) {
        QStringList urls;
        if (value.isArray()) {
            for (const QJsonValue &entry : value.toArray()) {
                const QString url = readImageUrl(entry);
                if (!url.isEmpty())
                    urls.append(url);
            }
            return urls;
        }
        if (value.isObject()) {
            const QJsonObject obj = value.toObject();
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                const QString url = readImageUrl(it.value());
                if (!url.isEmpty())
                    urls.append(url);
            }
        }
        return urls;
    }

    QString pickArtworkUrl(const QJsonValue &value, int artworkType) {
        if (!value.isObject())
            return {};
        const QJsonObject obj = value.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            const QJsonObject art = it.value().toObject();
            if (art.value(QStringLiteral("artwork_type")).toInt(-1) != artworkType)
                continue;
            const QString url = readImageUrl(art);
            if (!url.isEmpty())
                return url;
        }
        return {};
    }

    QString formatIgdbReleaseDate(const QJsonValue &value) {
        if (value.isString()) {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty())
                return text;
        }
        const qint64 epoch = value.toVariant().toLongLong();
        if (epoch <= 0)
            return {};
        return QDateTime::fromSecsSinceEpoch(epoch).toString(Qt::ISODate);
    }

} // namespace

GameMetadata parseIgdbGameObject(const QJsonObject &igdb) {
    GameMetadata metadata;
    metadata.providerId = QStringLiteral("hasheous");
    metadata.matchMethod = QStringLiteral("igdb_proxy");
    metadata.externalIds.insert(QStringLiteral("igdb"), igdb.value(QStringLiteral("id")).toVariant().toString());

    metadata.title = igdb.value(QStringLiteral("name")).toString();
    metadata.description = igdb.value(QStringLiteral("summary")).toString();
    if (metadata.description.isEmpty())
        metadata.description = igdb.value(QStringLiteral("storyline")).toString();

    metadata.boxArtUrl = readImageUrl(igdb.value(QStringLiteral("cover")));
    metadata.releaseDate = formatIgdbReleaseDate(igdb.value(QStringLiteral("first_release_date")));

    const QJsonValue genres = igdb.value(QStringLiteral("genres"));
    if (genres.isObject()) {
        for (auto it = genres.toObject().constBegin(); it != genres.toObject().constEnd(); ++it) {
            const QString name = it.value().toObject().value(QStringLiteral("name")).toString();
            if (!name.isEmpty())
                metadata.genres.append(name);
        }
    } else if (genres.isArray()) {
        for (const QJsonValue &entry : genres.toArray()) {
            const QString name = entry.toObject().value(QStringLiteral("name")).toString();
            if (!name.isEmpty())
                metadata.genres.append(name);
        }
    }

    const QJsonValue companies = igdb.value(QStringLiteral("involved_companies"));
    if (companies.isObject()) {
        for (auto it = companies.toObject().constBegin(); it != companies.toObject().constEnd(); ++it) {
            const QJsonObject row = it.value().toObject();
            const QString name = row.value(QStringLiteral("company")).toObject().value(QStringLiteral("name")).toString();
            if (name.isEmpty())
                continue;
            if (row.value(QStringLiteral("developer")).toBool() && metadata.developer.isEmpty())
                metadata.developer = name;
            if (row.value(QStringLiteral("publisher")).toBool() && metadata.publisher.isEmpty())
                metadata.publisher = name;
        }
    }
    if (metadata.developer.isEmpty())
        metadata.developer = igdb.value(QStringLiteral("developer")).toString();
    if (metadata.publisher.isEmpty())
        metadata.publisher = igdb.value(QStringLiteral("publisher")).toString();

    const double totalRating = igdb.value(QStringLiteral("total_rating")).toDouble(0.0);
    if (totalRating > 0.0) {
        metadata.rating = static_cast<float>(totalRating > 10.0 ? totalRating / 10.0 : totalRating);
        metadata.ratingSource = QStringLiteral("igdb");
    }

    metadata.screenshotUrls = collectNamedUrls(igdb.value(QStringLiteral("screenshots")));

    const QString hero = pickArtworkUrl(igdb.value(QStringLiteral("artworks")), 4);
    const QString banner = pickArtworkUrl(igdb.value(QStringLiteral("artworks")), 3);
    if (!hero.isEmpty())
        metadata.externalIds.insert(QStringLiteral("hero_url"), hero);
    if (!banner.isEmpty())
        metadata.externalIds.insert(QStringLiteral("banner_url"), banner);

    return metadata;
}

} // namespace remustwo
