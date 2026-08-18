#pragma once
// Phase 1 compendium compiler: system and region normalization.
// Resolves raw DAT system names and region strings into canonical IDs/codes
// aligned with the seeded compendium tables.

#include "compendium_types.h"
#include <QString>
#include <QMap>

namespace remustwo {
namespace Compendium {

    class CompendiumNormalizer {
    public:
        CompendiumNormalizer();

        // Resolve a raw DAT system name to a canonical system_id.
        // Returns 0 if the system name cannot be resolved.
        int resolveSystemId(const QString &rawSystemName) const;

        // Normalize a raw region string to a seeded region_code.
        // Returns empty string if the region cannot be resolved.
        QString resolveRegionCode(const QString &rawRegion) const;

        // Apply system and region normalization to a record in-place.
        void normalize(SourceRecordEnvelope &record) const;

        // True when a Sony-style product code prefix matches the region, or when
        // the prefix/region cannot be validated (unknown prefix or empty region).
        static bool serialMatchesRegion(const QString &serial, const QString &regionCode);

    private:
        QMap<QString, QString> m_regionToCode;

        void buildRegionMap();

        static QString canonicalizeToken(const QString &raw);
    };

} // namespace Compendium
} // namespace remustwo
