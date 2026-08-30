#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>

namespace remustwo {

struct GameMetadata {
    QString id;
    QString title;
    QString system;
    QString region;
    QString publisher;
    QString developer;
    QStringList genres;
    QString releaseDate;
    QString description;
    int players = 0;
    float rating = 0.0f;
    QString ratingSource;

    QString boxArtUrl;
    QStringList screenshotUrls;

    QString series;
    QString ageRating;
    QStringList alternateTitles;

    QMap<QString, QString> externalIds;
    QStringList serials;

    QString providerId;
    QDateTime fetchedAt;

    float matchScore = 0.0f;
    QString matchMethod;

    QString setKey;
    int matchedDiscNumber = 0;
    int catalogDiscCount = 0;
};

} // namespace remustwo
