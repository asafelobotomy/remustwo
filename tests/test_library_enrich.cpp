#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/database.h"
#include "../src/metadata/hasheous_provider.h"
#include "../src/metadata/hasheous_igdb.h"
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
        QVERIFY(gap.exec(QStringLiteral(
            "UPDATE games SET cover_url = '', description = '', developer = NULL, publisher = NULL, "
            "release_date = NULL")));
        gap.finish();
        const QString gapConn = gapDb.connectionName();
        gapDb.close();
        QSqlDatabase::removeDatabase(gapConn);

        int lookups = 0;
        EnrichOptions options;
        options.online = true;
        options.urlExists = [](const QUrl &) { return false; };
        options.lookup = [&](const QString &, const QString &, const QString &) {
            ++lookups;
            GameMetadata metadata;
            metadata.description = QStringLiteral("from lookup");
            metadata.boxArtUrl = QStringLiteral("https://example.com/cover.jpg");
            metadata.developer = QStringLiteral("Lookup Dev");
            metadata.publisher = QStringLiteral("Lookup Pub");
            metadata.releaseDate = QStringLiteral("1991-02-03");
            return metadata;
        };
        auto result = enrichLibrary(catalogPath, libraryPath, options);
        QVERIFY2(result, qPrintable(result.error()));
        QCOMPARE(result->matchedGames, 1);
        QCOMPARE(lookups, 1);
        QCOMPARE(result->fetched, 1);
        QCOMPARE(result->updated, 1);

        auto check = catalog::open(catalogPath, QStringLiteral("enrich_check"));
        QVERIFY(check);
        QSqlDatabase checkDb = *check;
        QSqlQuery q(checkDb);
        QVERIFY(q.exec(QStringLiteral(
            "SELECT description, developer, publisher, release_date, cover_url FROM games "
            "WHERE description = 'from lookup'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("from lookup"));
        QCOMPARE(q.value(1).toString(), QStringLiteral("Lookup Dev"));
        QCOMPARE(q.value(2).toString(), QStringLiteral("Lookup Pub"));
        QCOMPARE(q.value(3).toString(), QStringLiteral("1991-02-03"));
        QCOMPARE(q.value(4).toString(), QStringLiteral("https://example.com/cover.jpg"));
        QVERIFY(q.exec(QStringLiteral(
            "SELECT COUNT(*) FROM game_assets WHERE asset_type = 'boxart' AND source = 'hasheous'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
        const QString checkConn = checkDb.connectionName();
        checkDb.close();
        QSqlDatabase::removeDatabase(checkConn);
    }

    void enrichWritesLibretroAssetUrlsOffline() {
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

        {
            auto cat = catalog::open(catalogPath, QStringLiteral("enrich_libretro_prep"));
            QVERIFY(cat);
            QSqlDatabase db = std::move(*cat);
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral(
                "UPDATE games SET cover_url = '', description = 'already has text', "
                "developer = 'Dev', publisher = 'Pub', release_date = '1990-01-01'")));
            QVERIFY(q.exec(QStringLiteral(
                "UPDATE systems SET libretro_name = 'Nintendo - Nintendo Entertainment System' "
                "WHERE system_id = (SELECT system_id FROM games LIMIT 1)")));
            q.finish();
            const QString conn = db.connectionName();
            db.close();
            QSqlDatabase::removeDatabase(conn);
        }

        EnrichOptions options;
        options.online = false;
        auto result = enrichLibrary(catalogPath, libraryPath, options);
        QVERIFY2(result, qPrintable(result.error()));
        QCOMPARE(result->matchedGames, 1);
        QVERIFY(result->thumbnailUrls >= 4);
        QVERIFY(result->assetsWritten >= 4);

        auto check = catalog::open(catalogPath, QStringLiteral("enrich_libretro_check"));
        QVERIFY(check);
        QSqlDatabase db = *check;
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT asset_type, url FROM game_assets ORDER BY asset_type")));
        QStringList types;
        while (q.next()) {
            types.append(q.value(0).toString());
            QVERIFY(q.value(1).toString().contains(QStringLiteral("thumbnails.libretro.com")));
        }
        QVERIFY(types.contains(QStringLiteral("boxart")));
        QVERIFY(types.contains(QStringLiteral("snap")));
        QVERIFY(types.contains(QStringLiteral("title")));
        QVERIFY(types.contains(QStringLiteral("logo")));
        QVERIFY(q.exec(QStringLiteral("SELECT cover_url FROM games LIMIT 1")));
        QVERIFY(q.next());
        QVERIFY(q.value(0).toString().contains(QStringLiteral("Named_Boxarts")));
        const QString conn = db.connectionName();
        db.close();
        QSqlDatabase::removeDatabase(conn);
    }

    void parseIgdbGameObjectExtractsDeepFields() {
        QJsonObject igdb;
        igdb.insert(QStringLiteral("id"), 42);
        igdb.insert(QStringLiteral("name"), QStringLiteral("Deep Game"));
        igdb.insert(QStringLiteral("summary"), QStringLiteral("A deep summary"));
        igdb.insert(QStringLiteral("first_release_date"), QStringLiteral("1992-06-01"));
        igdb.insert(QStringLiteral("total_rating"), 82.5);
        QJsonObject cover;
        cover.insert(QStringLiteral("url"), QStringLiteral("//images.example/cover.jpg"));
        igdb.insert(QStringLiteral("cover"), cover);
        QJsonObject genres;
        genres.insert(QStringLiteral("1"), QJsonObject { { QStringLiteral("name"), QStringLiteral("Platform") } });
        igdb.insert(QStringLiteral("genres"), genres);
        QJsonObject screenshots;
        screenshots.insert(QStringLiteral("1"), QJsonObject { { QStringLiteral("url"), QStringLiteral("//images.example/snap.jpg") } });
        igdb.insert(QStringLiteral("screenshots"), screenshots);
        QJsonObject artworks;
        artworks.insert(QStringLiteral("1"),
            QJsonObject { { QStringLiteral("artwork_type"), 4 }, { QStringLiteral("url"), QStringLiteral("//images.example/hero.jpg") } });
        artworks.insert(QStringLiteral("2"),
            QJsonObject { { QStringLiteral("artwork_type"), 3 }, { QStringLiteral("url"), QStringLiteral("//images.example/banner.jpg") } });
        igdb.insert(QStringLiteral("artworks"), artworks);

        const GameMetadata metadata = parseIgdbGameObject(igdb);
        QCOMPARE(metadata.title, QStringLiteral("Deep Game"));
        QCOMPARE(metadata.genres, QStringList { QStringLiteral("Platform") });
        QVERIFY(metadata.rating > 8.0f);
        QCOMPARE(metadata.screenshotUrls.size(), 1);
        QVERIFY(metadata.screenshotUrls.first().startsWith(QStringLiteral("https:")));
        QCOMPARE(metadata.externalIds.value(QStringLiteral("hero_url")),
            QStringLiteral("https://images.example/hero.jpg"));
        QCOMPARE(metadata.externalIds.value(QStringLiteral("banner_url")),
            QStringLiteral("https://images.example/banner.jpg"));
    }

    void enrichDeepMergesIgdbProxy() {
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

        {
            auto cat = catalog::open(catalogPath, QStringLiteral("enrich_deep_prep"));
            QVERIFY(cat);
            QSqlDatabase db = std::move(*cat);
            QSqlQuery q(db);
            QVERIFY(q.exec(QStringLiteral(
                "UPDATE games SET cover_url = '', description = '', developer = NULL, publisher = NULL, "
                "release_date = NULL, genre = NULL, rating = NULL")));
            q.finish();
            const QString conn = db.connectionName();
            db.close();
            QSqlDatabase::removeDatabase(conn);
        }

        qputenv("REMUSTWO_HASHEOUS_API_KEY", "test-key");

        int proxyCalls = 0;
        EnrichOptions options;
        options.online = true;
        options.deep = true;
        options.urlExists = [](const QUrl &) { return false; };
        options.lookup = [&](const QString &, const QString &, const QString &) {
            GameMetadata metadata;
            metadata.description = QStringLiteral("from lookup");
            metadata.externalIds.insert(QStringLiteral("igdb"), QStringLiteral("99"));
            return metadata;
        };
        options.fetchIgdb = [&](int igdbId) {
            ++proxyCalls;
            QCOMPARE(igdbId, 99);
            GameMetadata metadata;
            metadata.genres = { QStringLiteral("Action") };
            metadata.rating = 8.5f;
            metadata.screenshotUrls = { QStringLiteral("https://example.com/snap.jpg") };
            metadata.externalIds.insert(QStringLiteral("hero_url"), QStringLiteral("https://example.com/hero.jpg"));
            metadata.externalIds.insert(QStringLiteral("banner_url"), QStringLiteral("https://example.com/banner.jpg"));
            return metadata;
        };
        auto result = enrichLibrary(catalogPath, libraryPath, options);
        qunsetenv("REMUSTWO_HASHEOUS_API_KEY");
        QVERIFY2(result, qPrintable(result.error()));
        QCOMPARE(proxyCalls, 1);
        QCOMPARE(result->proxyFetched, 1);
        QCOMPARE(result->updated, 1);

        auto check = catalog::open(catalogPath, QStringLiteral("enrich_deep_check"));
        QVERIFY(check);
        QSqlDatabase db = *check;
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT genre, rating FROM games LIMIT 1")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Action"));
        QCOMPARE(q.value(1).toFloat(), 8.5f);
        QVERIFY(q.exec(QStringLiteral("SELECT asset_type, url FROM game_assets ORDER BY asset_type")));
        QMap<QString, QString> assets;
        while (q.next())
            assets.insert(q.value(0).toString(), q.value(1).toString());
        QCOMPARE(assets.value(QStringLiteral("snap")), QStringLiteral("https://example.com/snap.jpg"));
        QCOMPARE(assets.value(QStringLiteral("hero")), QStringLiteral("https://example.com/hero.jpg"));
        QCOMPARE(assets.value(QStringLiteral("banner")), QStringLiteral("https://example.com/banner.jpg"));
        const QString conn = db.connectionName();
        db.close();
        QSqlDatabase::removeDatabase(conn);
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
        QVERIFY(result->wouldFetch >= 0);
    }
};

QTEST_MAIN(LibraryEnrichTest)
#include "test_library_enrich.moc"
