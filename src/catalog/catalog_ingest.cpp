#include "catalog_ingest.h"
#include "catalog.h"
#include "compendium_compiler_service.h"
#include "sql_pragmas.h"
#include "sql_utilities.h"

#include "../core/dat_parser.h"

#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>

namespace remustwo::catalog {

namespace {

QString slugSourceId(const QString &datPath) {
    return QFileInfo(datPath).completeBaseName().toLower().replace(QLatin1Char(' '), QLatin1Char('_'));
}

Result<void> ensureSource(QSqlDatabase &db, const QString &sourceId, const QString &displayName) {
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO sources "
        "(source_id, display_name, source_type, priority, enabled, license_id, attribution_required) "
        "VALUES (?, ?, 'dat', 100, 1, 'community', 0)"));
    query.addBindValue(sourceId);
    query.addBindValue(displayName);
    if (!query.exec()) {
        return Result<void>::fail(query.lastError().text());
    }
    return Result<void>::ok();
}

Result<void> ensureSnapshot(QSqlDatabase &db, const QString &sourceId, const QString &snapshotId,
    const QString &label) {
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO source_snapshots (snapshot_id, source_id, snapshot_label, fetched_at) "
        "VALUES (?, ?, ?, datetime('now'))"));
    query.addBindValue(snapshotId);
    query.addBindValue(sourceId);
    query.addBindValue(label);
    if (!query.exec()) {
        return Result<void>::fail(query.lastError().text());
    }
    return Result<void>::ok();
}

} // namespace

Result<void> ingestDat(QSqlDatabase &database, const QString &datPath) {
    DatParser parser;
    const DatParseResult parsed = parser.parse(datPath);
    const QString sourceId = slugSourceId(datPath);
    const QString snapshotId = sourceId + QStringLiteral("_v1");
    const QString displayName = parsed.header.name.isEmpty() ? sourceId : parsed.header.name;
    const QString snapshotLabel = parsed.header.version.isEmpty() ? snapshotId : parsed.header.version;

    QString error;
    if (!CatalogSql::beginImmediateTransaction(database, error)) {
        return Result<void>::fail(error);
    }

    if (!ensureSource(database, sourceId, displayName)) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("Failed to ensure source"));
    }
    if (!ensureSnapshot(database, sourceId, snapshotId, snapshotLabel)) {
        database.rollback();
        return Result<void>::fail(QStringLiteral("Failed to ensure snapshot"));
    }

    Compendium::CompendiumBuildConfig config;
    config.buildId = snapshotId;
    config.schemaVersion = 1;
    config.manifestJson = QStringLiteral("{}");

    Compendium::CompendiumSourceConfig source;
    source.sourceId = sourceId;
    source.displayName = displayName;
    source.sourceType = QStringLiteral("dat");
    source.snapshotId = snapshotId;
    source.filePath = datPath;
    source.priority = 100;
    source.enabled = true;
    config.sources.append(source);

    Compendium::CompilerRunOptions options;
    options.purgeChangedSources = true;
    options.extractParallelism = 1;

    Compendium::CompendiumCompilerService compiler;
    QString compilerError;
    compiler.run(config, database, compilerError, nullptr, options);
    if (!compilerError.isEmpty()) {
        database.rollback();
        return Result<void>::fail(compilerError);
    }

    if (!database.commit()) {
        return Result<void>::fail(database.lastError().text());
    }

    CatalogSql::finalizeDatabasePragmas(database);
    return Result<void>::ok();
}

Result<void> ingestDat(const QString &dbPath, const QString &datPath) {
    auto dbResult = open(dbPath);
    if (!dbResult) {
        return Result<void>::fail(dbResult.error());
    }

    QSqlDatabase database = *dbResult;
    const QString conn = database.connectionName();
    auto result = ingestDat(database, datPath);
    database.close();
    QSqlDatabase::removeDatabase(conn);
    return result;
}

} // namespace remustwo::catalog
