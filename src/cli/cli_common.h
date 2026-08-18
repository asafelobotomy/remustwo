#pragma once

#include <QStandardPaths>
#include <QString>

namespace remustwo::cli {

QString defaultCatalogPath();
QString defaultLibraryPath();
int run(int argc, char *argv[]);

} // namespace remustwo::cli
