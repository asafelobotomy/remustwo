#pragma once

#include "../core/result.h"

#include <QUrl>

namespace remustwo {

QString artworkCacheDir();
Result<QString> cacheArtwork(const QUrl &url, const QString &gameId, bool online);
QString cachedArtworkPath(const QString &gameId);
QString resolveArtworkPath(const QString &catalogDbPath, const QString &gameId, bool online);

} // namespace remustwo
