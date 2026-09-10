#pragma once

#include "metadata_provider.h"

#include <QJsonObject>

namespace remustwo {

GameMetadata parseIgdbGameObject(const QJsonObject &igdb);

} // namespace remustwo
