#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QIODevice>

#include "../src/catalog/catalog.h"
#include "../src/catalog/catalog_ingest.h"
#include "../src/catalog/catalog_match.h"
#include "../src/core/database.h"
#include "../src/core/rom_bundler.h"

using namespace remustwo;

class RomBundlerTest : public QObject {
    Q_OBJECT

private slots:
    void dryRunListsMarker() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString catalogPath = dir.filePath(QStringLiteral("catalog.db"));
        const QString libraryPath = dir.filePath(QStringLiteral("library.db"));
        const QString datPath = QStringLiteral(REMUSTWO_SOURCE_DIR) + QStringLiteral("/testdata/fixture.dat");
        const QString romSource = QStringLiteral(REMUSTWO_SOURCE_DIR) + QStringLiteral("/testdata/fixture.bin");
        const QString romPath = dir.filePath(QStringLiteral("fixture.bin"));
        QVERIFY(QFile::copy(romSource, romPath));
        QVERIFY(catalog::init(catalogPath));
        QVERIFY(catalog::ingestDat(catalogPath, datPath));
        QVERIFY(catalog::matchFile(catalogPath, romPath, libraryPath));

        Database library;
        QVERIFY(library.initialize(libraryPath));
        const auto files = library.getFilesEligibleForOrganize();
        QCOMPARE(files.size(), 1);

        RomBundler bundler(library);
        BundleConfig config;
        config.dryRun = true;
        config.convert = BundleConvertMode::Auto;
        GameMetadata metadata;
        metadata.title = files.first().baseTitle;
        metadata.system = QStringLiteral("NES");
        const BundleResult result
            = bundler.bundle(files.first(), metadata, dir.filePath(QStringLiteral("out")), config);
        QVERIFY2(result.success, qPrintable(result.error));
        QVERIFY(result.archiveEntries.contains(QStringLiteral(".remus.md")));
        QVERIFY(!result.outputPath.isEmpty());
        bool hasCartridgeRom = false;
        for (const QString &entry : result.archiveEntries) {
            if (entry.endsWith(QStringLiteral(".bin")) || entry.endsWith(QStringLiteral(".nes")))
                hasCartridgeRom = true;
        }
        QVERIFY2(hasCartridgeRom, qPrintable(result.archiveEntries.join(QLatin1Char(','))));
    }

    void skipsDiscSetZip() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Database db;
        QVERIFY(db.initialize(dir.filePath(QStringLiteral("library.db"))));
        const int libId = db.insertLibrary(dir.path(), QStringLiteral("t"));
        const int sysId = db.getSystemId(QStringLiteral("PlayStation"));
        if (sysId <= 0)
            QSKIP("PlayStation system missing");

        const auto insertDisc = [&](const QString &name) {
            const QString path = dir.filePath(name);
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly))
                return 0;
            file.write("disc");
            file.close();
            FileRecord record;
            record.libraryId = libId;
            record.filename = name;
            record.originalPath = path;
            record.currentPath = path;
            record.extension = QStringLiteral(".bin");
            record.systemId = sysId;
            record.fileSize = 4;
            return db.insertFile(record);
        };
        QVERIFY(insertDisc(QStringLiteral("Game (USA) (Disc 1).bin")) > 0);
        QVERIFY(insertDisc(QStringLiteral("Game (USA) (Disc 2).bin")) > 0);
        QVERIFY(db.rebuildDiscSetsForLibrary(libId));
        const auto files = db.getAllFiles();
        QVERIFY(files.size() >= 2);
        QVERIFY(!files.first().discSetKey.isEmpty());

        RomBundler bundler(db);
        BundleConfig config;
        config.dryRun = true;
        GameMetadata metadata;
        metadata.title = QStringLiteral("Game");
        const BundleResult result = bundler.bundle(files.first(), metadata, dir.path(), config);
        QVERIFY(result.success);
        QVERIFY(result.skippedDiscSet);
    }
};

QTEST_MAIN(RomBundlerTest)
#include "test_rom_bundler.moc"
