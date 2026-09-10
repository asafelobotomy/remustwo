#pragma once

#include <QString>

#include "exports.h"

namespace remustwo {
namespace Constants {
    namespace Cli {

        inline constexpr const char *APPLICATION_NAME = "remustwo";

        namespace Defaults {
            inline constexpr const char *CHD_CODEC = "auto";
            inline constexpr const char *BUNDLE_FORMAT = "zip";
            inline constexpr const char *PATCH_FORMAT = "bps";
            inline const QString EXPORT_FORMAT = Exports::Formats::CSV;
        }

    } // Cli
} // Constants
} // namespace remustwo
