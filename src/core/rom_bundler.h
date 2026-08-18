#pragma once

#include "database_types.h"
#include "result.h"

#include "../metadata/metadata_provider.h"

#include <QString>
#include <QStringList>

namespace remustwo {

class Database;

enum class BundleConvertMode { Auto, Never };

struct BundleConfig {
    bool dryRun = false;
    bool includeArt = false;
    BundleConvertMode convert = BundleConvertMode::Auto;
    QString artworkPath;
};

struct BundleResult {
    bool success = false;
    bool skippedAlreadyBundled = false;
    bool skippedDiscSet = false;
    QString outputPath;
    QStringList archiveEntries;
    QString error;
};

class RomBundler {
public:
    explicit RomBundler(Database &database);

    static bool isAlreadyBundled(const QString &archivePath);

    BundleResult bundle(const FileRecord &file, const GameMetadata &metadata, const QString &destinationDir,
        const BundleConfig &config);

private:
    Database &m_database;

    QString generateMarkerContent(const FileRecord &file, const GameMetadata &metadata) const;
    QString convertIfNeeded(const QString &sourcePath, const FileRecord &file, const BundleConfig &config,
        QString &payloadExtension) const;
};

} // namespace remustwo
