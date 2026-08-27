#pragma once

#include "constants/systems.h"

#include <QMap>

namespace remustwo::SystemsSeed {

using SystemDef = Constants::Systems::SystemDef;

const QMap<int, SystemDef> &registry();

} // namespace remustwo::SystemsSeed
