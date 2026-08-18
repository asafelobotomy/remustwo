#include "catalog_match.h"
#include "catalog.h"
#include "sql_pragmas.h"

#include "../core/constants/match_methods.h"
#include "../core/database.h"
#include "../core/hasher.h"
#include "../core/matching_engine.h"
#include "../core/system_resolver.h"

#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace remustwo::catalog {

namespace {

    HashResult hashesFromRecord(const FileRecord &file) {
        HashResult hashes;
        hashes.crc32 = file.crc32;
        hashes.md5 = file.md5;
        hashes.sha1 = file.sha1;
        hashes.success = file.hashCalculated && (!file.sha1.isEmpty() || !file.md5.isEmpty() || !file.crc32.isEmpty());
        return hashes;
    }

    Result<CatalogMatch> lookupHashes(QSqlDatabase &db, const HashResult &hashes) {
        const auto lookup = [&](const QString &hashType, const QString &value) -> Result<CatalogMatch> {
            if (value.isEmpty()) {
                return Result<CatalogMatch>::fail(QStringLiteral("empty hash"));
            }
            QSqlQuery query(db);
            query.prepare(QStringLiteral("SELECT g.game_id, g.canonical_title, gs.hash_value, g.system_id "
                                         "FROM game_signatures gs JOIN games g ON g.game_id = gs.game_id "
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
            match.title = query.value(1).toString();
            match.matchedHash = query.value(2).toString();
            match.systemId = query.value(3).toInt();
            match.confidence = MatchingEngine::calculateConfidence(Constants::MatchMethods::HASH, 0.0f);
            return Result<CatalogMatch>::ok(match);
        };

        Result<CatalogMatch> result = lookup(QStringLiteral("sha1"), hashes.sha1);
        if (!result) {
            result = lookup(QStringLiteral("md5"), hashes.md5);
        }
        if (!result) {
            result = lookup(QStringLiteral("crc32"), hashes.crc32);
        }
        if (!result) {
            return Result<CatalogMatch>::fail(QStringLiteral("No catalog match for file"));
        }
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
        if (!library.updateFileHashes(fileId, hashes.crc32, hashes.md5, hashes.sha1)) {
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

} // namespace

Result<CatalogMatch> matchFile(const QString &dbPath, const QString &filePath) {
    return matchFile(dbPath, filePath, QString());
}

Result<CatalogMatch> matchFile(const QString &dbPath, const QString &filePath, const QString &libraryPath) {
    Hasher hasher;
    const HashResult hashes = hasher.calculateHashes(filePath);
    if (!hashes.success) {
        return Result<CatalogMatch>::fail(hashes.error);
    }

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
    Database library;
    if (!library.initialize(libraryPath)) {
        return Result<int>::fail(QStringLiteral("Failed to open library database"));
    }

    const QList<FileRecord> files = library.getAllFiles();
    if (files.isEmpty()) {
        return Result<int>::fail(QStringLiteral("Library is empty — run scan DIR first"));
    }

    auto dbResult = open(dbPath, QStringLiteral("catalog_match_library"));
    if (!dbResult) {
        return Result<int>::fail(dbResult.error());
    }
    QSqlDatabase catalogDb = *dbResult;
    CatalogSql::applyReadOnlyPragmas(catalogDb);

    Hasher hasher;
    int matched = 0;
    for (const FileRecord &file : files) {
        const MatchResult existing = library.getMatchForFile(file.id);
        if (existing.isConfirmed && !file.catalogGameId.isEmpty()) {
            ++matched;
            continue;
        }

        HashResult hashes = hashesFromRecord(file);
        if (!hashes.success) {
            hashes = hasher.calculateHashes(file.currentPath);
            if (!hashes.success) {
                continue;
            }
        }

        auto result = lookupHashes(catalogDb, hashes);
        if (!result) {
            continue;
        }
        auto written = writeLibraryRecord(library, file.currentPath, hashes, *result, file.id);
        if (written) {
            ++matched;
        }
    }

    const QString conn = catalogDb.connectionName();
    catalogDb.close();
    QSqlDatabase::removeDatabase(conn);
    return Result<int>::ok(matched);
}

} // namespace remustwo::catalog
