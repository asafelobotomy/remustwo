#include "catalog_match.h"
#include "catalog.h"
#include "sql_pragmas.h"

#include "../core/archive_extractor.h"
#include "../core/constants/match_methods.h"
#include "../core/constants/network.h"
#include "../core/database.h"
#include "../core/extended_hashes.h"
#include "../core/hasher.h"
#include "../core/matching_engine.h"
#include "../core/ra_hasher.h"
#include "../core/system_detector.h"
#include "../core/system_resolver.h"

#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QThread>
#include <QUuid>

namespace remustwo::catalog {

namespace {

    HashResult hashesFromRecord(const FileRecord &file) {
        HashResult hashes;
        hashes.crc32 = file.crc32;
        hashes.md5 = file.md5;
        hashes.sha1 = file.sha1;
        hashes.raMd5 = file.raMd5;
        hashes.chdSha1 = file.chdSha1;
        hashes.rvzSha1 = file.rvzSha1;
        hashes.success = file.hashCalculated
            && (!file.sha1.isEmpty() || !file.md5.isEmpty() || !file.crc32.isEmpty() || !file.chdSha1.isEmpty()
                || !file.rvzSha1.isEmpty());
        return hashes;
    }

    bool needsExtendedHashes(const FileRecord &file, const HashResult &hashes) {
        const QString ext = file.extension.toLower();
        if ((ext == QStringLiteral(".chd") || ext == QStringLiteral(".rvz") || ext == QStringLiteral(".gcz"))
            && hashes.chdSha1.isEmpty() && hashes.rvzSha1.isEmpty()) {
            return true;
        }
        return file.systemId > 0 && hashes.raMd5.isEmpty() && RaHasher::hasRaMapping(file.systemId);
    }

    int resolveSystemId(const QString &path, const QString &extension, Database *library = nullptr) {
        SystemDetector detector;
        const QString detected = detector.detectSystem(extension, path);
        if (detected.isEmpty())
            return 0;
        if (library)
            return library->getSystemId(detected);
        return Constants::Systems::getSystemIdByName(detected);
    }

    QString titleFromSourceEntryKey(const QString &sourceEntryKey) {
        const QStringList parts = sourceEntryKey.split(QLatin1Char('|'));
        return parts.size() >= 2 ? parts.at(1).trimmed() : QString();
    }

    Result<CatalogMatch> lookupHashes(QSqlDatabase &db, const HashResult &hashes) {
        const auto lookup = [&](const QString &hashType, const QString &value) -> Result<CatalogMatch> {
            if (value.isEmpty()) {
                return Result<CatalogMatch>::fail(QStringLiteral("empty hash"));
            }
            QSqlQuery query(db);
            query.prepare(QStringLiteral("SELECT g.game_id, g.canonical_title, gs.hash_value, g.system_id, "
                                         "gs.source_entry_key "
                                         "FROM game_signatures gs JOIN games g ON gs.game_id = g.game_id "
                                         "WHERE gs.hash_type = ? AND lower(gs.hash_value) = lower(?) LIMIT 1"));
            query.addBindValue(hashType);
            query.addBindValue(value);
            if (!query.exec()) {
                return Result<CatalogMatch>::fail(query.lastError().text());
            }
            if (!query.next()) {
                return Result<CatalogMatch>::fail(QStringLiteral("no match"));
            }
            CatalogMatch match;
            match.gameId = query.value(0).toString();
            const QString canonicalTitle = query.value(1).toString();
            match.matchedHash = query.value(2).toString();
            match.systemId = query.value(3).toInt();
            const QString entryTitle = titleFromSourceEntryKey(query.value(4).toString());
            match.title = entryTitle.isEmpty() ? canonicalTitle : entryTitle;
            match.confidence = MatchingEngine::calculateConfidence(Constants::MatchMethods::HASH, 0.0f);
            return Result<CatalogMatch>::ok(match);
        };

        // Prefer disc-content digests (Redump CHD/RVZ SHA1) over container file hashes.
        Result<CatalogMatch> result = lookup(QStringLiteral("sha1"), hashes.chdSha1);
        if (!result)
            result = lookup(QStringLiteral("sha1"), hashes.rvzSha1);
        if (!result)
            result = lookup(QStringLiteral("sha1"), hashes.sha1);
        if (!result)
            result = lookup(QStringLiteral("md5"), hashes.md5);
        if (!result)
            result = lookup(QStringLiteral("crc32"), hashes.crc32);
        if (!result)
            return Result<CatalogMatch>::fail(QStringLiteral("No catalog match for file"));
        return result;
    }

    Result<void> writeLibraryRecord(Database &library, const QString &filePath, const HashResult &hashes,
        const CatalogMatch &match, int existingFileId) {
        int systemId = match.systemId;
        if (!SystemResolver::isValidSystem(systemId)) {
            systemId = library.getSystemId(QStringLiteral("NES"));
        }
        if (systemId <= 0) {
            return Result<void>::fail(QStringLiteral("No valid system for library game"));
        }

        const QFileInfo info(filePath);
        int fileId = existingFileId;
        if (fileId <= 0) {
            int libraryId = library.insertLibrary(info.absolutePath(), QStringLiteral("default"));
            if (libraryId <= 0) {
                libraryId = 1;
            }

            FileRecord record;
            record.libraryId = libraryId;
            record.originalPath = info.absoluteFilePath();
            record.currentPath = info.absoluteFilePath();
            record.filename = info.fileName();
            record.extension = QStringLiteral(".") + info.suffix().toLower();
            record.fileSize = info.size();
            record.systemId = systemId;
            record.crc32 = hashes.crc32;
            record.md5 = hashes.md5;
            record.sha1 = hashes.sha1;
            record.raMd5 = hashes.raMd5;
            record.chdSha1 = hashes.chdSha1;
            record.rvzSha1 = hashes.rvzSha1;
            record.hashCalculated = true;
            record.baseTitle = match.title;
            record.catalogGameId = match.gameId;
            record.lastModified = info.lastModified();
            fileId = library.insertFile(record);
            if (fileId <= 0) {
                const auto existing = library.getAllFiles();
                for (const FileRecord &file : existing) {
                    if (file.originalPath == record.originalPath && file.filename == record.filename) {
                        fileId = file.id;
                        break;
                    }
                }
            }
        }
        if (fileId <= 0) {
            return Result<void>::fail(QStringLiteral("Failed to insert library file"));
        }
        if (!library.updateFileHashes(
                fileId, hashes.crc32, hashes.md5, hashes.sha1, hashes.raMd5, hashes.chdSha1, hashes.rvzSha1)) {
            return Result<void>::fail(QStringLiteral("Failed to store library hashes"));
        }
        if (!library.updateFileCatalogMatch(fileId, systemId, match.title, match.gameId)) {
            return Result<void>::fail(QStringLiteral("Failed to store catalog identity"));
        }

        const int gameId = library.insertGame(match.title, systemId);
        if (gameId <= 0) {
            return Result<void>::fail(QStringLiteral("Failed to insert library game"));
        }
        if (!library.insertMatch(
                fileId, gameId, static_cast<float>(match.confidence) / 100.0f, Constants::MatchMethods::HASH)) {
            return Result<void>::fail(QStringLiteral("Failed to insert library match"));
        }
        library.confirmMatch(fileId);
        if (!library.updateFileCatalogMatch(fileId, systemId, match.title, match.gameId)) {
            return Result<void>::fail(QStringLiteral("Failed to restore catalog identity after confirm"));
        }
        return Result<void>::ok();
    }

    QString onlineCacheKey(const HashResult &hashes) {
        const QString digest = !hashes.sha1.isEmpty() ? hashes.sha1
            : (!hashes.md5.isEmpty()                 ? hashes.md5
                                                     : hashes.crc32);
        return QStringLiteral("match:hasheous:%1").arg(digest.trimmed().toLower());
    }

    bool isNegativeCached(QSqlDatabase &library, const QString &key) {
        QSqlQuery query(library);
        query.prepare(QStringLiteral(
            "SELECT 1 FROM cache WHERE cache_key = ? AND (expiry IS NULL OR expiry > datetime('now')) LIMIT 1"));
        query.addBindValue(key);
        return query.exec() && query.next();
    }

    void rememberMiss(QSqlDatabase &library, const QString &key) {
        QSqlQuery query(library);
        query.prepare(QStringLiteral("INSERT OR REPLACE INTO cache (cache_key, cache_value, created_at, expiry) "
                                     "VALUES (?, 'miss', datetime('now'), datetime('now', '+30 days'))"));
        query.addBindValue(key);
        query.exec();
    }

    Result<void> ensureHasheousSource(QSqlDatabase &db) {
        QSqlQuery source(db);
        source.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO sources "
            "(source_id, display_name, source_type, priority, enabled, license_id, attribution_required) "
            "VALUES ('hasheous', 'Hasheous', 'online', 50, 1, 'community', 0)"));
        if (!source.exec())
            return Result<void>::fail(source.lastError().text());
        QSqlQuery snap(db);
        snap.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO source_snapshots (snapshot_id, source_id, snapshot_label, fetched_at) "
            "VALUES ('hasheous_live', 'hasheous', 'live', datetime('now'))"));
        if (!snap.exec())
            return Result<void>::fail(snap.lastError().text());
        return Result<void>::ok();
    }

    Result<CatalogMatch> upsertOnlineHit(QSqlDatabase &db, const HashResult &hashes, const CatalogOnlineHit &hit,
        int systemId) {
        if (hit.title.trimmed().isEmpty())
            return Result<CatalogMatch>::fail(QStringLiteral("Hasheous hit missing title"));
        if (systemId <= 0)
            systemId = Constants::Systems::getSystemIdByName(QStringLiteral("NES"));
        if (systemId <= 0)
            return Result<CatalogMatch>::fail(QStringLiteral("No system for online identity"));

        auto ensured = ensureHasheousSource(db);
        if (!ensured)
            return Result<CatalogMatch>::fail(ensured.error());

        const QString gameId = hit.externalId.trimmed().isEmpty()
            ? QStringLiteral("hasheous:%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
            : QStringLiteral("hasheous:%1").arg(hit.externalId.trimmed());

        QSqlQuery insertGame(db);
        insertGame.prepare(QStringLiteral(
            "INSERT INTO games (game_id, system_id, canonical_title, description, cover_url, "
            "canonical_confidence) VALUES (?, ?, ?, ?, ?, 0.5) "
            "ON CONFLICT(game_id) DO UPDATE SET "
            "canonical_title = COALESCE(NULLIF(excluded.canonical_title, ''), games.canonical_title), "
            "description = COALESCE(NULLIF(excluded.description, ''), games.description), "
            "cover_url = COALESCE(NULLIF(excluded.cover_url, ''), games.cover_url), "
            "updated_at = CURRENT_TIMESTAMP"));
        insertGame.addBindValue(gameId);
        insertGame.addBindValue(systemId);
        insertGame.addBindValue(hit.title.trimmed());
        insertGame.addBindValue(hit.description);
        insertGame.addBindValue(hit.coverUrl);
        if (!insertGame.exec())
            return Result<CatalogMatch>::fail(insertGame.lastError().text());

        const auto insertSig = [&](const QString &type, const QString &value) -> Result<void> {
            if (value.trimmed().isEmpty())
                return Result<void>::ok();
            QSqlQuery sig(db);
            sig.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO game_signatures "
                "(game_id, hash_type, hash_value, source_id, snapshot_id, source_entry_key, confidence, is_primary) "
                "VALUES (?, ?, lower(?), 'hasheous', 'hasheous_live', ?, 0.8, 1)"));
            sig.addBindValue(gameId);
            sig.addBindValue(type);
            sig.addBindValue(value);
            sig.addBindValue(QStringLiteral("hasheous|%1").arg(hit.title));
            if (!sig.exec())
                return Result<void>::fail(sig.lastError().text());
            return Result<void>::ok();
        };

        if (auto r = insertSig(QStringLiteral("sha1"), hashes.sha1); !r)
            return Result<CatalogMatch>::fail(r.error());
        if (auto r = insertSig(QStringLiteral("md5"), hashes.md5); !r)
            return Result<CatalogMatch>::fail(r.error());
        if (auto r = insertSig(QStringLiteral("crc32"), hashes.crc32); !r)
            return Result<CatalogMatch>::fail(r.error());
        if (auto r = insertSig(QStringLiteral("sha1"), hashes.chdSha1); !r)
            return Result<CatalogMatch>::fail(r.error());
        if (auto r = insertSig(QStringLiteral("sha1"), hashes.rvzSha1); !r)
            return Result<CatalogMatch>::fail(r.error());

        CatalogMatch match;
        match.gameId = gameId;
        match.title = hit.title.trimmed();
        match.systemId = systemId;
        match.matchedHash = !hashes.sha1.isEmpty() ? hashes.sha1 : (!hashes.md5.isEmpty() ? hashes.md5 : hashes.crc32);
        match.confidence = MatchingEngine::calculateConfidence(Constants::MatchMethods::HASH, 0.0f);
        return Result<CatalogMatch>::ok(match);
    }

} // namespace

Result<CatalogMatch> matchFile(const QString &dbPath, const QString &filePath) {
    return matchFile(dbPath, filePath, QString());
}

Result<CatalogMatch> matchFile(const QString &dbPath, const QString &filePath, const QString &libraryPath) {
    Hasher hasher;
    HashResult hashes = hasher.calculateContentHashes(filePath);
    if (!hashes.success) {
        return Result<CatalogMatch>::fail(hashes.error);
    }

    const QFileInfo info(filePath);
    const QString extension = QStringLiteral(".") + info.suffix().toLower();
    const int systemId = resolveSystemId(filePath, extension);
    populateExtendedHashes(hashes, { filePath, systemId, extension });

    auto dbResult = open(dbPath, QStringLiteral("catalog_match"));
    if (!dbResult) {
        return Result<CatalogMatch>::fail(dbResult.error());
    }

    QSqlDatabase db = *dbResult;
    CatalogSql::applyReadOnlyPragmas(db);
    Result<CatalogMatch> result = lookupHashes(db, hashes);
    const QString conn = db.connectionName();
    db.close();
    QSqlDatabase::removeDatabase(conn);

    if (!result) {
        return result;
    }
    if (!libraryPath.isEmpty()) {
        Database library;
        if (!library.initialize(libraryPath)) {
            return Result<CatalogMatch>::fail(QStringLiteral("Failed to open library database"));
        }
        auto written = writeLibraryRecord(library, filePath, hashes, *result, 0);
        if (!written) {
            return Result<CatalogMatch>::fail(written.error());
        }
    }
    return result;
}

Result<int> matchLibrary(const QString &dbPath, const QString &libraryPath) {
    auto stats = matchLibrary(dbPath, libraryPath, MatchOptions {});
    if (!stats)
        return Result<int>::fail(stats.error());
    return Result<int>::ok(stats->matched);
}

Result<MatchLibraryStats> matchLibrary(
    const QString &dbPath, const QString &libraryPath, const MatchOptions &options) {
    Database library;
    if (!library.initialize(libraryPath)) {
        return Result<MatchLibraryStats>::fail(QStringLiteral("Failed to open library database"));
    }

    const QList<FileRecord> files = library.getAllFiles();
    if (files.isEmpty()) {
        return Result<MatchLibraryStats>::fail(QStringLiteral("Library is empty — run scan DIR first"));
    }

    auto dbResult = open(dbPath, QStringLiteral("catalog_match_library"));
    if (!dbResult) {
        return Result<MatchLibraryStats>::fail(dbResult.error());
    }
    QSqlDatabase catalogDb = *dbResult;
    if (options.online && !options.dryRun)
        CatalogSql::applyWritePragmas(catalogDb);
    else
        CatalogSql::applyReadOnlyPragmas(catalogDb);

    Hasher hasher;
    MatchLibraryStats stats;
    bool onlineLookupUsed = false;

    for (const FileRecord &file : files) {
        const MatchResult existing = library.getMatchForFile(file.id);
        if (existing.isConfirmed && !file.catalogGameId.isEmpty()) {
            ++stats.matched;
            continue;
        }

        HashResult hashes = hashesFromRecord(file);
        if (!hashes.success || needsExtendedHashes(file, hashes)) {
            QString hashPath = file.currentPath;
            QTemporaryDir tempExtract;
            if (file.isCompressed && !file.archivePath.isEmpty() && !file.archiveInternalPath.isEmpty()
                && tempExtract.isValid()) {
                ArchiveExtractor extractor;
                if (extractor.canExtract(file.archivePath)) {
                    const ExtractionResult extracted
                        = extractor.extractFile(file.archivePath, file.archiveInternalPath, tempExtract.path());
                    if (extracted.success && !extracted.extractedFiles.isEmpty())
                        hashPath = extracted.extractedFiles.first();
                    else
                        continue;
                } else {
                    continue;
                }
            }
            hashes = hasher.calculateContentHashes(hashPath);
            if (!hashes.success)
                continue;
            int systemId = file.systemId;
            if (systemId <= 0)
                systemId = resolveSystemId(hashPath, file.extension, &library);
            populateExtendedHashes(hashes, { hashPath, systemId, file.extension });
        }

        auto result = lookupHashes(catalogDb, hashes);
        if (result) {
            if (!options.dryRun) {
                auto written = writeLibraryRecord(library, file.currentPath, hashes, *result, file.id);
                if (written)
                    ++stats.matched;
            } else {
                ++stats.matched;
            }
            continue;
        }

        if (!options.online || !options.lookup)
            continue;

        const QString key = onlineCacheKey(hashes);
        if (isNegativeCached(library.database(), key)) {
            ++stats.negativeCached;
            continue;
        }

        if (onlineLookupUsed)
            QThread::msleep(Constants::Network::HASHEOUS_RATE_LIMIT_MS);
        onlineLookupUsed = true;

        const CatalogOnlineHit hit = options.lookup(hashes.crc32, hashes.md5, hashes.sha1);
        if (hit.title.trimmed().isEmpty()) {
            ++stats.onlineMisses;
            if (!options.dryRun)
                rememberMiss(library.database(), key);
            continue;
        }

        if (options.dryRun) {
            ++stats.onlineMatched;
            ++stats.matched;
            continue;
        }

        int systemId = file.systemId;
        if (systemId <= 0)
            systemId = resolveSystemId(file.currentPath, file.extension, &library);
        auto upserted = upsertOnlineHit(catalogDb, hashes, hit, systemId);
        if (!upserted) {
            ++stats.onlineMisses;
            continue;
        }
        auto written = writeLibraryRecord(library, file.currentPath, hashes, *upserted, file.id);
        if (written) {
            ++stats.onlineMatched;
            ++stats.matched;
        }
    }

    const QString conn = catalogDb.connectionName();
    catalogDb.close();
    QSqlDatabase::removeDatabase(conn);
    return Result<MatchLibraryStats>::ok(stats);
}

} // namespace remustwo::catalog
