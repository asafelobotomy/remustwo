#include "constants/systems.h"
#include "systems_registry.h"

namespace remustwo {
namespace Constants {
    namespace Systems {

        const QMap<int, SystemDef> &systemsRegistry() {
            return SystemsSeed::registry();
        }

    } // namespace Systems
} // namespace Constants
} // namespace remustwo
