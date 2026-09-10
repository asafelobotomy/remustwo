#include <QtTest/QtTest>

#include "../src/metadata/hasheous_provider.h"
#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>

class HasheousImportTest : public QObject {
    Q_OBJECT

private slots:
    void importJsonUpdatesCoverUrl();
};

void HasheousImportTest::importJsonUpdatesCoverUrl() {
    const QString dbPath = QDir::tempPath() + QStringLiteral("/remustwo_hasheous_test.db");
    QFile::remove(dbPath);

    auto initResult = remustwo::catalog::init(dbPath);
    QVERIFY(initResult);

    const QString datPath = QString(REMUSTWO_SOURCE_DIR) + QStringLiteral("/testdata/fixture.dat");
    auto ingestResult = remustwo::catalog::ingestDat(dbPath, datPath);
    QVERIFY(ingestResult);

    const QString jsonPath = QString(REMUSTWO_SOURCE_DIR) + QStringLiteral("/testdata/hasheous_fixture.json");
    auto importResult = remustwo::importHasheousJson(dbPath, jsonPath);
    QVERIFY(importResult);
    QCOMPARE(*importResult, 1);

    auto dbResult = remustwo::catalog::open(dbPath, QStringLiteral("hasheous_test_read"));
    QVERIFY(dbResult);
    QSqlDatabase database = *dbResult;
    QSqlQuery query(database);
    QVERIFY(query.exec(QStringLiteral(
        "SELECT cover_url, description, developer, publisher, release_date FROM games LIMIT 1")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("https://example.com/fixture-cover.jpg"));
    QCOMPARE(query.value(1).toString(), QStringLiteral("Synthetic fixture enrichment entry"));
    QCOMPARE(query.value(2).toString(), QStringLiteral("Fixture Dev"));
    QCOMPARE(query.value(3).toString(), QStringLiteral("Fixture Pub"));
    QCOMPARE(query.value(4).toString(), QStringLiteral("1990-01-01"));
    QVERIFY(query.exec(QStringLiteral(
        "SELECT url FROM game_assets WHERE asset_type = 'boxart' LIMIT 1")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("https://example.com/fixture-cover.jpg"));
    database.close();
    QSqlDatabase::removeDatabase(database.connectionName());
    QFile::remove(dbPath);
}

QTEST_MAIN(HasheousImportTest)
#include "test_hasheous_import.moc"
