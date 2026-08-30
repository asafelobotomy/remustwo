#include "system_resolver.h"
#include "constants/providers.h"

namespace remustwo {

using namespace Constants::Providers;
using namespace Constants::Systems;

QMap<int, QMap<QString, QString>> SystemResolver::providerMappings() {
    // RetroAchievements console IDs from API_GetConsoleIDs.php
    return {
        { ID_NES, { { RETROACHIEVEMENTS, QStringLiteral("7") } } },
        { ID_SNES, { { RETROACHIEVEMENTS, QStringLiteral("3") } } },
        { ID_N64, { { RETROACHIEVEMENTS, QStringLiteral("2") } } },
        { ID_GAMECUBE, { { RETROACHIEVEMENTS, QStringLiteral("16") } } },
        { ID_WII, { { RETROACHIEVEMENTS, QStringLiteral("20") } } },
        { ID_GB, { { RETROACHIEVEMENTS, QStringLiteral("4") } } },
        { ID_GBC, { { RETROACHIEVEMENTS, QStringLiteral("6") } } },
        { ID_GBA, { { RETROACHIEVEMENTS, QStringLiteral("5") } } },
        { ID_NDS, { { RETROACHIEVEMENTS, QStringLiteral("18") } } },
        { ID_3DS, { { RETROACHIEVEMENTS, QStringLiteral("62") } } },
        { ID_SWITCH, { { RETROACHIEVEMENTS, QStringLiteral("63") } } },
        { ID_VIRTUAL_BOY, { { RETROACHIEVEMENTS, QStringLiteral("28") } } },
        { ID_GENESIS, { { RETROACHIEVEMENTS, QStringLiteral("1") } } },
        { ID_MASTER_SYSTEM, { { RETROACHIEVEMENTS, QStringLiteral("11") } } },
        { ID_GAME_GEAR, { { RETROACHIEVEMENTS, QStringLiteral("15") } } },
        { ID_SATURN, { { RETROACHIEVEMENTS, QStringLiteral("39") } } },
        { ID_DREAMCAST, { { RETROACHIEVEMENTS, QStringLiteral("40") } } },
        { ID_SEGA_CD, { { RETROACHIEVEMENTS, QStringLiteral("9") } } },
        { ID_32X, { { RETROACHIEVEMENTS, QStringLiteral("10") } } },
        { ID_PSX, { { RETROACHIEVEMENTS, QStringLiteral("12") } } },
        { ID_PS2, { { RETROACHIEVEMENTS, QStringLiteral("21") } } },
        { ID_PSP, { { RETROACHIEVEMENTS, QStringLiteral("41") } } },
        { ID_ATARI_2600, { { RETROACHIEVEMENTS, QStringLiteral("25") } } },
        { ID_ATARI_7800, { { RETROACHIEVEMENTS, QStringLiteral("51") } } },
        { ID_LYNX, { { RETROACHIEVEMENTS, QStringLiteral("13") } } },
        { ID_ATARI_JAGUAR, { { RETROACHIEVEMENTS, QStringLiteral("17") } } },
        { ID_TURBOGRAFX16, { { RETROACHIEVEMENTS, QStringLiteral("8") } } },
        { ID_TURBOGRAFX_CD, { { RETROACHIEVEMENTS, QStringLiteral("76") } } },
        { ID_NGP, { { RETROACHIEVEMENTS, QStringLiteral("14") } } },
        { ID_WONDERSWAN, { { RETROACHIEVEMENTS, QStringLiteral("53") } } },
        { ID_PS3, { { RETROACHIEVEMENTS, QStringLiteral("43") } } },
        { ID_WIIU, { { RETROACHIEVEMENTS, QStringLiteral("38") } } },
    };
}

} // namespace remustwo
