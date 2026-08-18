#pragma once

#include "../core/result.h"

#include <QSqlDatabase>
#include <QString>

namespace remustwo::catalog {

Result<QSqlDatabase> open(const QString &dbPath, const QString &connectionName = QString());
Result<void> init(const QString &dbPath);
Result<void> applyMigrations(QSqlDatabase &database);
Result<void> applySeeds(QSqlDatabase &database);

} // namespace remustwo::catalog
