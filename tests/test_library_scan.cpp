#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/library_scan.h"
#include "../src/metadata/library_organize.h"
#include "rom_paths.h"

using namespace remustwo;

class LibraryScanTest : public QObject {
    Q_OBJECT

private slots:
    void scanStoresAndHashesFixture() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString romDir = dir.filePath(QStringLiteral("roms"));
        QVERIFY(QDir().mkpath(romDir));
        const QString romSource = test::syntheticFixtureRom();
        QVERIFY(QFile::copy(romSource, romDir + QStringLiteral("/fixture.bin")));

        const QString libraryPath = dir.filePath(QStringLiteral("library.db"));
        auto result = scanLibrary(romDir, libraryPath);
        QVERIFY2(result, qPrintable(result.error()));
        QCOMPARE(result->scanned, 1);
        QCOMPARE(result->stored, 1);
        QCOMPARE(result->hashed, 1);
    }

    void scanRejectsFilePath() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString filePath = dir.filePath(QStringLiteral("not-a-dir.bin"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        auto result = scanLibrary(filePath, dir.filePath(QStringLiteral("library.db")));
        QVERIFY(!result);
    }

    void organizeDryRunBundleAfterMatch() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString catalogPath = dir.filePath(QStringLiteral("catalog.db"));
        const QString libraryPath = dir.filePath(QStringLiteral("library.db"));
        const QString dest = dir.filePath(QStringLiteral("out"));
        QVERIFY(QDir().mkpath(dest));
        const QString datPath = test::syntheticFixtureDat();
        const QString romSource = test::syntheticFixtureRom();
        const QString romDir = dir.filePath(QStringLiteral("roms"));
        QVERIFY(QDir().mkpath(romDir));
        QVERIFY(QFile::copy(romSource, romDir + QStringLiteral("/fixture.bin")));
        QVERIFY(catalog::init(catalogPath));
        QVERIFY(catalog::ingestDat(catalogPath, datPath));
        QVERIFY(scanLibrary(romDir, libraryPath));
        QVERIFY(catalog::matchLibrary(catalogPath, libraryPath));

        OrganizeLibraryOptions options;
        options.dryRun = true;
        options.bundle = true;
        auto result = organizeLibrary(catalogPath, libraryPath, dest, options);
        QVERIFY2(result, qPrintable(result.error()));
        QCOMPARE(result->organized, 1);
        QCOMPARE(result->failed, 0);
        QVERIFY(result->archiveEntries.contains(QStringLiteral(".remus.md")));
    }
};

QTEST_MAIN(LibraryScanTest)
#include "test_library_scan.moc"
