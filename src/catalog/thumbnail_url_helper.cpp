#include "thumbnail_url_helper.h"

#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace remustwo {
namespace Metadata {

QString ThumbnailUrlHelper::sanitizeThumbnailName(const QString &name) {
    QString sanitized = name;
    static const QString invalidChars = QStringLiteral("&*/:\\<>?|\"");
    for (QChar ch : invalidChars) {
        sanitized.replace(ch, QLatin1Char('_'));
    }
    return sanitized;
}

QString ThumbnailUrlHelper::stripLanguageTags(const QString &name) {
    static const QRegularExpression langTagRe(
        QStringLiteral("\\s*\\(\\s*(?:[A-Z][a-z])(?:,\\s*[A-Z][a-z])*\\s*\\)"));

    QString result = name;
    result.remove(langTagRe);
    return result.trimmed();
}

QString ThumbnailUrlHelper::buildThumbnailUrl(
    const QString &systemName, const QString &gameName, const QString &type) {
    const QString sanitized = sanitizeThumbnailName(gameName);
    const QString path
        = systemName + QLatin1Char('/') + type + QLatin1Char('/') + sanitized + QStringLiteral(".png");

    QUrl url;
    url.setScheme(QStringLiteral("https"));
    url.setHost(QStringLiteral("thumbnails.libretro.com"));
    url.setPath(QLatin1Char('/') + path, QUrl::DecodedMode);
    return url.toString(QUrl::FullyEncoded);
}

QStringList ThumbnailUrlHelper::generateThumbnailCandidates(
    const QString &systemName, const QString &gameName, const QString &type) {
    QStringList candidates;
    QSet<QString> seen;

    auto addCandidate = [&](const QString &candidateName) {
        const QString url = buildThumbnailUrl(systemName, candidateName, type);
        if (!seen.contains(url)) {
            seen.insert(url);
            candidates.append(url);
        }
    };

    addCandidate(gameName);

    const QString stripped = stripLanguageTags(gameName);
    if (stripped != gameName)
        addCandidate(stripped);

    return candidates;
}

QString ThumbnailUrlHelper::libretroFolderForAssetType(const QString &assetType) {
    if (assetType == QStringLiteral("box"))
        return QStringLiteral("Named_Boxarts");
    if (assetType == QStringLiteral("snap"))
        return QStringLiteral("Named_Snaps");
    if (assetType == QStringLiteral("title"))
        return QStringLiteral("Named_Titles");
    if (assetType == QStringLiteral("logo"))
        return QStringLiteral("Named_Logos");
    return { };
}

} // namespace Metadata
} // namespace remustwo
