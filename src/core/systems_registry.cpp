#include "systems_registry.h"

#include "../catalog/sql_utilities.h"

#include <QDir>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>

namespace remustwo::SystemsSeed {

namespace {

    QMap<int, SystemDef> buildRegistryFromCatalogSeed() {
        const QString conn = QStringLiteral("systems_seed_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (QSqlDatabase::contains(conn))
            QSqlDatabase::removeDatabase(conn);

        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
        db.setDatabaseName(QStringLiteral(":memory:"));
        if (!db.open())
            return { };

        QSqlQuery create(db);
        if (!create.exec(QStringLiteral(
                "CREATE TABLE systems ("
                "system_id INTEGER PRIMARY KEY, "
                "internal_name TEXT NOT NULL, "
                "display_name TEXT NOT NULL, "
                "manufacturer TEXT, "
                "generation INTEGER, "
                "release_year INTEGER, "
                "preferred_hash TEXT NOT NULL, "
                "is_disc_based INTEGER NOT NULL DEFAULT 0, "
                "is_handheld INTEGER NOT NULL DEFAULT 0)"))) {
            db.close();
            QSqlDatabase::removeDatabase(conn);
            return { };
        }

        QString error;
        const QString seedPath = QDir(CatalogSql::seedsDir()).filePath(QStringLiteral("0002_systems.sql"));
        if (!CatalogSql::executeSqlScript(db, seedPath, error)) {
            db.close();
            QSqlDatabase::removeDatabase(conn);
            return { };
        }

        QMap<int, SystemDef> registry;
        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("SELECT system_id, internal_name, display_name, manufacturer, generation, "
                                       "release_year, preferred_hash, is_disc_based, is_handheld "
                                       "FROM systems ORDER BY system_id"))) {
            db.close();
            QSqlDatabase::removeDatabase(conn);
            return { };
        }

        while (query.next()) {
            SystemDef def;
            def.id = query.value(0).toInt();
            def.internalName = query.value(1).toString();
            def.displayName = query.value(2).toString();
            def.manufacturer = query.value(3).toString();
            def.generation = query.value(4).toInt();
            def.releaseYear = query.value(5).toInt();
            def.preferredHash = query.value(6).toString();
            def.isDiscBased = query.value(7).toInt() != 0;
            def.isHandheld = query.value(8).toInt() != 0;
            def.extensions = Constants::Systems::dbExtensionsForSystem(def.id);
            def.isMultiFile = def.isDiscBased;
            registry.insert(def.id, def);
        }

        db.close();
        QSqlDatabase::removeDatabase(conn);
        return registry;
    }

} // namespace

const QMap<int, SystemDef> &registry() {
    static const QMap<int, SystemDef> kRegistry = buildRegistryFromCatalogSeed();
    return kRegistry;
}

} // namespace remustwo::SystemsSeed
