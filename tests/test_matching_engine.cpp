#include <QtTest/QtTest>
#include "../src/core/matching_engine.h"
#include "../src/core/constants/match_methods.h"
#include "../src/core/constants/confidence.h"

using namespace remustwo;
using namespace remustwo::Constants;

class MatchingEngineTest : public QObject {
    Q_OBJECT

private slots:
    void testConfidenceHashMatch();
    void testConfidenceExactNameMatch();
    void testConfidenceFuzzyMatchHigh();
    void testConfidenceFuzzyMatchMedium();
    void testConfidenceFuzzyMatchLow();
    void testConfidenceManualMatch();
    void testConfidenceUnknown();

    void testNormalizeFileNameBasic();
    void testNormalizeFileNameWithRegion();
    void testNormalizeFileNameWithTags();
    void testNormalizeFileNameWithUnderscores();
    void testNormalizeFileNameEmpty();
    void testNormalizeFileNameSpecialChars();

    void testExtractGameTitleBasic();
    void testExtractGameTitleWithRegion();
    void testExtractGameTitleWithVersion();
    void testExtractGameTitleEmpty();
    void testExtractGameTitleNoIntroFormat();
};

void MatchingEngineTest::testConfidenceHashMatch() {
    int confidence = MatchingEngine::calculateConfidence(MatchMethods::HASH, 0.0f);
    QCOMPARE(confidence, static_cast<int>(Confidence::Thresholds::HASH_MATCH));
}

void MatchingEngineTest::testConfidenceExactNameMatch() {
    int confidence = MatchingEngine::calculateConfidence(MatchMethods::EXACT_NAME, 1.0f);
    QCOMPARE(confidence, static_cast<int>(Confidence::Thresholds::EXACT_NAME));
}

void MatchingEngineTest::testConfidenceFuzzyMatchHigh() {
    int confidence = MatchingEngine::calculateConfidence(MatchMethods::FUZZY_NAME, 0.85f);
    QCOMPARE(confidence, static_cast<int>(Confidence::Thresholds::FUZZY_MAX));
}

void MatchingEngineTest::testConfidenceFuzzyMatchMedium() {
    int confidence = MatchingEngine::calculateConfidence(MatchMethods::FUZZY_NAME, 0.70f);
    QCOMPARE(confidence, static_cast<int>(Confidence::Thresholds::FUZZY_MIN));
}

void MatchingEngineTest::testConfidenceFuzzyMatchLow() {
    int confidence = MatchingEngine::calculateConfidence(MatchMethods::FUZZY_NAME, 0.50f);
    QCOMPARE(confidence, static_cast<int>(Confidence::Thresholds::VERY_LOW));
}

void MatchingEngineTest::testConfidenceManualMatch() {
    int confidence = MatchingEngine::calculateConfidence(MatchMethods::MANUAL, 0.0f);
    QCOMPARE(confidence, static_cast<int>(Confidence::Thresholds::USER_CONFIRMED));
}

void MatchingEngineTest::testConfidenceUnknown() {
    int confidence = MatchingEngine::calculateConfidence("unknown_method", 0.0f);
    QCOMPARE(confidence, 0);
}

void MatchingEngineTest::testNormalizeFileNameBasic() {
    QCOMPARE(MatchingEngine::normalizeFileName("Super Mario Bros.nes"), QStringLiteral("super mario bros"));
}

void MatchingEngineTest::testNormalizeFileNameWithRegion() {
    QCOMPARE(MatchingEngine::normalizeFileName("Super Mario Bros. (USA).nes"), QStringLiteral("super mario bros"));
}

void MatchingEngineTest::testNormalizeFileNameWithTags() {
    QCOMPARE(MatchingEngine::normalizeFileName("Super Mario Bros. (USA) [!].nes"), QStringLiteral("super mario bros"));
}

void MatchingEngineTest::testNormalizeFileNameWithUnderscores() {
    QCOMPARE(MatchingEngine::normalizeFileName("Super_Mario_Bros_3.nes"), QStringLiteral("super mario bros 3"));
}

void MatchingEngineTest::testNormalizeFileNameEmpty() {
    QCOMPARE(MatchingEngine::normalizeFileName(""), QStringLiteral(""));
}

void MatchingEngineTest::testNormalizeFileNameSpecialChars() {
    QCOMPARE(MatchingEngine::normalizeFileName("Mega-Man-X (USA) (Rev 1).sfc"), QStringLiteral("mega man x"));
}

void MatchingEngineTest::testExtractGameTitleBasic() {
    QCOMPARE(MatchingEngine::extractGameTitle("Sonic the Hedgehog.md"), QStringLiteral("Sonic the Hedgehog"));
}

void MatchingEngineTest::testExtractGameTitleWithRegion() {
    QCOMPARE(MatchingEngine::extractGameTitle("Sonic the Hedgehog (USA, Europe).md"),
        QStringLiteral("Sonic the Hedgehog"));
}

void MatchingEngineTest::testExtractGameTitleWithVersion() {
    QCOMPARE(MatchingEngine::extractGameTitle("Street Fighter II (USA) (Rev A).sfc"),
        QStringLiteral("Street Fighter II"));
}

void MatchingEngineTest::testExtractGameTitleEmpty() {
    QCOMPARE(MatchingEngine::extractGameTitle(""), QStringLiteral(""));
}

void MatchingEngineTest::testExtractGameTitleNoIntroFormat() {
    QCOMPARE(MatchingEngine::extractGameTitle("Legend of Zelda, The - A Link to the Past (USA).sfc"),
        QStringLiteral("Legend of Zelda, The - A Link to the Past"));
}

QTEST_MAIN(MatchingEngineTest)
#include "test_matching_engine.moc"
