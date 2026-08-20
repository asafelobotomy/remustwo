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
    /// Output is a loose folder (multi-disc set or emulator-incompatible zip payload).
    bool skippedDiscSet = false;
    QString outputPath;
    QStringList archiveEntries;
    QString error;
};

class RomBundler {
public:
    explicit RomBundler(Database &database);

    static bool isAlreadyBundled(const QString &archivePath);

    bool usesFolderBundle(const FileRecord &file, BundleConvertMode convert) const;

    BundleResult bundle(const FileRecord &file, const GameMetadata &metadata, const QString &destinationDir,
        const BundleConfig &config);

    QString convertIfNeeded(
        const QString &sourcePath, const FileRecord &file, const BundleConfig &config, QString &payloadExtension) const;

private:
    Database &m_database;

    QString generateMarkerContent(const FileRecord &file, const GameMetadata &metadata) const;
    QString folderNameForBundle(const FileRecord &file, const GameMetadata &metadata) const;
    BundleResult bundleGameFolder(const FileRecord &file, const GameMetadata &metadata,
        const QString &destinationDir, const BundleConfig &config);
};

} // namespace remustwo
