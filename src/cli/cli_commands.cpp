#include "../catalog/catalog.h"
#include "../catalog/catalog_ingest.h"
#include "../catalog/catalog_match.h"
#include "../core/database.h"
#include "../core/hasher.h"
#include "../core/library_scan.h"
#include "../core/organize_engine.h"
#include "../core/patch_engine.h"
#include "../core/verification_engine.h"
#include "../core/constants/api.h"
#include "../core/constants/constants.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/catalog_fetch.h"
#include "../metadata/library_enrich.h"
#include "../metadata/library_organize.h"
#include "cli_common.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
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
        if (!parser.value(QStringLiteral("catalog-db")).isEmpty()) {
            return parser.value(QStringLiteral("catalog-db"));
        }
        if (!parser.value(QStringLiteral("db")).isEmpty()) {
            return parser.value(QStringLiteral("db"));
        }
        return defaultCatalogPath();
    }

    QString libraryDbPath(const QCommandLineParser &parser) {
        return parser.value(QStringLiteral("library-db")).isEmpty() ? defaultLibraryPath()
                                                                    : parser.value(QStringLiteral("library-db"));
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

    int cmdCatalogEnrich() {
        return printError(QStringLiteral("catalog enrich was removed. Run 'enrich' after match."));
    }

    int cmdEnrich(const QCommandLineParser &parser, bool json) {
        EnrichOptions options;
        options.online = parser.isSet(QStringLiteral("online"));
        options.deep = parser.isSet(QStringLiteral("deep"));
        options.dryRun = parser.isSet(QStringLiteral("dry-run"));
        auto result = enrichLibrary(catalogDbPath(parser), libraryDbPath(parser), options);
        if (!result) {
            return printError(result.error());
        }
        if (json) {
            QJsonObject obj;
            obj.insert(QStringLiteral("matched_games"), result->matchedGames);
            obj.insert(QStringLiteral("complete"), result->complete);
            obj.insert(QStringLiteral("negative_cached"), result->negativeCached);
            obj.insert(QStringLiteral("would_fetch"), result->wouldFetch);
            obj.insert(QStringLiteral("fetched"), result->fetched);
            obj.insert(QStringLiteral("proxy_fetched"), result->proxyFetched);
            obj.insert(QStringLiteral("updated"), result->updated);
            obj.insert(QStringLiteral("thumbnail_urls"), result->thumbnailUrls);
            obj.insert(QStringLiteral("assets_written"), result->assetsWritten);
            obj.insert(QStringLiteral("dry_run"), options.dryRun);
            obj.insert(QStringLiteral("online"), options.online);
            obj.insert(QStringLiteral("deep"), options.deep);
            QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
        } else {
            QTextStream(stdout) << (options.dryRun ? QStringLiteral("would_fetch ") : QStringLiteral("fetched "))
                                << (options.dryRun ? result->wouldFetch : result->fetched) << " of "
                                << result->matchedGames << " matched (" << result->complete << " complete, "
                                << result->negativeCached << " negative-cached)\n";
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

    int cmdCatalogFetch(const QCommandLineParser &parser, bool json) {
        CatalogFetchOptions options;
        options.system = parser.value(QStringLiteral("system"));
        options.fromDir = parser.value(QStringLiteral("from-dir"));
        options.dryRun = parser.isSet(QStringLiteral("dry-run"));
        if (options.system.isEmpty() && options.fromDir.isEmpty()) {
            return printError(QStringLiteral(
                "catalog fetch: require --system NAME|slug or --from-dir DIR (No-Intro / local DATs)"));
        }
        auto result = fetchCatalog(catalogDbPath(parser), options);
        if (!result) {
            return printError(result.error());
        }
        if (json) {
            QJsonObject obj;
            obj.insert(QStringLiteral("status"), QStringLiteral("ok"));
            obj.insert(QStringLiteral("downloaded"), result->downloaded);
            obj.insert(QStringLiteral("extracted"), result->extracted);
            obj.insert(QStringLiteral("ingested"), result->ingested);
            obj.insert(QStringLiteral("skipped"), result->skipped);
            QJsonArray paths;
            for (const QString &path : result->datPaths)
                paths.append(path);
            obj.insert(QStringLiteral("dat_paths"), paths);
            QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
        } else {
            if (options.dryRun)
                QTextStream(stdout) << "[dry-run] ";
            QTextStream(stdout) << "catalog fetch: downloaded " << result->downloaded << ", extracted "
                                << result->extracted << ", ingested " << result->ingested;
            if (result->skipped > 0)
                QTextStream(stdout) << ", skipped " << result->skipped;
            QTextStream(stdout) << '\n';
            for (const QString &msg : result->messages)
                QTextStream(stdout) << "  " << msg << '\n';
        }
        return 0;
    }

    int cmdScan(const QStringList &args, const QCommandLineParser &parser, bool json) {
        if (args.isEmpty()) {
            return printError(QStringLiteral("scan: missing DIR"));
        }
        const QString libraryPath = libraryDbPath(parser);
        ScanLibraryOptions options;
        options.noArchives = parser.isSet(QStringLiteral("no-archives"));
        auto scanResult = scanLibrary(args.at(0), libraryPath, options);
        if (!scanResult)
            return printError(scanResult.error());

        if (json) {
            QJsonObject obj;
            obj.insert(QStringLiteral("scanned"), scanResult->scanned);
            obj.insert(QStringLiteral("stored"), scanResult->stored);
            obj.insert(QStringLiteral("hashed"), scanResult->hashed);
            obj.insert(QStringLiteral("library"), libraryPath);
            QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
        } else {
            QTextStream(stdout) << "Scanned " << scanResult->scanned << " file(s), stored " << scanResult->stored
                                << ", hashed " << scanResult->hashed << '\n';
        }
        return 0;
    }

    int cmdMatch(const QStringList &args, const QCommandLineParser &parser, bool json) {
        const QString catalogPath = catalogDbPath(parser);
        const QString libraryPath = libraryDbPath(parser);

        if (args.isEmpty()) {
            catalog::MatchOptions options;
            options.online = parser.isSet(QStringLiteral("online"));
            options.dryRun = parser.isSet(QStringLiteral("dry-run"));
            if (options.online) {
                options.lookup = [](const QString &crc, const QString &md5, const QString &sha1) {
                    catalog::CatalogOnlineHit hit;
                    const GameMetadata meta = HasheousProvider().lookupByHashes(crc, md5, sha1);
                    hit.title = meta.title;
                    hit.coverUrl = meta.boxArtUrl;
                    hit.description = meta.description;
                    hit.externalId = meta.id;
                    return hit;
                };
            }
            auto result = catalog::matchLibrary(catalogPath, libraryPath, options);
            if (!result) {
                return printError(result.error());
            }
            if (json) {
                QJsonObject obj;
                obj.insert(QStringLiteral("matched"), result->matched);
                obj.insert(QStringLiteral("online_matched"), result->onlineMatched);
                obj.insert(QStringLiteral("negative_cached"), result->negativeCached);
                obj.insert(QStringLiteral("online_misses"), result->onlineMisses);
                obj.insert(QStringLiteral("catalog"), catalogPath);
                obj.insert(QStringLiteral("library"), libraryPath);
                obj.insert(QStringLiteral("online"), options.online);
                obj.insert(QStringLiteral("dry_run"), options.dryRun);
                QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
            } else {
                QTextStream(stdout) << "Matched " << result->matched << " library file(s)";
                if (options.online)
                    QTextStream(stdout) << " (" << result->onlineMatched << " via Hasheous)";
                QTextStream(stdout) << '\n';
            }
            return 0;
        }

        const QFileInfo pathInfo(args.at(0));
        if (pathInfo.isDir()) {
            return printError(QStringLiteral("match: PATH is a directory. Run 'scan DIR' then 'match' with no PATH."));
        }

        auto result = catalog::matchFile(catalogPath, args.at(0), libraryPath);
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
        const QString libraryPath = libraryDbPath(parser);
        const QString catalogPath = catalogDbPath(parser);

        if (!args.isEmpty() && args.at(0) == QStringLiteral("undo")) {
            Database db;
            if (!db.initialize(libraryPath))
                return printError(QStringLiteral("organize undo: failed to open library database"));
            OrganizeEngine engine(db);
            const int limit = parser.isSet(QStringLiteral("all")) ? 0 : 1;
            const int undone = engine.undoAll(limit);
            if (json) {
                QJsonObject obj;
                obj.insert(QStringLiteral("undone"), undone);
                QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
            } else {
                QTextStream(stdout) << "Undone " << undone << " organize operation(s)\n";
            }
            return undone > 0 ? 0 : 1;
        }

        if (args.isEmpty()) {
            return printError(QStringLiteral("organize: missing DEST (or 'undo')"));
        }
        const QString dest = args.at(0);
        const bool dryRun = parser.isSet(QStringLiteral("dry-run"));
        const bool bundle = parser.isSet(QStringLiteral("bundle"));
        const bool includeArt = parser.isSet(QStringLiteral("include-art"));
        const QString convertMode = parser.value(QStringLiteral("convert")).toLower();
        if (!convertMode.isEmpty() && convertMode != QStringLiteral("auto") && convertMode != QStringLiteral("never")) {
            return printError(QStringLiteral("organize: --convert must be auto or never"));
        }

        OrganizeLibraryOptions options;
        options.dryRun = dryRun;
        options.bundle = bundle;
        options.includeArt = includeArt;
        options.online = parser.isSet(QStringLiteral("online"));
        options.convert = convertMode == QStringLiteral("never") ? BundleConvertMode::Never : BundleConvertMode::Auto;
        auto organized = organizeLibrary(catalogPath, libraryPath, dest, options);
        if (!organized)
            return printError(organized.error());

        if (json) {
            QJsonObject obj;
            obj.insert(QStringLiteral("dest"), dest);
            obj.insert(QStringLiteral("dry_run"), dryRun);
            obj.insert(QStringLiteral("organized"), organized->organized);
            obj.insert(QStringLiteral("failed"), organized->failed);
            obj.insert(QStringLiteral("skipped_unmatched"), organized->skippedUnmatched);
            obj.insert(QStringLiteral("playlists"), organized->playlists);
            obj.insert(QStringLiteral("skipped"), organized->skipped);
            obj.insert(QStringLiteral("bundle"), bundle);
            if (!organized->archiveEntries.isEmpty()) {
                QJsonArray entries;
                for (const QString &entry : organized->archiveEntries)
                    entries.append(entry);
                obj.insert(QStringLiteral("archive_entries"), entries);
            }
            obj.insert(
                QStringLiteral("status"), organized->failed == 0 ? QStringLiteral("ok") : QStringLiteral("partial"));
            QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
        } else {
            QTextStream(stdout) << (dryRun ? "[dry-run] " : "") << "organized " << organized->organized
                                << " file(s) into " << dest;
            if (organized->skippedUnmatched > 0)
                QTextStream(stdout) << " (" << organized->skippedUnmatched << " unmatched skipped)";
            if (organized->playlists > 0)
                QTextStream(stdout) << ", " << organized->playlists << " playlist(s)";
            if (organized->failed > 0)
                QTextStream(stdout) << " (" << organized->failed << " failed)";
            QTextStream(stdout) << '\n';
        }
        return organized->failed > 0 ? 1 : 0;
    }

    int cmdPatch(const QStringList &args, const QCommandLineParser &parser, bool json) {
        if (args.isEmpty() || args.at(0) != QStringLiteral("apply")) {
            return printError(QStringLiteral("patch: use 'patch apply BASE PATCH [--output PATH]'"));
        }
        if (args.size() < 3) {
            return printError(QStringLiteral("patch apply: missing BASE and/or PATCH"));
        }
        const QString basePath = args.at(1);
        const QString patchPath = args.at(2);
        const QString outputPath = parser.value(QStringLiteral("output"));

        PatchEngine engine;
        const PatchInfo info = engine.detectFormat(patchPath);
        if (!info.valid) {
            return printError(info.error.isEmpty() ? QStringLiteral("Unrecognized patch format") : info.error);
        }
        const PatchResult result = engine.apply(basePath, info, outputPath);
        if (!result.success) {
            return printError(result.error);
        }
        if (json) {
            QJsonObject obj;
            obj.insert(QStringLiteral("status"), QStringLiteral("ok"));
            obj.insert(QStringLiteral("output"), result.outputPath);
            obj.insert(QStringLiteral("format"), info.formatName);
            QTextStream(stdout) << QJsonDocument(obj).toJson(QJsonDocument::Compact) << '\n';
        } else {
            QTextStream(stdout) << "Patched -> " << result.outputPath << '\n';
        }
        return 0;
    }

    int cmdVerify(const QCommandLineParser &parser, bool json) {
        const QString libraryPath = libraryDbPath(parser);
        const QString catalogPath = catalogDbPath(parser);

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
        const QString dbPath = libraryDbPath(parser);

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
        out << "Catalog (DAT reference — ingest before you can match):\n";
        out << "  catalog init [--db PATH]              Create schema only\n";
        out << "  catalog ingest DAT [--db PATH]        Load a No-Intro/Redump DAT\n";
        out << "  catalog fetch --system NAME|slug      Download Redump DAT + ingest\n";
        out << "  catalog fetch --from-dir DIR          Ingest local No-Intro/Redump DATs\n";
        out << "  catalog import-hasheous JSON          Optional cover URLs from JSON\n\n";
        out << "Library (your files — scan, then match):\n";
        out << "  scan DIR [--library-db PATH] [--no-archives]\n";
        out << "                                        Find ROM files, hash, store\n";
        out << "  match [FILE] [--catalog-db PATH] [--library-db PATH] [--online] [--dry-run]\n";
        out << "                                        Identify FILE, or every scanned file\n";
        out << "                                        --online: Hasheous identity for unmatched\n";
        out << "  list [--library-db PATH]              Print stored paths\n";
        out << "  verify [--catalog-db PATH] [--library-db PATH]\n";
        out << "                                        Recheck hashes against the catalog\n";
        out << "  enrich [--online] [--deep] [--dry-run]  Optional metadata for matched games only\n";
        out << "  organize DEST [--dry-run] [--bundle] [--include-art] [--convert auto|never]\n";
        out << "                                        Rename/move matched files; zip singles, folder sets\n";
        out << "  organize undo [--all]                 Undo last organize (or all with --all)\n";
        out << "  patch apply BASE PATCH [--output PATH]\n";
        out << "                                        Apply IPS/BPS/UPS/XDelta/PPF patch\n\n";
        out << "Diagnostics:\n";
        out << "  hash PATH                             Print CRC32 MD5 SHA1 (stdout only)\n\n";
        out << "Databases (two files, never mixed):\n";
        out << "  --db / --catalog-db PATH              catalog.db\n";
        out << "  --library-db PATH                     library.db\n";
        out << "  --json                                Machine-readable output\n";
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
    QCoreApplication::setApplicationName(QString::fromLatin1(remustwo::Constants::SETTINGS_APPLICATION));
    QCoreApplication::setOrganizationName(QString::fromLatin1(remustwo::Constants::SETTINGS_ORGANIZATION));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(remustwo::Constants::APP_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Offline ROM catalog and library manager."));
    parser.addOption({ { QStringLiteral("h"), QStringLiteral("help") }, QStringLiteral("Show help") });
    parser.addOption({ { QStringLiteral("j"), QStringLiteral("json") }, QStringLiteral("JSON output") });
    parser.addOption({ QStringLiteral("db"), QStringLiteral("Catalog database path"), QStringLiteral("path") });
    parser.addOption({ QStringLiteral("catalog-db"), QStringLiteral("Catalog database path"), QStringLiteral("path") });
    parser.addOption({ QStringLiteral("library-db"), QStringLiteral("Library database path"), QStringLiteral("path") });
    parser.addOption({ QStringLiteral("dry-run"), QStringLiteral("Dry run (no writes)") });
    parser.addOption({ QStringLiteral("online"), QStringLiteral("Enable online metadata/artwork fetch") });
    parser.addOption({ QStringLiteral("deep"),
        QStringLiteral("Fetch expanded IGDB metadata via Hasheous (requires client API key)") });
    parser.addOption({ QStringLiteral("bundle"),
        QStringLiteral("Cartridge ROMs -> zip + .remus.md; disc/CHD sets -> named folder + .remus.md") });
    parser.addOption({ QStringLiteral("include-art"),
        QStringLiteral("Copy cover art into bundle (uses cache; fetch with --online)") });
    parser.addOption(
        { QStringLiteral("convert"), QStringLiteral("Bundle conversion: auto or never"), QStringLiteral("mode") });
    parser.addOption({ QStringLiteral("system"), QStringLiteral("System name or Redump slug for catalog fetch"),
        QStringLiteral("name") });
    parser.addOption({ QStringLiteral("from-dir"), QStringLiteral("Directory of .dat files to ingest"),
        QStringLiteral("dir") });
    parser.addOption({ QStringLiteral("no-archives"), QStringLiteral("Do not scan inside zip/7z/rar archives") });
    parser.addOption({ QStringLiteral("all"), QStringLiteral("With organize undo: undo all pending operations") });
    parser.addOption({ { QStringLiteral("o"), QStringLiteral("output") }, QStringLiteral("Output path for patch apply"),
        QStringLiteral("path") });
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
            return printError(QStringLiteral("catalog: missing subcommand (init|ingest|fetch|import-hasheous)"));
        }
        const QString sub = positional.at(1);
        if (sub == QStringLiteral("init")) {
            return cmdCatalogInit(parser, json);
        }
        if (sub == QStringLiteral("ingest")) {
            return cmdCatalogIngest(positional.mid(2), parser, json);
        }
        if (sub == QStringLiteral("fetch")) {
            return cmdCatalogFetch(parser, json);
        }
        if (sub == QStringLiteral("import-hasheous")) {
            return cmdCatalogImportHasheous(positional.mid(2), parser, json);
        }
        if (sub == QStringLiteral("enrich")) {
            return cmdCatalogEnrich();
        }
        return printError(QStringLiteral("Unknown catalog subcommand: ") + sub);
    }
    if (command == QStringLiteral("enrich")) {
        return cmdEnrich(parser, json);
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
    if (command == QStringLiteral("patch")) {
        return cmdPatch(positional.mid(1), parser, json);
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
