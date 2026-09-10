#pragma once

#include <QString>
#include "templates.h"

namespace remustwo {
namespace Constants {
    namespace Settings {

        namespace Providers {
            /// QSettings key for optional Hasheous API base URL override.
            inline constexpr const char *HASHEOUS_BASE_URL = "hasheous/base_url";
            /// Hasheous client API key (MetadataProxy / fix-match endpoints only).
            inline constexpr const char *HASHEOUS_CLIENT_API_KEY = "hasheous/client_api_key";
        }

        namespace Organize {
            inline constexpr const char *NAMING_TEMPLATE = "organize/naming_template";
        }

        namespace Defaults {
            inline const QString PROVIDER_PRIORITY = QStringLiteral("Hasheous");
            inline const QString NAMING_TEMPLATE = Templates::DEFAULT_SIMPLE;
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

} // Constants
} // namespace remustwo
