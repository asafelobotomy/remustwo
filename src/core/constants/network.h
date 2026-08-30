#pragma once

/**
 * @file network.h
 * @brief Timeouts and HTTP status constants used by live network code
 */

namespace remustwo {
namespace Constants {
    namespace Network {

        inline constexpr int HTTP_TIMEOUT_MS = 5000;
        inline constexpr int METADATA_TIMEOUT_MS = 10000;
        inline constexpr int ARTWORK_TIMEOUT_MS = 60000;
        inline constexpr int HASHEOUS_TIMEOUT_MS = 5000;
        inline constexpr int CONNECT_TIMEOUT_MS = 3000;

        inline constexpr int HASHEOUS_RATE_LIMIT_MS = 400;
        inline constexpr int DEFAULT_RATE_LIMIT_MS = 500;

        inline constexpr int MAX_RETRIES = 3;
        inline constexpr float RETRY_BACKOFF_MULTIPLIER = 1.5f;
        inline constexpr int INITIAL_RETRY_DELAY_MS = 500;
        inline constexpr int MAX_RETRY_DELAY_MS = 10000;

        inline constexpr int HTTP_UNAUTHORIZED = 401;
        inline constexpr int HTTP_FORBIDDEN = 403;
        inline constexpr int HTTP_TOO_MANY_REQUESTS = 429;
        inline constexpr int HTTP_NOT_FOUND = 404;
        inline constexpr int HTTP_SERVER_ERROR = 500;
        inline constexpr int HTTP_SERVICE_UNAVAILABLE = 503;

    } // namespace Network
} // namespace Constants
} // namespace remustwo
