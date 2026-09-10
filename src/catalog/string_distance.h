#pragma once

#include <QString>
#include <algorithm>
#include <vector>

namespace remustwo {
namespace Compendium {

/// Normalized Levenshtein similarity in [0, 1] (1 = identical). Empty inputs → 0.
inline float normalizedLevenshteinSimilarity(const QString &a, const QString &b) {
    if (a.isEmpty() && b.isEmpty())
        return 1.0f;
    if (a.isEmpty() || b.isEmpty())
        return 0.0f;
    if (a == b)
        return 1.0f;

    const int n = a.size();
    const int m = b.size();
    std::vector<int> prev(static_cast<size_t>(m) + 1);
    std::vector<int> cur(static_cast<size_t>(m) + 1);
    for (int j = 0; j <= m; ++j)
        prev[static_cast<size_t>(j)] = j;

    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        const QChar ca = a.at(i - 1);
        for (int j = 1; j <= m; ++j) {
            const int cost = (ca == b.at(j - 1)) ? 0 : 1;
            cur[static_cast<size_t>(j)] = std::min({ prev[static_cast<size_t>(j)] + 1, cur[static_cast<size_t>(j - 1)] + 1,
                prev[static_cast<size_t>(j - 1)] + cost });
        }
        prev.swap(cur);
    }
    const int dist = prev[static_cast<size_t>(m)];
    const int denom = std::max(n, m);
    return 1.0f - (static_cast<float>(dist) / static_cast<float>(denom));
}

} // namespace Compendium
} // namespace remustwo
