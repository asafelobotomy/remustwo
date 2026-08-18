#pragma once

#include "../core/result.h"

#include <QSqlDatabase>
#include <QString>

namespace remustwo::catalog {

Result<void> ingestDat(const QString &dbPath, const QString &datPath);
Result<void> ingestDat(QSqlDatabase &database, const QString &datPath);

} // namespace remustwo::catalog
