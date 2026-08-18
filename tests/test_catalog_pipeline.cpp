#include <QtTest>
#include <QTemporaryDir>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/database.h"

using namespace remustwo;

class CatalogPipelineTest : public QObject {
    Q_OBJECT

private slots:
    void ingestAndMatchFixture() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("catalog.db"));
        const QString datPath = QStringLiteral(REMUSTWO_SOURCE_DIR) + QStringLiteral("/testdata/fixture.dat");
        const QString romPath = QStringLiteral(REMUSTWO_SOURCE_DIR) + QStringLiteral("/testdata/fixture.bin");
        QVERIFY(catalog::init(dbPath));
        auto ingest = catalog::ingestDat(dbPath, datPath);
        QVERIFY2(ingest, qPrintable(ingest.error()));
        const QString libraryPath = dir.filePath(QStringLiteral("library.db"));
        auto match = catalog::matchFile(dbPath, romPath, libraryPath);
        QVERIFY2(match, qPrintable(match.error()));
        QCOMPARE(match->title, QStringLiteral("Fixture Game (USA)"));
        QCOMPARE(match->confidence, 100);
        QVERIFY(match->systemId > 0);

        Database library;
        QVERIFY(library.initialize(libraryPath));
        const auto files = library.getAllFiles();
        QCOMPARE(files.size(), 1);
        QCOMPARE(files.first().baseTitle, QStringLiteral("Fixture Game (USA)"));
        QVERIFY(files.first().hashCalculated);
        QVERIFY(files.first().systemId > 0);

        auto ingestAgain = catalog::ingestDat(dbPath, datPath);
        QVERIFY2(ingestAgain, qPrintable(ingestAgain.error()));
        auto matchAgain = catalog::matchFile(dbPath, romPath);
        QVERIFY2(matchAgain, qPrintable(matchAgain.error()));
        QCOMPARE(matchAgain->title, QStringLiteral("Fixture Game (USA)"));
    }
};

QTEST_MAIN(CatalogPipelineTest)
#include "test_catalog_pipeline.moc"
