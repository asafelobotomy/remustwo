#pragma once

#include "providers.h"

#include <QMap>
#include <QSet>
#include <QString>

namespace remustwo {
namespace Constants {
    namespace ProviderFields {

        inline constexpr const char *TITLE = "title";
        inline constexpr const char *PUBLISHER = "publisher";
        inline constexpr const char *DEVELOPER = "developer";
        inline constexpr const char *RELEASE_DATE = "releaseDate";
        inline constexpr const char *GENRES = "genres";
        inline constexpr const char *PLAYERS = "players";
        inline constexpr const char *DESCRIPTION = "description";
        inline constexpr const char *BOX_ART_URL = "boxArtUrl";
        inline constexpr const char *RATING = "rating";
        inline constexpr const char *SCREENSHOTS = "screenshotUrls";
        inline constexpr const char *EXTERNAL_IDS = "externalIds";

        inline const QSet<QString> REQUIRED_FIELDS
            = { TITLE, PUBLISHER, DEVELOPER, RELEASE_DATE, GENRES, PLAYERS, DESCRIPTION, BOX_ART_URL };

        inline const QSet<QString> HASHEOUS_FIELDS = { TITLE, DESCRIPTION, BOX_ART_URL, EXTERNAL_IDS };

        inline const QMap<QString, QSet<QString>> CAPABILITIES = {
            { QStringLiteral("hasheous"), HASHEOUS_FIELDS },
        };

        inline bool providerSupportsMetadataLookup(const QString &providerId) {
            return CAPABILITIES.contains(providerId.toLower());
        }

        inline bool providerSupportsNameLookup(const QString &providerId) {
            const auto info = Constants::Providers::getProviderInfo(providerId.toLower());
            return info && info->supportsNameMatch;
        }

        inline bool providerSupportsArtworkLookup(const QString &providerId) {
            const QString key = providerId.toLower();
            return key == QStringLiteral("hasheous") || key == QStringLiteral("libretro");
        }

    } // namespace ProviderFields
} // namespace Constants
} // namespace remustwo
