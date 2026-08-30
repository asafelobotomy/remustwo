#pragma once

#include <QString>
#include <QMap>
#include "constants/systems.h"

namespace remustwo {

/**
 * @brief Unified system name resolution for database, UI, and provider mappings
 *
 * Usage:
 *   - UI displays: SystemResolver::displayName(systemId)
 *   - Database queries: SystemResolver::internalName(systemId)
 *   - RetroAchievements: SystemResolver::providerName(systemId, "retroachievements")
 */
class SystemResolver {
public:
    static QString displayName(int systemId);
    static QString internalName(int systemId);

    /**
     * @brief Provider-specific platform identifier (currently RetroAchievements console IDs)
     */
    static QString providerName(int systemId, const QString &providerId);

    static int systemIdByName(const QString &internalName);
    static int systemIdByDatName(const QString &datName);
    static QString resolveSystemName(const QString &name);
    static bool isValidSystem(int systemId);

private:
    /// { systemId: { providerId: platformId } } — RetroAchievements only today
    static QMap<int, QMap<QString, QString>> providerMappings();
};

} // namespace remustwo
