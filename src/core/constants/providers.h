#pragma once

#include <QString>
#include <QMap>
#include <QList>
#include <algorithm>
#include "settings.h"

namespace remustwo {
namespace Constants {
    namespace Providers {

        namespace Priority {
            inline constexpr int HASHEOUS = 91;
            inline constexpr int LIBRETRO_THUMBNAILS = 10;
        }

        inline constexpr const char *HASHEOUS = "hasheous";
        inline constexpr const char *LIBRETRO_THUMBNAILS = "libretro";

        // Provider IDs retained for system/API mapping only (no runtime provider yet).
        inline constexpr const char *RETROACHIEVEMENTS = "retroachievements";
        inline constexpr const char *THEGAMESDB = "thegamesdb";
        inline constexpr const char *SCREENSCRAPER = "screenscraper";
        inline constexpr const char *IGDB = "igdb";
        inline constexpr const char *WIKIDATA = "wikidata";

        inline const QString DISPLAY_HASHEOUS = QStringLiteral("Hasheous");
        inline const QString DISPLAY_LIBRETRO_THUMBNAILS = QStringLiteral("Libretro Thumbnails");

        struct ProviderInfo {
            QString id;
            QString displayName;
            QString description;
            bool supportsHashMatch = false;
            bool supportsNameMatch = false;
            bool requiresAuth = false;
            QString authHelpUrl;
            int priority = 0;
            bool isFreeService = true;
        };

        inline const QMap<QString, ProviderInfo> PROVIDER_REGISTRY = {
            { HASHEOUS,
                { HASHEOUS, DISPLAY_HASHEOUS,
                    QStringLiteral("Free hash database for cover URLs and descriptions (--online)"),
                    true, false, false, QStringLiteral(""), Priority::HASHEOUS, true } },
        };

        namespace SettingsKeys = Settings::Providers;

        inline const ProviderInfo *getProviderInfo(const QString &providerId) {
            auto it = PROVIDER_REGISTRY.find(providerId);
            if (it != PROVIDER_REGISTRY.end())
                return &it.value();
            return nullptr;
        }

        inline QStringList getProvidersByPriority() {
            QStringList providers;
            for (auto it = PROVIDER_REGISTRY.constBegin(); it != PROVIDER_REGISTRY.constEnd(); ++it)
                providers << it.key();
            std::sort(providers.begin(), providers.end(), [](const QString &a, const QString &b) {
                return PROVIDER_REGISTRY[a].priority > PROVIDER_REGISTRY[b].priority;
            });
            return providers;
        }

        inline QString getProviderDisplayName(const QString &providerId) {
            const ProviderInfo *info = getProviderInfo(providerId);
            return info ? info->displayName : QStringLiteral("Unknown");
        }

        inline QStringList getHashSupportingProviders() {
            QStringList providers;
            for (auto it = PROVIDER_REGISTRY.constBegin(); it != PROVIDER_REGISTRY.constEnd(); ++it) {
                if (it.value().supportsHashMatch)
                    providers << it.key();
            }
            return providers;
        }

        inline QStringList getNameSupportingProviders() {
            QStringList providers;
            for (auto it = PROVIDER_REGISTRY.constBegin(); it != PROVIDER_REGISTRY.constEnd(); ++it) {
                if (it.value().supportsNameMatch)
                    providers << it.key();
            }
            return providers;
        }

    } // Providers
} // Constants
} // namespace remustwo
