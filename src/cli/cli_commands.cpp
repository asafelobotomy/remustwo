#include "../catalog/catalog.h"
#include "../catalog/catalog_ingest.h"
#include "../catalog/catalog_match.h"
#include "../core/database.h"
#include "../core/hasher.h"
#include "../core/scanner.h"
#include "../core/organize_engine.h"
#include "../core/template_engine.h"
#include "../core/verification_engine.h"
#include "../metadata/artwork_cache.h"
#include "../metadata/hasheous_provider.h"
#include "cli_common.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace remustwo::cli {

namespace {

int printError(const QString &message) {
    QTextStream(stderr) << message << '\n';
    return 1;
}

int cmdHash(const QStringList &args, bool json) {
    if (args.isEmpty()) {
        return printError(QStringLiteral("hash: missing PATH"));
    }
    Hasher hasher;
    const HashResult result = hasher.calculateHashes(args.at(0));
    if (!result.success) {
        return printError(result.error);
    }
    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("crc32"), result.crc32);
        obj.insert(QStringLiteral("md5"), result.md5);
        obj.insert(QStringLiteral("sha1"), result.sha1);
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << result.crc32 << ' ' << result.md5 << ' ' << result.sha1 << '\n';
    }
    return 0;
}

QString catalogDbPath(const QCommandLineParser &parser) {
    return parser.value(QStringLiteral("db")).isEmpty() ? defaultCatalogPath() : parser.value(QStringLiteral("db"));
}

int cmdCatalogInit(const QCommandLineParser &parser, bool json) {
    const QString dbPath = catalogDbPath(parser);
    auto result = catalog::init(dbPath);
    if (!result) {
        return printError(result.error());
    }
    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("status"), QStringLiteral("ok"));
        obj.insert(QStringLiteral("catalog"), dbPath);
        obj.insert(QStringLiteral("note"), QStringLiteral("Schema only — cannot match until catalog ingest"));
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << "Catalog initialized: " << dbPath << '\n';
        QTextStream(stdout) << "Note: init creates schema only. Run 'catalog ingest' before match.\n";
    }
    return 0;
}

int cmdCatalogImportHasheous(const QStringList &args, const QCommandLineParser &parser, bool json) {
    if (args.isEmpty()) {
        return printError(QStringLiteral("catalog import-hasheous: missing JSON"));
    }
    auto result = importHasheousJson(catalogDbPath(parser), args.at(0));
    if (!result) {
        return printError(result.error());
    }
    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("status"), QStringLiteral("ok"));
        obj.insert(QStringLiteral("updated"), *result);
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << "Updated " << *result << " game(s) from Hasheous JSON\n";
    }
    return 0;
}

int cmdCatalogEnrich(const QCommandLineParser &parser, bool json) {
    const bool online = parser.isSet(QStringLiteral("online"));
    auto result = enrichCatalogFromHasheous(catalogDbPath(parser), online);
    if (!result) {
        return printError(result.error());
    }
    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("status"), QStringLiteral("ok"));
        obj.insert(QStringLiteral("updated"), *result);
        obj.insert(QStringLiteral("online"), online);
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << "Enriched " << *result << " game(s)" << (online ? " (online)" : "") << '\n';
    }
    return 0;
}

int cmdCatalogIngest(const QStringList &args, const QCommandLineParser &parser, bool json) {
    if (args.isEmpty()) {
        return printError(QStringLiteral("catalog ingest: missing DAT"));
    }
    auto result = catalog::ingestDat(catalogDbPath(parser), args.at(0));
    if (!result) {
        return printError(result.error());
    }
    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("status"), QStringLiteral("ok"));
        obj.insert(QStringLiteral("dat"), args.at(0));
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << "Ingested " << args.at(0) << '\n';
    }
    return 0;
}

int cmdScan(const QStringList &args, const QCommandLineParser &parser, bool json) {
    if (args.isEmpty()) {
        return printError(QStringLiteral("scan: missing PATH"));
    }
    const QString scanPath = args.at(0);
    const QString libraryPath = parser.value(QStringLiteral("library-db")).isEmpty()
        ? defaultLibraryPath()
        : parser.value(QStringLiteral("library-db"));
    const QString catalogPath = parser.value(QStringLiteral("catalog-db")).isEmpty()
        ? catalogDbPath(parser)
        : parser.value(QStringLiteral("catalog-db"));

    Scanner scanner;
    scanner.setArchiveScanning(false);
    const QList<ScanResult> results = scanner.scan(scanPath);

    Database db;
    if (!db.initialize(libraryPath)) {
        return printError(QStringLiteral("Failed to open library database"));
    }
    const int libraryId = db.insertLibrary(scanPath, QStringLiteral("scan"));
    int stored = 0;
    for (const ScanResult &item : results) {
        FileRecord record;
        record.libraryId = libraryId;
        record.originalPath = item.path;
        record.currentPath = item.path;
        record.filename = item.filename;
        record.extension = item.extension;
        record.fileSize = item.fileSize;
        record.lastModified = item.lastModified;
        if (db.insertFile(record) > 0)
            ++stored;
    }

    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("scanned"), results.size());
        obj.insert(QStringLiteral("stored"), stored);
        obj.insert(QStringLiteral("catalog"), catalogPath);
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << "Scanned " << results.size() << " file(s), stored " << stored << '\n';
    }
    return 0;
}

int cmdMatch(const QStringList &args, const QCommandLineParser &parser, bool json) {
    if (args.isEmpty()) {
        return printError(QStringLiteral("match: missing PATH"));
    }
    const QString dbPath = parser.value(QStringLiteral("catalog-db")).isEmpty()
        ? catalogDbPath(parser)
        : parser.value(QStringLiteral("catalog-db"));
    auto result = catalog::matchFile(dbPath, args.at(0),
        parser.value(QStringLiteral("library-db")).isEmpty() ? defaultLibraryPath()
                                                             : parser.value(QStringLiteral("library-db")));
    if (!result) {
        return printError(result.error());
    }
    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("game_id"), result->gameId);
        obj.insert(QStringLiteral("title"), result->title);
        obj.insert(QStringLiteral("confidence"), result->confidence);
        obj.insert(QStringLiteral("hash"), result->matchedHash);
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << result->title << " (" << result->confidence << "%)\n";
    }
    return 0;
}

int cmdOrganize(const QStringList &args, const QCommandLineParser &parser, bool json) {
    if (args.isEmpty()) {
        return printError(QStringLiteral("organize: missing DEST"));
    }
    const QString dest = args.at(0);
    const bool dryRun = parser.isSet(QStringLiteral("dry-run"));
    const QString libraryPath = parser.value(QStringLiteral("library-db")).isEmpty()
        ? defaultLibraryPath()
        : parser.value(QStringLiteral("library-db"));
    const QString catalogPath = parser.value(QStringLiteral("catalog-db")).isEmpty()
        ? catalogDbPath(parser)
        : parser.value(QStringLiteral("catalog-db"));

    Database db;
    if (!db.initialize(libraryPath)) {
        return printError(QStringLiteral("Failed to open library database"));
    }
    db.setCompendiumDbPath(catalogPath);

    OrganizeEngine engine(db);
    engine.setDryRun(dryRun);
    engine.setTemplate(TemplateEngine::getNoIntroTemplate());

    const auto files = db.getAllFiles();
    QList<int> fileIds;
    QMap<int, GameMetadata> metadataMap;
    for (const FileRecord &file : files) {
        fileIds.append(file.id);
        GameMetadata meta;
        meta.title = file.baseTitle.isEmpty() ? QFileInfo(file.filename).completeBaseName() : file.baseTitle;
        meta.system = db.getSystemDisplayName(file.systemId);
        metadataMap.insert(file.id, meta);
    }

    const QList<OrganizeResult> results = engine.organizeFiles(fileIds, metadataMap, dest, FileOperation::Move);
    int ok = 0;
    int failed = 0;
    for (const OrganizeResult &result : results) {
        if (result.success)
            ++ok;
        else
            ++failed;
    }

    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("dest"), dest);
        obj.insert(QStringLiteral("dry_run"), dryRun);
        obj.insert(QStringLiteral("organized"), ok);
        obj.insert(QStringLiteral("failed"), failed);
        obj.insert(QStringLiteral("status"), failed == 0 ? QStringLiteral("ok") : QStringLiteral("partial"));
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << (dryRun ? "[dry-run] " : "") << "organized " << ok << " file(s) into " << dest;
        if (failed > 0)
            QTextStream(stdout) << " (" << failed << " failed)";
        QTextStream(stdout) << '\n';
    }
    return failed > 0 ? 1 : 0;
}

int cmdVerify(const QCommandLineParser &parser, bool json) {
    const QString libraryPath = parser.value(QStringLiteral("library-db")).isEmpty()
        ? defaultLibraryPath()
        : parser.value(QStringLiteral("library-db"));
    const QString catalogPath = parser.value(QStringLiteral("catalog-db")).isEmpty()
        ? catalogDbPath(parser)
        : parser.value(QStringLiteral("catalog-db"));

    Database db;
    if (!db.initialize(libraryPath)) {
        if (json) {
            QJsonObject obj;
            obj.insert(QStringLiteral("status"), QStringLiteral("ok"));
            obj.insert(QStringLiteral("files"), 0);
            QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
        } else {
            QTextStream(stdout) << "verify: no library database yet (ok)\n";
        }
        return 0;
    }

    VerificationEngine engine(&db);
    engine.setCompendiumDb(catalogPath);
    const QList<VerificationResult> results = engine.verifyLibrary();
    const VerificationSummary summary = engine.getLastSummary();

    if (json) {
        QJsonObject obj;
        obj.insert(QStringLiteral("status"), QStringLiteral("ok"));
        obj.insert(QStringLiteral("total"), summary.totalFiles);
        obj.insert(QStringLiteral("verified"), summary.verified);
        obj.insert(QStringLiteral("mismatched"), summary.mismatched);
        obj.insert(QStringLiteral("not_in_dat"), summary.notInDat);
        obj.insert(QStringLiteral("no_hash"), summary.noHash);
        obj.insert(QStringLiteral("corrupt"), summary.corrupt);
        obj.insert(QStringLiteral("results"), static_cast<int>(results.size()));
        QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
    } else {
        QTextStream(stdout) << "verify: " << summary.totalFiles << " file(s), " << summary.verified << " verified, "
                            << summary.mismatched << " mismatched\n";
    }
    return 0;
}

int cmdList(const QCommandLineParser &parser, bool json) {
    const QString dbPath = parser.value(QStringLiteral("library-db")).isEmpty()
        ? defaultLibraryPath()
        : parser.value(QStringLiteral("library-db"));

    Database db;
    if (!db.initialize(dbPath)) {
        return printError(QStringLiteral("Failed to open library database"));
    }

    const auto files = db.getAllFiles();
    if (json) {
        QJsonArray arr;
        for (const FileRecord &file : files) {
            QJsonObject obj;
            obj.insert(QStringLiteral("path"), file.currentPath);
            obj.insert(QStringLiteral("filename"), file.filename);
            arr.append(obj);
        }
        QJsonObject root;
        root.insert(QStringLiteral("files"), arr);
        QTextStream(stdout) << QJsonDocument(root).toJson(QJsonDocument::Compact) << '\n';
    } else {
        for (const FileRecord &file : files) {
            QTextStream(stdout) << file.currentPath << '\n';
        }
    }
    return 0;
}

void printRootHelp() {
    QTextStream out(stdout);
    out << "Usage: remustwo <command> [args]\n\n";
    out << "Commands:\n";
    out << "  hash PATH                      Print file hashes\n";
    out << "  catalog init [--db PATH]       Create catalog schema (cannot match)\n";
    out << "  catalog ingest DAT [--db PATH] Ingest a DAT into the catalog\n";
    out << "  catalog import-hasheous JSON     Import offline Hasheous JSON enrichment\n";
    out << "  catalog enrich [--online]        Enrich catalog metadata (default offline)\n";
    out << "  match PATH [--catalog-db PATH] Match a file against the catalog\n";
    out << "  scan PATH                      Scan a directory into the library\n";
    out << "  list [--library-db PATH]       List library files\n";
    out << "  organize DEST [--dry-run]      Organize matched files\n";
    out << "  verify                         Verify library integrity\n";
    out << "\nGlobal options: --json, --online\n";
}

QStringList positionalArgs(const QCommandLineParser &parser) {
    QStringList args = parser.positionalArguments();
    for (int i = args.size() - 1; i >= 0; --i) {
        if (args.at(i).startsWith(QLatin1Char('-'))) {
            args.removeAt(i);
        }
    }
    return args;
}

} // namespace

int run(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("remustwo"));
    QCoreApplication::setOrganizationName(QStringLiteral("remustwo"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.4.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Offline ROM catalog and library manager."));
    parser.addHelpOption();
    parser.addOption({ { QStringLiteral("j"), QStringLiteral("json") }, QStringLiteral("JSON output") });
    parser.addOption({ QStringLiteral("db"), QStringLiteral("Catalog database path"), QStringLiteral("path") });
    parser.addOption({ QStringLiteral("catalog-db"), QStringLiteral("Catalog database path"), QStringLiteral("path") });
    parser.addOption({ QStringLiteral("library-db"), QStringLiteral("Library database path"), QStringLiteral("path") });
    parser.addOption({ QStringLiteral("dry-run"), QStringLiteral("Dry run (no writes)") });
    parser.addOption({ QStringLiteral("online"), QStringLiteral("Enable online metadata/artwork fetch") });
    parser.addPositionalArgument(QStringLiteral("command"), QStringLiteral("Command and arguments"));
    parser.process(app);

    if (parser.isSet(QStringLiteral("help"))) {
        printRootHelp();
        return 0;
    }

    const QStringList positional = positionalArgs(parser);
    if (positional.isEmpty()) {
        printRootHelp();
        return 0;
    }

    const bool json = parser.isSet(QStringLiteral("json"));
    const QString command = positional.first();

    if (command == QStringLiteral("hash")) {
        return cmdHash(positional.mid(1), json);
    }
    if (command == QStringLiteral("catalog")) {
        if (positional.size() < 2) {
            return printError(QStringLiteral("catalog: missing subcommand (init|ingest|import-hasheous|enrich)"));
        }
        const QString sub = positional.at(1);
        if (sub == QStringLiteral("init")) {
            return cmdCatalogInit(parser, json);
        }
        if (sub == QStringLiteral("ingest")) {
            return cmdCatalogIngest(positional.mid(2), parser, json);
        }
        if (sub == QStringLiteral("import-hasheous")) {
            return cmdCatalogImportHasheous(positional.mid(2), parser, json);
        }
        if (sub == QStringLiteral("enrich")) {
            return cmdCatalogEnrich(parser, json);
        }
        return printError(QStringLiteral("Unknown catalog subcommand: ") + sub);
    }
    if (command == QStringLiteral("match")) {
        return cmdMatch(positional.mid(1), parser, json);
    }
    if (command == QStringLiteral("scan")) {
        return cmdScan(positional.mid(1), parser, json);
    }
    if (command == QStringLiteral("organize")) {
        return cmdOrganize(positional.mid(1), parser, json);
    }
    if (command == QStringLiteral("verify")) {
        return cmdVerify(parser, json);
    }
    if (command == QStringLiteral("list")) {
        return cmdList(parser, json);
    }

    return printError(QStringLiteral("Unknown command: ") + command);
}

} // namespace remustwo::cli
