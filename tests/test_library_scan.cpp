#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QProcess>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/database.h"
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

    void scanPersistsCueBinPrimaryFlag() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString romDir = dir.filePath(QStringLiteral("roms"));
        QVERIFY(QDir().mkpath(romDir));
        QFile cue(romDir + QStringLiteral("/Metal Gear Solid (USA) (Disc 1).cue"));
        QVERIFY(cue.open(QIODevice::WriteOnly | QIODevice::Text));
        cue.write("FILE \"Metal Gear Solid (USA) (Disc 1).bin\" BINARY\n"
                  "  TRACK 01 MODE2/2352\n"
                  "    INDEX 01 00:00:00\n");
        cue.close();
        QFile bin(romDir + QStringLiteral("/Metal Gear Solid (USA) (Disc 1).bin"));
        QVERIFY(bin.open(QIODevice::WriteOnly));
        bin.write(QByteArray(64, 'B'));
        bin.close();

        const QString libraryPath = dir.filePath(QStringLiteral("library.db"));
        auto result = scanLibrary(romDir, libraryPath);
        QVERIFY2(result, qPrintable(result.error()));
        QCOMPARE(result->scanned, 2);
        QCOMPARE(result->stored, 2);

        Database db;
        QVERIFY(db.initialize(libraryPath));
        bool sawCue = false;
        bool sawBin = false;
        for (const FileRecord &file : db.getAllFiles()) {
            if (file.extension == QLatin1String(".cue")) {
                sawCue = true;
                QVERIFY(!file.isPrimary);
                QVERIFY(file.parentFileId > 0);
            } else if (file.extension == QLatin1String(".bin")) {
                sawBin = true;
                QVERIFY(file.isPrimary);
            }
        }
        QVERIFY(sawCue);
        QVERIFY(sawBin);
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

#ifdef REMUSTWO_HAS_LIBARCHIVE
    void scanHashesRomInsideZip() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString romDir = dir.filePath(QStringLiteral("roms"));
        QVERIFY(QDir().mkpath(romDir));
        const QString romSource = test::syntheticFixtureRom();
        const QString zipPath = romDir + QStringLiteral("/pack.zip");

        QProcess zip;
        zip.start(QStringLiteral("python3"),
            { QStringLiteral("-c"),
                QStringLiteral(
                    "import zipfile,sys; z=zipfile.ZipFile(sys.argv[1],'w'); z.write(sys.argv[2],'fixture.bin'); z.close()"),
                zipPath, romSource });
        QVERIFY(zip.waitForFinished(30000));
        QCOMPARE(zip.exitCode(), 0);

        const QString catalogPath = dir.filePath(QStringLiteral("catalog.db"));
        const QString libraryPath = dir.filePath(QStringLiteral("library.db"));
        QVERIFY(catalog::init(catalogPath));
        QVERIFY(catalog::ingestDat(catalogPath, test::syntheticFixtureDat()));

        auto scanned = scanLibrary(romDir, libraryPath);
        QVERIFY2(scanned, qPrintable(scanned.error()));
        QVERIFY(scanned->scanned >= 1);
        QVERIFY(scanned->hashed >= 1);

        auto matched = catalog::matchLibrary(catalogPath, libraryPath);
        QVERIFY(matched);
        QCOMPARE(*matched, 1);
    }
#endif
};

QTEST_MAIN(LibraryScanTest)
#include "test_library_scan.moc"
