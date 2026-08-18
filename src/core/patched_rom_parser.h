#pragma once

#include <QString>

#include "constants/file_types.h"

namespace remustwo {

struct PatchedRomInfo {
    QString rawName;
    QString baseTitle;
    QString patchName;
    QString fileType = Constants::FileTypes::OFFICIAL;
    bool isPatched = false;
};

class PatchedRomParser {
public:
    static PatchedRomInfo parse(const QString &nameOrPath);

private:
    PatchedRomParser() = delete;
};

} // namespace remustwo