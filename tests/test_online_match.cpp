#include <QtTest/QtTest>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/database.h"
#include "../src/core/library_scan.h"
#include "rom_paths.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

class OnlineMatchTest : public QObject {
    Q_OBJECT

private slots:
    void onlineLookupUpsertsAndMatches();
    void onlineMissIsNegativeCached();
};

void OnlineMatchTest::onlineLookupUpsertsAndMatches() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString catalogPath = temp.path() + QStringLiteral("/catalog.db");
    const QString libraryPath = temp.path() + QStringLiteral("/library.db");
    QVERIFY(remustwo::catalog::init(catalogPath));

    const QString romDir = temp.path() + QStringLiteral("/roms");
    QVERIFY(QDir().mkpath(romDir));
    const QString romPath = romDir + QStringLiteral("/fixture.bin");
    QVERIFY(QFile::copy(remustwo::test::syntheticFixtureRom(), romPath));

    auto scanned = remustwo::scanLibrary(romDir, libraryPath);
    QVERIFY(scanned);

    remustwo::catalog::MatchOptions options;
    options.online = true;
    options.lookup = [](const QString &, const QString &, const QString &) {
        remustwo::catalog::CatalogOnlineHit hit;
        hit.title = QStringLiteral("Online Fixture Title");
        hit.coverUrl = QStringLiteral("https://example.com/cover.jpg");
        hit.description = QStringLiteral("From mock Hasheous");
        hit.externalId = QStringLiteral("999");
        return hit;
    };

    auto matched = remustwo::catalog::matchLibrary(catalogPath, libraryPath, options);
    QVERIFY2(matched, qPrintable(matched ? QString() : matched.error()));
    QCOMPARE(matched->matched, 1);
    QCOMPARE(matched->onlineMatched, 1);

    remustwo::Database library;
    QVERIFY(library.initialize(libraryPath));
    const auto files = library.getAllFiles();
    QCOMPARE(files.size(), 1);
    QCOMPARE(files.first().catalogGameId, QStringLiteral("hasheous:999"));
    QVERIFY(library.getMatchForFile(files.first().id).isConfirmed);
}

void OnlineMatchTest::onlineMissIsNegativeCached() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString catalogPath = temp.path() + QStringLiteral("/catalog.db");
    const QString libraryPath = temp.path() + QStringLiteral("/library.db");
    QVERIFY(remustwo::catalog::init(catalogPath));

    const QString romDir = temp.path() + QStringLiteral("/roms");
    QVERIFY(QDir().mkpath(romDir));
    QVERIFY(QFile::copy(remustwo::test::syntheticFixtureRom(), romDir + QStringLiteral("/fixture.bin")));
    QVERIFY(remustwo::scanLibrary(romDir, libraryPath));

    remustwo::catalog::MatchOptions options;
    options.online = true;
    int calls = 0;
    options.lookup = [&](const QString &, const QString &, const QString &) {
        ++calls;
        return remustwo::catalog::CatalogOnlineHit {};
    };

    auto first = remustwo::catalog::matchLibrary(catalogPath, libraryPath, options);
    QVERIFY(first);
    QCOMPARE(first->onlineMisses, 1);
    QCOMPARE(calls, 1);

    auto second = remustwo::catalog::matchLibrary(catalogPath, libraryPath, options);
    QVERIFY(second);
    QCOMPARE(second->negativeCached, 1);
    QCOMPARE(calls, 1);
}

QTEST_MAIN(OnlineMatchTest)
#include "test_online_match.moc"
