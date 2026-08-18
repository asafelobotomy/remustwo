#include "library_paths.h"

#include <QDir>
#include <QStandardPaths>

namespace remustwo {

QString defaultDataDir() {
    const QString base
        = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/remustwo");
    QDir().mkpath(base);
    return base;
}

QString defaultCatalogPath() {
    return defaultDataDir() + QStringLiteral("/catalog.db");
}

QString defaultLibraryPath() {
    return defaultDataDir() + QStringLiteral("/library.db");
}

} // namespace remustwo
