#pragma once

#include "../core/result.h"
#include "metadata_provider.h"

#include <QString>

namespace remustwo {

class HasheousProvider {
public:
    explicit HasheousProvider(const QString &baseUrl = QString());

    bool hasApiKey() const;

    GameMetadata lookupByHashes(const QString &crc32, const QString &md5, const QString &sha1) const;

    /// Expanded IGDB metadata via Hasheous MetadataProxy (requires client API key).
    GameMetadata fetchIgdbGame(int igdbId) const;

    /// Merge @p overlay onto @p base, keeping existing base values when overlay is empty.
    static GameMetadata mergeMetadata(const GameMetadata &base, const GameMetadata &overlay);

    static QByteArray lookupJson(const QString &crc32, const QString &md5, const QString &sha1);

private:
    QString m_baseUrl;
    QString m_apiKey;
};

Result<int> importHasheousJson(const QString &catalogDbPath, const QString &jsonPath);

} // namespace remustwo
