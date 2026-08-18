#pragma once

#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

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

struct ArtworkUrls {
    QUrl boxFront;
    QUrl boxBack;
    QUrl boxFull;
    QUrl screenshot;
    QUrl screenshot2;
    QUrl titleScreen;
    QUrl banner;
    QUrl logo;
    QUrl clearLogo;
    QUrl systemLogo;
};

struct SearchResult {
    QString id;
    QString title;
    QString system;
    QString region;
    int releaseYear = 0;
    float matchScore = 0.0f;
    QString provider;
};

class MetadataProvider : public QObject {
    Q_OBJECT

public:
    explicit MetadataProvider(QObject *parent = nullptr);
    ~MetadataProvider() override = default;

    virtual QString name() const = 0;
    virtual bool requiresAuth() const = 0;
    virtual void setCredentials(const QString &username, const QString &password);
    virtual QList<SearchResult> searchByName(
        const QString &title, const QString &system = QString(), const QString &region = QString()) = 0;
    virtual GameMetadata getByHash(const QString &hash, const QString &system) = 0;
    virtual GameMetadata getBySerial(const QString &serial, const QString &system);
    virtual GameMetadata getById(const QString &id) = 0;
    virtual ArtworkUrls getArtwork(const QString &id) = 0;
    virtual bool isAvailable();

signals:
    void searchCompleted(const QList<SearchResult> &results);
    void metadataFetched(const GameMetadata &metadata);
    void artworkFetched(const ArtworkUrls &artwork);
    void errorOccurred(const QString &error);
    void rateLimitReached();

protected:
    QString m_username;
    QString m_password;
    bool m_authenticated = false;
};

} // namespace remustwo
