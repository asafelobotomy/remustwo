#include "library_organize.h"

#include "../core/database.h"
#include "../core/m3u_generator.h"
#include "../core/organize_engine.h"
#include "../core/template_engine.h"
#include "artwork_cache.h"

#include <QFile>
#include <QMap>
#include <QSet>

namespace remustwo {

Result<OrganizeLibraryStats> organizeLibrary(const QString &catalogDbPath, const QString &libraryDbPath,
    const QString &dest, const OrganizeLibraryOptions &options) {
    if (dest.trimmed().isEmpty())
        return Result<OrganizeLibraryStats>::fail(QStringLiteral("organize: missing DEST"));

    Database db;
    if (!db.initialize(libraryDbPath))
        return Result<OrganizeLibraryStats>::fail(QStringLiteral("Failed to open library database"));
    db.setCompendiumDbPath(catalogDbPath);

    const auto files = db.getFilesEligibleForOrganize();
    OrganizeLibraryStats stats;
    stats.skippedUnmatched = db.getAllFiles().size() - files.size();

    QList<int> fileIds;
    QMap<int, GameMetadata> metadataMap;
    for (const FileRecord &file : files) {
        fileIds.append(file.id);
        GameMetadata meta;
        meta.title = file.baseTitle;
        meta.system = db.getSystemDisplayName(file.systemId);
        metadataMap.insert(file.id, meta);
    }

    QSet<int> organizedIds;
    if (options.bundle) {
        RomBundler bundler(db);
        BundleConfig config;
        config.dryRun = options.dryRun;
        config.includeArt = options.includeArt;
        config.convert = options.convert;
        for (const FileRecord &file : files) {
            if (options.includeArt)
                config.artworkPath = cachedArtworkPath(file.catalogGameId);
            else
                config.artworkPath.clear();
            const BundleResult bundled = bundler.bundle(file, metadataMap.value(file.id), dest, config);
            if (bundled.skippedAlreadyBundled) {
                ++stats.skipped;
                continue;
            }
            if (!bundled.success) {
                ++stats.failed;
                continue;
            }
            ++stats.organized;
            organizedIds.insert(file.id);
            stats.archiveEntries.append(bundled.archiveEntries);
        }
    } else {
        OrganizeEngine engine(db);
        engine.setDryRun(options.dryRun);
        engine.setTemplate(TemplateEngine::getNoIntroTemplate());
        const QList<OrganizeResult> results = engine.organizeFiles(fileIds, metadataMap, dest, FileOperation::Move);
        for (const OrganizeResult &result : results) {
            if (result.success)
                ++stats.organized;
            else
                ++stats.failed;
        }
        for (int fileId : fileIds)
            organizedIds.insert(fileId);
    }

    if (!options.dryRun && stats.failed == 0 && !organizedIds.isEmpty()) {
        M3UGenerator playlistsEngine(db);
        stats.playlists = playlistsEngine.generateAll(organizedIds, dest);
    }
    return Result<OrganizeLibraryStats>::ok(stats);
}

} // namespace remustwo
