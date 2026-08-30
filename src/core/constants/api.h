#pragma once

/**
 * @file api.h
 * @brief Application version and live service URLs
 */

#include <QString>

namespace remustwo {
namespace Constants {

    inline constexpr const char *APP_VERSION = "0.5.0";

    namespace API {

        inline constexpr const char *HASHEOUS_BASE_URL = "https://hasheous.org";
        inline constexpr const char *HASHEOUS_LOOKUP_ENDPOINT = "/api/v1/Lookup/ByHash";

        inline const QString USER_AGENT = QString("remustwo/%1 (ROM Library Manager)").arg(APP_VERSION);

    } // namespace API
} // namespace Constants
} // namespace remustwo
