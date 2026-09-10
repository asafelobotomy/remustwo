#include <QtTest/QtTest>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_match.h"
#include "../src/metadata/catalog_fetch.h"
#include "rom_paths.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

class CatalogFetchTest : public QObject {
    Q_OBJECT

private slots:
    void redumpSlugMapsCommonSystems();
    void redumpDatUrlUsesSlug();
    void fromDirIngestsFixtureDat();
    void extractDatFromZipRoundTrip();
};

void CatalogFetchTest::redumpSlugMapsCommonSystems() {
    QCOMPARE(remustwo::redumpSlugForSystem(QStringLiteral("psx")), QStringLiteral("psx"));
    QCOMPARE(remustwo::redumpSlugForSystem(QStringLiteral("PlayStation")), QStringLiteral("psx"));
    QCOMPARE(remustwo::redumpSlugForSystem(QStringLiteral("PS2")), QStringLiteral("ps2"));
    QCOMPARE(remustwo::redumpSlugForSystem(QStringLiteral("Dreamcast")), QStringLiteral("dc"));
    QCOMPARE(remustwo::redumpSlugForSystem(QStringLiteral("no-such-system")), QString());
}

void CatalogFetchTest::redumpDatUrlUsesSlug() {
    QCOMPARE(remustwo::redumpDatUrl(QStringLiteral("psx")).toString(),
        QStringLiteral("http://redump.org/datfile/psx/"));
}

void CatalogFetchTest::fromDirIngestsFixtureDat() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString catalogPath = temp.path() + QStringLiteral("/catalog.db");
    QVERIFY(remustwo::catalog::init(catalogPath));

    const QString datDir = temp.path() + QStringLiteral("/dats");
    QVERIFY(QDir().mkpath(datDir));
    const QString fixtureDat = remustwo::test::syntheticFixtureDat();
    QVERIFY(QFile::copy(fixtureDat, datDir + QStringLiteral("/fixture.dat")));

    remustwo::CatalogFetchOptions options;
    options.fromDir = datDir;
    auto result = remustwo::fetchCatalog(catalogPath, options);
    QVERIFY2(result, qPrintable(result ? QString() : result.error()));
    QCOMPARE(result->ingested, 1);

    const QString romPath = remustwo::test::syntheticFixtureRom();
    auto match = remustwo::catalog::matchFile(catalogPath, romPath);
    QVERIFY2(match, qPrintable(match ? QString() : match.error()));
    QVERIFY(!match->gameId.isEmpty());
}

void CatalogFetchTest::extractDatFromZipRoundTrip() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString fixtureDat = remustwo::test::syntheticFixtureDat();
    const QString zipPath = temp.path() + QStringLiteral("/pack.zip");

    QProcess zip;
    zip.start(QStringLiteral("python3"),
        { QStringLiteral("-c"),
            QStringLiteral(
                "import zipfile,sys; z=zipfile.ZipFile(sys.argv[1],'w'); z.write(sys.argv[2], 'fixture.dat'); z.close()"),
            zipPath, fixtureDat });
    QVERIFY(zip.waitForFinished(30000));
    QCOMPARE(zip.exitCode(), 0);
    QVERIFY(QFileInfo::exists(zipPath));

    const QString extractDir = temp.path() + QStringLiteral("/out");
    auto extracted = remustwo::extractDatFromZip(zipPath, extractDir);
    QVERIFY2(extracted, qPrintable(extracted ? QString() : extracted.error()));
    QVERIFY(QFileInfo::exists(*extracted));
    QVERIFY(extracted->endsWith(QStringLiteral(".dat"), Qt::CaseInsensitive));
}

QTEST_MAIN(CatalogFetchTest)
#include "test_catalog_fetch.moc"
