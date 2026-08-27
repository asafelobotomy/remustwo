#include "extended_hashes.h"

#include "chd_header.h"
#include "constants/files.h"
#include "ra_hasher.h"
#include "rvz_converter.h"

namespace remustwo {

void populateExtendedHashes(HashResult &hashes, const ExtendedHashInput &input) {
    if (!hashes.success || input.path.isEmpty())
        return;

    const QString ext = input.extension.startsWith(QLatin1Char('.'))
        ? input.extension.toLower()
        : QStringLiteral(".") + input.extension.toLower();

    if (ext == Constants::Files::CHD) {
        const ChdHeaderDigest digest = readChdHeaderDigest(input.path);
        if (digest.valid)
            hashes.chdSha1 = digest.sha1;
        return;
    }

    if (ext == Constants::Files::RVZ || ext == Constants::Files::GCZ) {
        RVZConverter converter;
        if (converter.isDolphinToolAvailable()) {
            const QString rvzSha1 = converter.discContentSha1(input.path);
            if (!rvzSha1.isEmpty())
                hashes.rvzSha1 = rvzSha1;
        }
        return;
    }

    if (input.systemId > 0 && RaHasher::hasRaMapping(input.systemId)) {
        const RaHasher::Result raResult = RaHasher::compute(input.path, input.systemId, ext);
        if (raResult.success)
            hashes.raMd5 = raResult.md5;
    }
}

} // namespace remustwo
