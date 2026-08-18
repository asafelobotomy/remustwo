#include <QtTest>
#include <QTemporaryDir>
#include <QFile>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/database.h"
#include "rom_paths.h"

using namespace remustwo;

class CatalogPipelineTest : public QObject {
    Q_OBJECT

private slots:
    void ingestAndMatchFixture() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("catalog.db"));
        const QString datPath = test::syntheticFixtureDat();
        const QString romSource = test::syntheticFixtureRom();
        const QString romPath = dir.filePath(QStringLiteral("fixture.bin"));
        QVERIFY(QFile::copy(romSource, romPath));
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
        QVERIFY(!files.first().catalogGameId.isEmpty());
        QCOMPARE(library.getFilesEligibleForOrganize().size(), 1);

        QVERIFY(QFile::remove(romPath));
        QVERIFY(!QFile::exists(romPath));
        auto matchWithoutFile = catalog::matchLibrary(dbPath, libraryPath);
        QVERIFY2(matchWithoutFile, qPrintable(matchWithoutFile.error()));
        QCOMPARE(*matchWithoutFile, 1);

        auto ingestAgain = catalog::ingestDat(dbPath, datPath);
        QVERIFY2(ingestAgain, qPrintable(ingestAgain.error()));
        QVERIFY(QFile::copy(romSource, romPath));
        auto matchAgain = catalog::matchFile(dbPath, romPath);
        QVERIFY2(matchAgain, qPrintable(matchAgain.error()));
        QCOMPARE(matchAgain->title, QStringLiteral("Fixture Game (USA)"));
    }
};

QTEST_MAIN(CatalogPipelineTest)
#include "test_catalog_pipeline.moc"
