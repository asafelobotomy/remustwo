#pragma once

#include "../core/result.h"
#include "metadata_provider.h"

#include <QString>

namespace remustwo {

class HasheousProvider {
public:
    explicit HasheousProvider(const QString &baseUrl = QString());

    GameMetadata lookupByHashes(const QString &crc32, const QString &md5, const QString &sha1) const;

    static QByteArray lookupJson(const QString &crc32, const QString &md5, const QString &sha1);

private:
    QString m_baseUrl;
};

Result<int> importHasheousJson(const QString &catalogDbPath, const QString &jsonPath);

} // namespace remustwo
