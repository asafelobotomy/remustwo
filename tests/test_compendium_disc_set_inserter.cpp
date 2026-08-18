#include <QtTest/QtTest>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "../src/catalog/catalog.h"
#include "../src/catalog/compendium_dat_extractor.h"
#include "../src/catalog/compendium_fact_inserter.h"
#include "../src/catalog/compendium_identity_linker.h"
#include "../src/catalog/compendium_normalizer.h"
#include "../src/core/disc_set_key.h"

using namespace remustwo;
using namespace remustwo::Compendium;

class CompendiumDiscSetInserterTest : public QObject {
    Q_OBJECT

private:
    static bool execSql(QSqlDatabase &db, const QString &sql) {
        QSqlQuery query(db);
        return query.exec(sql);
    }

    static bool openSeededCatalog(const QString &dbPath, const QString &connectionName, QSqlDatabase &db,
        QString &error) {
        auto initResult = catalog::init(dbPath);
        if (!initResult) {
            error = initResult.error();
            return false;
        }
        auto opened = catalog::open(dbPath, connectionName);
        if (!opened) {
            error = opened.error();
            return false;
        }
        db = *opened;
        return execSql(db,
                   QStringLiteral("INSERT OR IGNORE INTO sources "
                                  "(source_id, display_name, source_type, priority, enabled, license_id, "
                                  "attribution_required) "
                                  "VALUES ('redump', 'Redump', 'dat', 10, 1, 'community', 0)"))
            && execSql(db,
                QStringLiteral("INSERT OR IGNORE INTO source_snapshots (snapshot_id, source_id, snapshot_label) "
                               "VALUES ('snap-ff7', 'redump', 'test'), ('snap-chd', 'redump', 'test-chd')"));
    }

private slots:
    void ff7MultiDisc_createsSharedSetKeyAndTracks();
    void chdDisk_setsPrimaryContentSha1();
    void shenmueVariants_sameSetKeyDifferentVariant();
    void residentEvilSplitPath_differentSetKeys();
};

void CompendiumDiscSetInserterTest::ff7MultiDisc_createsSharedSetKeyAndTracks() {
    const QString datContent
        = QStringLiteral("clrmamepro (\n"
                         "    name \"Sony - PlayStation\"\n"
                         ")\n"
                         "game (\n"
                         "    name \"Final Fantasy VII (USA) (Disc 1)\"\n"
                         "    rom ( name \"Final Fantasy VII (USA) (Disc 1).bin\" size 100 crc AAAAAAAA )\n"
                         ")\n"
                         "game (\n"
                         "    name \"Final Fantasy VII (USA) (Disc 2)\"\n"
                         "    rom ( name \"Final Fantasy VII (USA) (Disc 2).bin\" size 100 crc BBBBBBBB )\n"
                         ")\n"
                         "game (\n"
                         "    name \"Final Fantasy VII (USA) (Disc 3)\"\n"
                         "    rom ( name \"Final Fantasy VII (USA) (Disc 3).bin\" size 100 crc CCCCCCCC )\n"
                         ")\n");

    QTemporaryFile datFile;
    datFile.setAutoRemove(true);
    QVERIFY(datFile.open());
    datFile.write(datContent.toUtf8());
    datFile.close();

    QString extractError;
    QList<SourceRecordEnvelope> records
        = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-ff7"), extractError);
    QVERIFY2(extractError.isEmpty(), qPrintable(extractError));
    QCOMPARE(records.size(), 3);
    QCOMPARE(records[0].parsedDiscNumber, 1);
    QCOMPARE(records[1].parsedDiscNumber, 2);
    QCOMPARE(records[2].parsedDiscNumber, 3);
    QCOMPARE(records[0].parsedDiscCount, 3);

    CompendiumNormalizer normalizer;
    for (SourceRecordEnvelope &rec : records)
        normalizer.normalize(rec);
    QCOMPARE(records[0].resolvedSystemId, 14);
    QCOMPARE(records[0].resolvedRegionCode, QStringLiteral("USA"));

    IdentityLinker linker;
    const int created = linker.link(records);
    QCOMPARE(created, 1);
    const QString gameId = records[0].linkedGameId;
    QVERIFY(!gameId.isEmpty());
    QCOMPARE(records[1].linkedGameId, gameId);
    QCOMPARE(records[2].linkedGameId, gameId);
    QVERIFY(!records[0].datGameBlockName.isEmpty());

    const QString expectedSetKey
        = DiscSetKey::compute(14, QStringLiteral("Final Fantasy VII (USA) (Disc 1)"), QStringLiteral("USA"));
    QCOMPARE(DiscSetKey::compute(14, records[1].datGameBlockName, QStringLiteral("USA")), expectedSetKey);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("compendium_disc_set_inserter_test");
    QSqlDatabase db;
    QString seedError;
    QVERIFY2(openSeededCatalog(tempDir.filePath(QStringLiteral("catalog.db")), connectionName, db, seedError),
        qPrintable(seedError));

    CompilerStats stats;
    QString insertError;
    const FactInserter inserter;
    QVERIFY2(inserter.insert(records, db, stats, insertError), qPrintable(insertError));
    QCOMPARE(stats.discSetsCreated, 3);
    QCOMPARE(stats.tracksCreated, 3);

    QSqlQuery query(db);
    QVERIFY(query.exec(
        QStringLiteral("SELECT COUNT(*) FROM game_disc_sets WHERE game_id = '") + gameId + QLatin1Char('\'')));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 3);

    QVERIFY(query.exec(
        QStringLiteral("SELECT DISTINCT set_key FROM game_disc_sets WHERE game_id = '") + gameId + QLatin1Char('\'')));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), expectedSetKey);
    QVERIFY(!query.next());

    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM game_disc_tracks")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 3);

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumDiscSetInserterTest::chdDisk_setsPrimaryContentSha1() {
    const QString datContent = QStringLiteral("clrmamepro (\n"
                                              "    name \"Sony - PlayStation\"\n"
                                              ")\n"
                                              "game (\n"
                                              "    name \"Metal Gear Solid (USA) (Disc 1)\"\n"
                                              "    rom ( name \"Metal Gear Solid (USA) (Disc 1).chd\" size 100 "
                                              "sha1 abcdef0123456789abcdef0123456789abcdef01 )\n"
                                              ")\n");

    QTemporaryFile datFile;
    datFile.setAutoRemove(true);
    QVERIFY(datFile.open());
    datFile.write(datContent.toUtf8());
    datFile.close();

    QString extractError;
    QList<SourceRecordEnvelope> records
        = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-chd"), extractError);
    QVERIFY2(extractError.isEmpty(), qPrintable(extractError));
    QCOMPARE(records.size(), 1);
    QCOMPARE(records[0].primaryContentSha1, QStringLiteral("abcdef0123456789abcdef0123456789abcdef01"));

    CompendiumNormalizer normalizer;
    for (SourceRecordEnvelope &rec : records)
        normalizer.normalize(rec);

    IdentityLinker linker;
    linker.link(records);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("compendium_disc_set_chd_test");
    QSqlDatabase db;
    QString seedError;
    QVERIFY2(openSeededCatalog(tempDir.filePath(QStringLiteral("catalog.db")), connectionName, db, seedError),
        qPrintable(seedError));

    CompilerStats stats;
    QString insertError;
    const FactInserter inserter;
    QVERIFY2(inserter.insert(records, db, stats, insertError), qPrintable(insertError));

    QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("SELECT primary_content_sha1 FROM game_disc_sets LIMIT 1")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("abcdef0123456789abcdef0123456789abcdef01"));

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumDiscSetInserterTest::shenmueVariants_sameSetKeyDifferentVariant() {
    const QString datContent
        = QStringLiteral("clrmamepro (\n"
                         "    name \"Sony - PlayStation\"\n"
                         ")\n"
                         "game (\n"
                         "    name \"Shenmue (USA) (Disc 3) [!][1S]\"\n"
                         "    rom ( name \"Shenmue (USA) (Disc 3) [!][1S].bin\" size 100 crc AAAAAAAA )\n"
                         ")\n"
                         "game (\n"
                         "    name \"Shenmue (USA) (Disc 3) [!][2S]\"\n"
                         "    rom ( name \"Shenmue (USA) (Disc 3) [!][2S].bin\" size 100 crc BBBBBBBB )\n"
                         ")\n");

    QTemporaryFile datFile;
    datFile.setAutoRemove(true);
    QVERIFY(datFile.open());
    datFile.write(datContent.toUtf8());
    datFile.close();

    QString extractError;
    QList<SourceRecordEnvelope> records
        = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-ff7"), extractError);
    QVERIFY2(extractError.isEmpty(), qPrintable(extractError));
    QCOMPARE(records.size(), 2);
    QVERIFY(records[0].parsedSetVariant.contains(QStringLiteral("1S")));
    QVERIFY(records[1].parsedSetVariant.contains(QStringLiteral("2S")));

    CompendiumNormalizer normalizer;
    for (SourceRecordEnvelope &rec : records)
        normalizer.normalize(rec);

    IdentityLinker linker;
    linker.link(records);

    const QString setKeyOne = DiscSetKey::compute(14, records[0].datGameBlockName, QStringLiteral("USA"));
    const QString setKeyTwo = DiscSetKey::compute(14, records[1].datGameBlockName, QStringLiteral("USA"));
    QCOMPARE(setKeyOne, setKeyTwo);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("compendium_disc_set_variant_test");
    QSqlDatabase db;
    QString seedError;
    QVERIFY2(openSeededCatalog(tempDir.filePath(QStringLiteral("catalog.db")), connectionName, db, seedError),
        qPrintable(seedError));

    CompilerStats stats;
    QString insertError;
    const FactInserter inserter;
    QVERIFY2(inserter.insert(records, db, stats, insertError), qPrintable(insertError));
    QCOMPARE(stats.discSetsCreated, 2);

    QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("SELECT set_key, set_variant FROM game_disc_sets ORDER BY set_variant")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), setKeyOne);
    QVERIFY(query.value(1).toString().contains(QStringLiteral("1S")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), setKeyTwo);
    QVERIFY(query.value(1).toString().contains(QStringLiteral("2S")));
    QVERIFY(!query.next());

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

void CompendiumDiscSetInserterTest::residentEvilSplitPath_differentSetKeys() {
    const QString datContent
        = QStringLiteral("clrmamepro (\n"
                         "    name \"Sony - PlayStation\"\n"
                         ")\n"
                         "game (\n"
                         "    name \"Resident Evil 2 (USA) (Disc 1) (Leon)\"\n"
                         "    rom ( name \"Resident Evil 2 (USA) (Disc 1) (Leon).bin\" size 100 crc AAAAAAAA )\n"
                         ")\n"
                         "game (\n"
                         "    name \"Resident Evil 2 (USA) (Disc 2) (Claire)\"\n"
                         "    rom ( name \"Resident Evil 2 (USA) (Disc 2) (Claire).bin\" size 100 crc BBBBBBBB )\n"
                         ")\n");

    QTemporaryFile datFile;
    datFile.setAutoRemove(true);
    QVERIFY(datFile.open());
    datFile.write(datContent.toUtf8());
    datFile.close();

    QString extractError;
    QList<SourceRecordEnvelope> records
        = DatExtractor::extract(datFile.fileName(), QStringLiteral("redump"), QStringLiteral("snap-ff7"), extractError);
    QVERIFY2(extractError.isEmpty(), qPrintable(extractError));
    QCOMPARE(records.size(), 2);

    CompendiumNormalizer normalizer;
    for (SourceRecordEnvelope &rec : records)
        normalizer.normalize(rec);

    IdentityLinker linker;
    linker.link(records);

    const QString leonKey = DiscSetKey::compute(14, records[0].datGameBlockName, QStringLiteral("USA"));
    const QString claireKey = DiscSetKey::compute(14, records[1].datGameBlockName, QStringLiteral("USA"));
    QVERIFY(leonKey != claireKey);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString connectionName = QStringLiteral("compendium_disc_set_re2_test");
    QSqlDatabase db;
    QString seedError;
    QVERIFY2(openSeededCatalog(tempDir.filePath(QStringLiteral("catalog.db")), connectionName, db, seedError),
        qPrintable(seedError));

    CompilerStats stats;
    QString insertError;
    const FactInserter inserter;
    QVERIFY2(inserter.insert(records, db, stats, insertError), qPrintable(insertError));
    QCOMPARE(stats.discSetsCreated, 2);

    QSqlQuery query(db);
    QVERIFY(query.exec(QStringLiteral("SELECT COUNT(DISTINCT set_key) FROM game_disc_sets")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);

    db.close();
    QSqlDatabase::removeDatabase(connectionName);
}

QTEST_MAIN(CompendiumDiscSetInserterTest)
#include "test_compendium_disc_set_inserter.moc"
