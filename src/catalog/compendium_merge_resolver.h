#pragma once
// Phase 1 compendium compiler: merge resolver.
// Implements the merge_policy rules from seeds/0003_merge_policy.sql as
// hardcoded SQL (and a small C++ title-similarity pass). The merge_policy
// table documents rule keys/order; the resolver does not interpret it at runtime.

#include "compendium_types.h"
#include <QSqlDatabase>
#include <QString>

namespace remustwo {
namespace Compendium {

    class MergeResolver {
    public:
        // Run the full merge pass over all game_facts in the open DB.
        // Updates stats.resolvedFields and stats.unresolvedConflicts.
        // Returns false on fatal DB error.
        bool resolve(QSqlDatabase &db, CompilerStats &stats, QString &error) const;

    private:
        /// When titles disagree and no high-confidence hash title exists, pick the
        /// fact closest (normalized Levenshtein) to the game_names / title alias set.
        bool applyNormalizedNameSimilarity(QSqlDatabase &db, QString &error) const;
    };

} // namespace Compendium
} // namespace remustwo
