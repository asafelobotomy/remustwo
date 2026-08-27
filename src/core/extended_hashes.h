#pragma once

#include "hasher.h"

namespace remustwo {

struct ExtendedHashInput {
    QString path;
    int systemId = 0;
    QString extension;
};

void populateExtendedHashes(HashResult &hashes, const ExtendedHashInput &input);

} // namespace remustwo
