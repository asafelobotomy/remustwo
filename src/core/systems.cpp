// System group lists and utility function implementations.
// Canonical system rows come from data/catalog/seeds/0002_systems.sql via systems_registry.cpp.
// Extension heuristics live in systems_extensions.cpp.
#include "constants/systems.h"
#include "systems_registry.h"

#include <QSet>

namespace remustwo {
namespace Constants {
    namespace Systems {

        namespace {

            QList<int> systemsMatching(bool (*predicate)(const SystemDef &)) {
                QList<int> ids;
                for (auto it = systemsRegistry().constBegin(); it != systemsRegistry().constEnd(); ++it) {
                    if (predicate(it.value()))
                        ids.append(it.key());
                }
                return ids;
            }

            bool isNintendo(const SystemDef &def) {
                return def.manufacturer == QStringLiteral("Nintendo");
            }

            bool isSega(const SystemDef &def) {
                return def.manufacturer == QStringLiteral("Sega") || def.manufacturer == QStringLiteral("Sammy");
            }

            bool isSony(const SystemDef &def) {
                return def.manufacturer == QStringLiteral("Sony");
            }

            bool isMicrosoft(const SystemDef &def) {
                return def.manufacturer == QStringLiteral("Microsoft");
            }

            bool isComputer(const SystemDef &def) {
                static const QSet<QString> computerNames = {
                    QStringLiteral("C64"), QStringLiteral("Amiga"), QStringLiteral("ZX Spectrum"),
                    QStringLiteral("Atari ST"), QStringLiteral("Atari 8-bit"), QStringLiteral("MSX"),
                    QStringLiteral("MSX2"), QStringLiteral("Amstrad CPC"), QStringLiteral("Enterprise 128"),
                    QStringLiteral("ZX 81"), QStringLiteral("Videoton TVC"), QStringLiteral("VIC-20"),
                    QStringLiteral("PC-98"), QStringLiteral("Sharp X1"), QStringLiteral("X68000"),
                    QStringLiteral("PC"), QStringLiteral("Mac"), QStringLiteral("FM Towns"),
                    QStringLiteral("PC-88"), QStringLiteral("Apple II"), QStringLiteral("BBC Micro"),
                    QStringLiteral("Commodore 16"),
                };
                return computerNames.contains(def.internalName);
            }

            bool isCartridge(const SystemDef &def) {
                return !def.isDiscBased && !isComputer(def);
            }

        } // namespace

        const QList<int> &nintendoSystems() {
            static const QList<int> cached = systemsMatching(isNintendo);
            return cached;
        }

        const QList<int> &segaSystems() {
            static const QList<int> cached = systemsMatching(isSega);
            return cached;
        }

        const QList<int> &sonySystems() {
            static const QList<int> cached = systemsMatching(isSony);
            return cached;
        }

        const QList<int> &microsoftSystems() {
            static const QList<int> cached = systemsMatching(isMicrosoft);
            return cached;
        }

        const QList<int> &handheldSystems() {
            static const QList<int> cached = systemsMatching([](const SystemDef &def) { return def.isHandheld; });
            return cached;
        }

        const QList<int> &discSystems() {
            static const QList<int> cached = systemsMatching([](const SystemDef &def) { return def.isDiscBased; });
            return cached;
        }

        const QList<int> &cartridgeSystems() {
            static const QList<int> cached = systemsMatching(isCartridge);
            return cached;
        }

        const QList<int> &computerSystems() {
            static const QList<int> cached = systemsMatching(isComputer);
            return cached;
        }

        const SystemDef *getSystem(int systemId) {
            const auto it = systemsRegistry().find(systemId);
            return (it != systemsRegistry().end()) ? &it.value() : nullptr;
        }

        int getSystemIdByName(const QString &name) {
            for (auto it = systemsRegistry().constBegin(); it != systemsRegistry().constEnd(); ++it) {
                if (it.value().internalName == name)
                    return it.key();
            }
            return 0;
        }

        const SystemDef *getSystemByName(const QString &name) {
            const int id = getSystemIdByName(name);
            return (id > 0) ? getSystem(id) : nullptr;
        }

        QStringList getSystemDisplayNames() {
            QStringList names;
            for (auto it = systemsRegistry().constBegin(); it != systemsRegistry().constEnd(); ++it)
                names << it.value().displayName;
            return names;
        }

        QStringList getSystemInternalNames() {
            QStringList names;
            for (auto it = systemsRegistry().constBegin(); it != systemsRegistry().constEnd(); ++it)
                names << it.value().internalName;
            return names;
        }

        QList<int> getSystemsForExtension(const QString &extension) {
            const auto it = EXTENSION_TO_SYSTEMS.find(extension.toLower());
            return (it != EXTENSION_TO_SYSTEMS.end()) ? it.value() : QList<int>();
        }

        bool isAmbiguousExtension(const QString &extension) {
            return getSystemsForExtension(extension).size() > 1;
        }

    } // namespace Systems
} // namespace Constants
} // namespace remustwo
