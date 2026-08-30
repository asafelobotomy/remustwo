#pragma once

#include "constants/confidence.h"
#include <QObject>
#include <QString>

namespace remustwo {

/**
 * @brief Matching helpers for confidence scoring and title extraction.
 */
class MatchingEngine : public QObject {
    Q_OBJECT

public:
    explicit MatchingEngine(QObject *parent = nullptr);
    ~MatchingEngine() override = default;

    /**
     * @brief Calculate confidence score based on matching method
     * @param method Match method string, normalized via Constants::MatchMethods
     * @param nameMatchScore For fuzzy matches, the similarity score (0.0-1.0)
     * @return Confidence percentage (0-100)
     */
    static int calculateConfidence(const QString &method, float nameMatchScore = 0.0f);

    /**
     * @brief Normalize a filename for matching
     *
     * Removes file extension, region tags, version tags, etc.
     * "Super Mario Bros. (USA).nes" → "super mario bros"
     */
    static QString normalizeFileName(const QString &fileName);

    /**
     * @brief Extract a display title from a filename (keeps original casing)
     */
    static QString extractGameTitle(const QString &fileName);
};

} // namespace remustwo
