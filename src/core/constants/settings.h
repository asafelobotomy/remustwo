#pragma once

#include <QString>
#include <array>
#include "templates.h"

namespace remustwo {
namespace Constants {
    namespace Settings {

        namespace Providers {
            inline constexpr const char *HASHEOUS_CLIENT_API_KEY = "hasheous/client_api_key";
            inline constexpr const char *HASHEOUS_BASE_URL = "hasheous/base_url";

            /// Secret-bearing settings keys (reset / export redaction).
            inline constexpr std::array<const char *, 1> ALL_SECRET_KEYS = { {
                HASHEOUS_CLIENT_API_KEY,
            } };
        }

        namespace Metadata {
            inline constexpr const char *PROVIDER_PRIORITY = "metadata/provider_priority";
            inline constexpr const char *PROVIDER_PRIORITY_ORDER = "metadata/provider_priority_order";
            inline constexpr const char *PROVIDERS_ENABLED = "metadata/providers_enabled";
        }

        namespace Organize {
            inline constexpr const char *NAMING_TEMPLATE = "organize/naming_template";
            inline constexpr const char *BY_SYSTEM = "organize/by_system";
            inline constexpr const char *FOLDER_SCHEME = "organize/folder_scheme";
            inline constexpr const char *PRESERVE_ORIGINALS = "organize/preserve_originals";
        }

        namespace MetadataRateLimit {
            inline constexpr const char *GLOBAL_MS = "metadata/rate_limit_ms";
            inline constexpr const char *HASHEOUS_MS = "metadata/rate_limit/hasheous";
        }

        namespace Match {
            inline constexpr const char *CONFIDENCE_THRESHOLD = "match/confidence_threshold";
        }

        namespace Performance {
            inline constexpr const char *HASH_ALGORITHM = "performance/hash_algorithm";
            inline constexpr const char *PARALLEL_HASHING = "performance/parallel_hashing";
        }

        namespace Defaults {
            inline const QString PROVIDER_PRIORITY = QStringLiteral("Hasheous");
            inline const QString NAMING_TEMPLATE = Templates::DEFAULT_SIMPLE;
            inline const QString HASH_ALGORITHM = QStringLiteral("Auto (System Default)");
            inline const QString ORGANIZE_BY_SYSTEM = QStringLiteral("true");
            inline const QString FOLDER_SCHEME = QStringLiteral("default");
            inline const QString PRESERVE_ORIGINALS = QStringLiteral("false");
            inline const QString PARALLEL_HASHING = QStringLiteral("true");
            inline const QString CONFIDENCE_THRESHOLD = QStringLiteral("75");
            inline const QString TEMPLATE_VARIABLE_HINT = Templates::VARIABLE_HINT;
        }

        namespace Files {
            /// Marker file placed inside an extracted/organised directory to signal it has been processed
            inline constexpr const char *MARKER_PROCESSED = ".remus.md";

            /// Marker file placed in a directory to instruct the scanner to skip it
            inline constexpr const char *MARKER_SKIP_SCAN = ".remusdir";

            /// Subdirectory name (relative to the app data path) where downloaded artwork is stored
            inline constexpr const char *ARTWORK_SUBDIR = "artwork";
        }

    } // Settings

    inline constexpr std::array<const char *, 1> ALL_PROVIDER_KEYS = {
        Settings::Providers::HASHEOUS_CLIENT_API_KEY,
    };

    struct ProviderSettingField {
        const char *key;
        const char *label;
        bool isPassword;
    };

    inline constexpr std::array<ProviderSettingField, 1> ALL_PROVIDER_FIELDS = { {
        { Settings::Providers::HASHEOUS_CLIENT_API_KEY, "Hasheous API Key", false },
    } };

} // Constants
} // namespace remustwo
