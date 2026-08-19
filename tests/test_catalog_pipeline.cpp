#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/database.h"
#include "../src/core/hasher.h"
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

    void matchUsesDatEntryTitleWhenCanonicalMerged();
};

void CatalogPipelineTest::matchUsesDatEntryTitleWhenCanonicalMerged() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString retailPath = dir.filePath(QStringLiteral("retail.md"));
    const QString betaPath = dir.filePath(QStringLiteral("beta.md"));
    QFile retailFile(retailPath);
    QVERIFY(retailFile.open(QIODevice::WriteOnly));
    QVERIFY(retailFile.write(QByteArrayLiteral("retail")) == 6);
    retailFile.close();
    QFile betaFile(betaPath);
    QVERIFY(betaFile.open(QIODevice::WriteOnly));
    QVERIFY(betaFile.write(QByteArrayLiteral("beta")) == 4);
    betaFile.close();

    Hasher hasher;
    const HashResult retailHash = hasher.calculateHashes(retailPath);
    const HashResult betaHash = hasher.calculateHashes(betaPath);
    QVERIFY(retailHash.success);
    QVERIFY(betaHash.success);

    const QString datPath = dir.filePath(QStringLiteral("genesis.dat"));
    QFile datFile(datPath);
    QVERIFY(datFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&datFile);
    stream << "clrmamepro (\n    name \"Sega - Mega Drive - Genesis\"\n)\n";
    stream << "game (\n    name \"Castlevania - Bloodlines (USA)\"\n"
           << "    rom ( name \"Castlevania - Bloodlines (USA).md\" size 6 crc " << retailHash.crc32 << " md5 "
           << retailHash.md5 << " sha1 " << retailHash.sha1 << " serial \"T-95076-00\" )\n)\n";
    stream << "game (\n    name \"Castlevania - Bloodlines (USA) (Beta 1)\"\n"
           << "    rom ( name \"Castlevania - Bloodlines (USA) (Beta 1).md\" size 4 crc " << betaHash.crc32 << " md5 "
           << betaHash.md5 << " sha1 " << betaHash.sha1 << " serial \"T-95076-00\" )\n)\n";
    datFile.close();

    const QString catalogPath = dir.filePath(QStringLiteral("catalog.db"));
    QVERIFY(catalog::init(catalogPath));
    auto ingest = catalog::ingestDat(catalogPath, datPath);
    QVERIFY2(ingest, qPrintable(ingest.error()));

    auto retailMatch = catalog::matchFile(catalogPath, retailPath);
    QVERIFY2(retailMatch, qPrintable(retailMatch.error()));
    QCOMPARE(retailMatch->title, QStringLiteral("Castlevania - Bloodlines (USA)"));

    auto betaMatch = catalog::matchFile(catalogPath, betaPath);
    QVERIFY2(betaMatch, qPrintable(betaMatch.error()));
    QCOMPARE(betaMatch->title, QStringLiteral("Castlevania - Bloodlines (USA) (Beta 1)"));
    QVERIFY(retailMatch->gameId != betaMatch->gameId);
}

QTEST_MAIN(CatalogPipelineTest)
#include "test_catalog_pipeline.moc"
