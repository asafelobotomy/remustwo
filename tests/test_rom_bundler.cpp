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
#include "rom_paths.h"

using namespace remustwo;

class RomBundlerTest : public QObject {
    Q_OBJECT

private slots:
    void dryRunListsMarker() {
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

    void discSetPlacedInNamedFolder() {
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

        const QString dest = dir.filePath(QStringLiteral("out"));
        QVERIFY(QDir().mkpath(dest));
        RomBundler bundler(db);
        BundleConfig config;
        config.convert = BundleConvertMode::Never;
        GameMetadata metadata;
        metadata.title = QStringLiteral("Game (USA) (Disc 1)");
        for (const FileRecord &file : db.getAllFiles()) {
            const BundleResult result = bundler.bundle(file, metadata, dest, config);
            QVERIFY2(result.success, qPrintable(result.error));
            QVERIFY(result.skippedDiscSet);
        }

        const QString folder = dest + QStringLiteral("/Game");
        QVERIFY(QFileInfo(folder).isDir());
        QVERIFY(QFile::exists(folder + QStringLiteral("/.remus.md")));
        QVERIFY(QFile::exists(folder + QStringLiteral("/Game (USA) (Disc 1).bin")));
        QVERIFY(QFile::exists(folder + QStringLiteral("/Game (USA) (Disc 2).bin")));
        QVERIFY(!QFile::exists(dest + QStringLiteral("/Game.zip")));
        QCOMPARE(QDir(dest).entryList(QDir::Files).size(), 0);
    }

    void cueAndBinSameDiscStillBundles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Database db;
        QVERIFY(db.initialize(dir.filePath(QStringLiteral("library.db"))));
        const int libId = db.insertLibrary(dir.path(), QStringLiteral("t"));
        const int sysId = db.getSystemId(QStringLiteral("PlayStation"));
        if (sysId <= 0)
            QSKIP("PlayStation system missing");

        const QString cuePath = dir.filePath(QStringLiteral("Game (USA) (Disc 1).cue"));
        const QString binPath = dir.filePath(QStringLiteral("Game (USA) (Disc 1).bin"));
        QFile cue(cuePath);
        QVERIFY(cue.open(QIODevice::WriteOnly | QIODevice::Text));
        cue.write("FILE \"Game (USA) (Disc 1).bin\" BINARY\n");
        cue.close();
        QFile bin(binPath);
        QVERIFY(bin.open(QIODevice::WriteOnly));
        bin.write("disc");
        bin.close();

        FileRecord cueRecord;
        cueRecord.libraryId = libId;
        cueRecord.filename = QStringLiteral("Game (USA) (Disc 1).cue");
        cueRecord.originalPath = cuePath;
        cueRecord.currentPath = cuePath;
        cueRecord.extension = QStringLiteral(".cue");
        cueRecord.systemId = sysId;
        cueRecord.isPrimary = false;
        cueRecord.fileSize = 8;
        QVERIFY(db.insertFile(cueRecord) > 0);

        FileRecord binRecord;
        binRecord.libraryId = libId;
        binRecord.filename = QStringLiteral("Game (USA) (Disc 1).bin");
        binRecord.originalPath = binPath;
        binRecord.currentPath = binPath;
        binRecord.extension = QStringLiteral(".bin");
        binRecord.systemId = sysId;
        binRecord.isPrimary = true;
        binRecord.fileSize = 4;
        const int binId = db.insertFile(binRecord);
        QVERIFY(binId > 0);
        QVERIFY(db.rebuildDiscSetsForLibrary(libId));

        const FileRecord stored = db.getFileById(binId);
        QVERIFY(db.getFilesByDiscSetKey(stored.discSetKey).size() < 2);

        RomBundler bundler(db);
        BundleConfig config;
        config.dryRun = true;
        GameMetadata metadata;
        metadata.title = QStringLiteral("Game");
        const BundleResult result = bundler.bundle(stored, metadata, dir.path(), config);
        QVERIFY(result.success);
        QVERIFY(result.skippedDiscSet);
        QVERIFY(!result.outputPath.endsWith(QStringLiteral(".zip")));
    }

    void singleDiscPs2UsesFolder() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Database db;
        QVERIFY(db.initialize(dir.filePath(QStringLiteral("library.db"))));
        const int libId = db.insertLibrary(dir.path(), QStringLiteral("t"));
        const int sysId = db.getSystemId(QStringLiteral("PlayStation 2"));
        if (sysId <= 0)
            QSKIP("PlayStation 2 system missing");

        const QString isoPath = dir.filePath(QStringLiteral("Simpsons, The - Hit & Run (USA).iso"));
        QFile iso(isoPath);
        QVERIFY(iso.open(QIODevice::WriteOnly));
        iso.write("disc");
        iso.close();

        FileRecord record;
        record.libraryId = libId;
        record.filename = QStringLiteral("Simpsons, The - Hit & Run (USA).iso");
        record.originalPath = isoPath;
        record.currentPath = isoPath;
        record.extension = QStringLiteral(".iso");
        record.systemId = sysId;
        record.isPrimary = true;
        record.fileSize = 4;
        const int fileId = db.insertFile(record);
        QVERIFY(fileId > 0);

        const QString dest = dir.filePath(QStringLiteral("out"));
        QVERIFY(QDir().mkpath(dest));
        RomBundler bundler(db);
        BundleConfig config;
        config.convert = BundleConvertMode::Never;
        GameMetadata metadata;
        metadata.title = QStringLiteral("Simpsons, The - Hit & Run (USA)");
        const BundleResult result = bundler.bundle(db.getFileById(fileId), metadata, dest, config);
        QVERIFY2(result.success, qPrintable(result.error));
        QVERIFY(result.skippedDiscSet);
        QVERIFY(QFileInfo(result.outputPath).isDir());
        QVERIFY(QFile::exists(result.outputPath + QStringLiteral("/Simpsons, The - Hit & Run (USA).iso")));
        QVERIFY(QFile::exists(result.outputPath + QStringLiteral("/.remus.md")));
        QVERIFY(!QFile::exists(dest + QStringLiteral("/Simpsons, The - Hit & Run (USA).zip")));
        QCOMPARE(db.getFileById(fileId).currentPath,
            result.outputPath + QStringLiteral("/Simpsons, The - Hit & Run (USA).iso"));
    }
};

QTEST_MAIN(RomBundlerTest)
#include "test_rom_bundler.moc"
