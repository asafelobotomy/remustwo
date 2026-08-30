#include "matching_engine.h"
#include "constants/confidence.h"
#include "constants/match_methods.h"
#include <QFileInfo>
#include <QRegularExpression>

namespace remustwo {

MatchingEngine::MatchingEngine(QObject *parent)
    : QObject(parent) { }

int MatchingEngine::calculateConfidence(const QString &method, float nameMatchScore) {
    const QString canonicalMethod = Constants::MatchMethods::canonicalize(method);

    if (canonicalMethod == Constants::MatchMethods::HASH || canonicalMethod == Constants::MatchMethods::MANUAL) {
        return static_cast<int>(Constants::Confidence::Thresholds::HASH_MATCH);
    }
    if (canonicalMethod == Constants::MatchMethods::NAME) {
        return static_cast<int>(Constants::Confidence::Thresholds::EXACT_NAME);
    }
    if (canonicalMethod == Constants::MatchMethods::FUZZY) {
        if (nameMatchScore >= Constants::Confidence::FuzzyThresholds::MEDIUM_SIMILARITY) {
            return static_cast<int>(Constants::Confidence::Thresholds::FUZZY_MAX);
        }
        if (nameMatchScore >= Constants::Confidence::FuzzyThresholds::LOW_SIMILARITY) {
            return static_cast<int>(Constants::Confidence::Thresholds::FUZZY_MIN);
        }
        return static_cast<int>(Constants::Confidence::Thresholds::VERY_LOW);
    }

    return 0;
}

QString MatchingEngine::normalizeFileName(const QString &fileName) {
    QString normalized = fileName;

    QFileInfo info(normalized);
    normalized = info.completeBaseName();

    normalized.remove(QRegularExpression("\\([^)]*\\)"));
    normalized.remove(QRegularExpression("\\[[^\\]]*\\]"));
    normalized = normalized.replace('_', ' ').replace('-', ' ').replace('.', ' ').simplified().toLower();
    normalized = normalized.simplified();

    return normalized;
}

QString MatchingEngine::extractGameTitle(const QString &fileName) {
    QString title = fileName;

    QFileInfo info(title);
    title = info.completeBaseName();

    QRegularExpression regionPattern("^([^(]+)");
    QRegularExpressionMatch match = regionPattern.match(title);

    if (match.hasMatch()) {
        title = match.captured(1).trimmed();
    }

    title = title.replace('_', ' ').simplified();

    return title;
}

} // namespace remustwo
