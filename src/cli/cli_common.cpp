#include "cli_common.h"

#include <QCoreApplication>
#include <QDir>

namespace remustwo::cli {

QString defaultCatalogPath() {
    const QString base
        = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/catalog.db");
}

QString defaultLibraryPath() {
    const QString base
        = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/library.db");
}

} // namespace remustwo::cli
