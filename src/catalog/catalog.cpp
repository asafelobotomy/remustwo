#include "catalog.h"
#include "sql_pragmas.h"
#include "sql_utilities.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace remustwo::catalog {

namespace {

int migrationVersionFromFilename(const QString &filename) {
    const int underscore = filename.indexOf(QLatin1Char('_'));
    if (underscore <= 0)
        return -1;
    bool ok = false;
    const int version = filename.left(underscore).toInt(&ok);
    return ok ? version : -1;
}

Result<int> currentUserVersion(QSqlDatabase &database) {
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA user_version"))) {
        return Result<int>::fail(query.lastError().text());
    }
    if (!query.next()) {
        return Result<int>::fail(QStringLiteral("PRAGMA user_version returned no rows"));
    }
    return Result<int>::ok(query.value(0).toInt());
}

Result<void> setUserVersion(QSqlDatabase &database, int version) {
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version))) {
        return Result<void>::fail(query.lastError().text());
    }
    return Result<void>::ok();
}

} // namespace

Result<QSqlDatabase> open(const QString &dbPath, const QString &connectionName) {
    const QString conn = connectionName.isEmpty()
        ? QStringLiteral("catalog_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))
        : connectionName;

    if (QSqlDatabase::contains(conn))
        QSqlDatabase::removeDatabase(conn);

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
    database.setDatabaseName(dbPath);
    if (!database.open()) {
        return Result<QSqlDatabase>::fail(database.lastError().text());
    }

    CatalogSql::applyWritePragmas(database);
    return Result<QSqlDatabase>::ok(database);
}

Result<void> applyMigrations(QSqlDatabase &database) {
    QString error;
    const QString manifestPath = QDir(CatalogSql::migrationsDir()).filePath(QStringLiteral("manifest.json"));
    const QStringList migrations = CatalogSql::loadMigrationManifest(manifestPath, error);
    if (migrations.isEmpty()) {
        return Result<void>::fail(error.isEmpty() ? QStringLiteral("No migrations in manifest") : error);
    }

    auto versionResult = currentUserVersion(database);
    if (!versionResult) {
        return Result<void>::fail(versionResult.error());
    }
    int userVersion = *versionResult;

    for (const QString &migrationFile : migrations) {
        const int migrationVersion = migrationVersionFromFilename(migrationFile);
        if (migrationVersion < 0) {
            return Result<void>::fail(QStringLiteral("Invalid migration filename: %1").arg(migrationFile));
        }
        if (migrationVersion <= userVersion) {
            continue;
        }

        const QString path = QDir(CatalogSql::migrationsDir()).filePath(migrationFile);
        if (!CatalogSql::executeSqlScript(database, path, error)) {
            return Result<void>::fail(error);
        }

        auto setVersion = setUserVersion(database, migrationVersion);
        if (!setVersion) {
            return setVersion;
        }

        userVersion = migrationVersion;
    }

    CatalogSql::finalizeDatabasePragmas(database);
    return Result<void>::ok();
}

Result<void> applySeeds(QSqlDatabase &database) {
    QSqlQuery countQuery(database);
    if (!countQuery.exec(QStringLiteral("SELECT COUNT(*) FROM systems"))) {
        return Result<void>::fail(countQuery.lastError().text());
    }
    if (countQuery.next() && countQuery.value(0).toInt() > 0) {
        return Result<void>::ok();
    }

    QString error;
    const QDir seedDir(CatalogSql::seedsDir());
    const QStringList seedFiles = seedDir.entryList({ QStringLiteral("*.sql") }, QDir::Files, QDir::Name);
    for (const QString &seedFile : seedFiles) {
        if (!CatalogSql::executeSqlScript(database, seedDir.filePath(seedFile), error)) {
            return Result<void>::fail(error);
        }
    }
    return Result<void>::ok();
}

Result<void> init(const QString &dbPath) {
    QFileInfo info(dbPath);
    if (info.dir().exists() || QDir().mkpath(info.dir().absolutePath())) {
        // ok
    } else if (!info.dir().exists()) {
        return Result<void>::fail(QStringLiteral("Failed to create catalog directory: %1").arg(info.dir().path()));
    }

    auto dbResult = open(dbPath);
    if (!dbResult) {
        return Result<void>::fail(dbResult.error());
    }

    QSqlDatabase database = std::move(*dbResult);
    const QString conn = database.connectionName();

    auto migrations = applyMigrations(database);
    if (!migrations) {
        database.close();
        QSqlDatabase::removeDatabase(conn);
        return migrations;
    }

    auto seeds = applySeeds(database);
    if (!seeds) {
        database.close();
        QSqlDatabase::removeDatabase(conn);
        return seeds;
    }

    database.close();
    QSqlDatabase::removeDatabase(conn);
    return Result<void>::ok();
}

} // namespace remustwo::catalog
