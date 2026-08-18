#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/database.h"
#include "../src/metadata/hasheous_provider.h"
#include "../src/metadata/library_enrich.h"
#include "rom_paths.h"

using namespace remustwo;

class LibraryEnrichTest : public QObject {
    Q_OBJECT

private slots:
    void lookupJsonContainsHashes() {
        const QByteArray body = HasheousProvider::lookupJson(QStringLiteral("8a27668c"),
            QStringLiteral("5c5efc5cad24167b91d06ed3dc6bf524"),
            QStringLiteral("b5c6db56d69f455f4dfa8800d0ce4a7b7f183dd6"));
        QVERIFY(!body.isEmpty());
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QVERIFY(doc.isArray());
        QCOMPARE(doc.array().size(), 1);
        const QJsonObject entry = doc.array().first().toObject();
        QCOMPARE(entry.value(QStringLiteral("mD5")).toString(), QStringLiteral("5c5efc5cad24167b91d06ed3dc6bf524"));
        QCOMPARE(
            entry.value(QStringLiteral("shA1")).toString(), QStringLiteral("b5c6db56d69f455f4dfa8800d0ce4a7b7f183dd6"));
    }

    void enrichOnlyMatchedLibraryGames() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString catalogPath = dir.filePath(QStringLiteral("catalog.db"));
        const QString libraryPath = dir.filePath(QStringLiteral("library.db"));
        const QString datPath = test::syntheticFixtureDat();
        const QString romSource = test::syntheticFixtureRom();
        const QString romPath = dir.filePath(QStringLiteral("fixture.bin"));
        QVERIFY(QFile::copy(romSource, romPath));
        QVERIFY(catalog::init(catalogPath));
        QVERIFY(catalog::ingestDat(catalogPath, datPath));

        {
            auto catalogResult = catalog::open(catalogPath, QStringLiteral("enrich_extra"));
            QVERIFY(catalogResult);
            QSqlDatabase extraDb = *catalogResult;
            QSqlQuery extra(extraDb);
            QVERIFY(extra.exec(
                QStringLiteral("INSERT INTO games (game_id, system_id, canonical_title, canonical_confidence) "
                               "VALUES ('unowned-game', 1, 'Unowned Title', 1.0)")));
            extra.prepare(QStringLiteral(
                "INSERT INTO game_signatures (game_id, hash_type, hash_value, source_id, confidence, is_primary) "
                "VALUES ('unowned-game', 'md5', 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa', "
                "(SELECT source_id FROM sources LIMIT 1), 1.0, 1)"));
            extra.exec();
            extra.finish();
            const QString conn = extraDb.connectionName();
            extraDb.close();
            QSqlDatabase::removeDatabase(conn);
        }

        QVERIFY(catalog::matchFile(catalogPath, romPath, libraryPath));

        auto gapped = catalog::open(catalogPath, QStringLiteral("enrich_gap"));
        QVERIFY(gapped);
        QSqlDatabase gapDb = *gapped;
        QSqlQuery gap(gapDb);
        QVERIFY(gap.exec(QStringLiteral("UPDATE games SET cover_url = '', description = ''")));
        gap.finish();
        const QString gapConn = gapDb.connectionName();
        gapDb.close();
        QSqlDatabase::removeDatabase(gapConn);

        int lookups = 0;
        EnrichOptions options;
        options.online = true;
        options.lookup = [&](const QString &, const QString &, const QString &) {
            ++lookups;
            GameMetadata metadata;
            metadata.description = QStringLiteral("from lookup");
            metadata.boxArtUrl = QStringLiteral("https://example.com/cover.jpg");
            return metadata;
        };
        auto result = enrichLibrary(catalogPath, libraryPath, options);
        QVERIFY2(result, qPrintable(result.error()));
        QCOMPARE(result->matchedGames, 1);
        QCOMPARE(lookups, 1);
        QCOMPARE(result->fetched, 1);
    }

    void dryRunDoesNotCallLookup() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString catalogPath = dir.filePath(QStringLiteral("catalog.db"));
        const QString libraryPath = dir.filePath(QStringLiteral("library.db"));
        const QString datPath = test::syntheticFixtureDat();
        const QString romSource = test::syntheticFixtureRom();
        const QString romPath = dir.filePath(QStringLiteral("fixture.bin"));
        QVERIFY(QFile::copy(romSource, romPath));
        QVERIFY(catalog::init(catalogPath));
        QVERIFY(catalog::ingestDat(catalogPath, datPath));
        QVERIFY(catalog::matchFile(catalogPath, romPath, libraryPath));

        int lookups = 0;
        EnrichOptions options;
        options.online = true;
        options.dryRun = true;
        options.lookup = [&](const QString &, const QString &, const QString &) {
            ++lookups;
            return GameMetadata();
        };
        auto result = enrichLibrary(catalogPath, libraryPath, options);
        QVERIFY2(result, qPrintable(result.error()));
        QCOMPARE(lookups, 0);
        QVERIFY(result->wouldFetch <= 1);
        QVERIFY(result->matchedGames <= 1);
    }
};

QTEST_MAIN(LibraryEnrichTest)
#include "test_library_enrich.moc"
