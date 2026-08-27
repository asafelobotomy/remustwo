#pragma once

#include <QStringList>

namespace remustwo {
namespace Metadata {

/**
 * @brief Static helpers for building libretro thumbnail CDN URLs.
 *
 * URL format: https://thumbnails.libretro.com/{System}/{Type}/{Name}.png
 */
class ThumbnailUrlHelper {
public:
    ThumbnailUrlHelper() = delete;

    static QString sanitizeThumbnailName(const QString &name);
    static QString stripLanguageTags(const QString &name);
    static QString buildThumbnailUrl(const QString &systemName, const QString &gameName, const QString &type);
    static QStringList generateThumbnailCandidates(
        const QString &systemName, const QString &gameName, const QString &type);
    static QString libretroFolderForAssetType(const QString &assetType);
};

} // namespace Metadata
} // namespace remustwo
