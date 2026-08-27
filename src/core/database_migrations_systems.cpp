#include "database.h"

#include "constants/constants.h"
#include "system_detector.h"

#include <functional>

#include <QSqlError>
#include <QSqlQuery>

namespace remustwo {

bool migrateCanonicalSystems(
    QSqlDatabase &db, Database &database, const std::function<bool(const QString &)> &rollbackAndFail) {
    SystemDetector detector;

    for (const QString &name : Constants::Systems::getSystemInternalNames()) {
        const SystemInfo info = detector.getSystemInfo(name);
        if (info.name.isEmpty() || info.extensions.isEmpty())
            continue;

        QSqlQuery selectCanonicalSlot(db);
        selectCanonicalSlot.prepare("SELECT name FROM systems WHERE id = ?");
        selectCanonicalSlot.addBindValue(info.id);
        if (!selectCanonicalSlot.exec()) {
            return rollbackAndFail(
                QStringLiteral("Migration: Failed to inspect canonical system slot: %1")
                    .arg(selectCanonicalSlot.lastError().text()));
        }
        if (selectCanonicalSlot.next()) {
            const QString occupyingName = selectCanonicalSlot.value(0).toString();
            if (!occupyingName.isEmpty() && occupyingName != info.name) {
                const QString movedName
                    = QStringLiteral("%1__legacy_slot_%2").arg(occupyingName).arg(info.id);

                QSqlQuery renameOccupyingRow(db);
                renameOccupyingRow.prepare("UPDATE systems SET id = id + ?, name = ? WHERE id = ?");
                renameOccupyingRow.addBindValue(Constants::DatabaseSchema::Migrations::LEGACY_SYSTEM_SLOT_OFFSET);
                renameOccupyingRow.addBindValue(movedName);
                renameOccupyingRow.addBindValue(info.id);
                if (!renameOccupyingRow.exec()) {
                    return rollbackAndFail(
                        QStringLiteral("Migration: Failed to free canonical system slot: %1")
                            .arg(renameOccupyingRow.lastError().text()));
                }
            }
        }

        int existingId = 0;
        QSqlQuery selectSystem(db);
        selectSystem.prepare("SELECT id FROM systems WHERE name = ?");
        selectSystem.addBindValue(info.name);
        if (!selectSystem.exec()) {
            return rollbackAndFail(
                QStringLiteral("Migration: Failed to inspect systems table: %1").arg(selectSystem.lastError().text()));
        }
        if (selectSystem.next())
            existingId = selectSystem.value(0).toInt();

        if (existingId > 0 && existingId != info.id) {
            const QString legacyName = QStringLiteral("%1__legacy_%2").arg(info.name).arg(existingId);

            QSqlQuery renameSystem(db);
            renameSystem.prepare("UPDATE systems SET name = ? WHERE id = ?");
            renameSystem.addBindValue(legacyName);
            renameSystem.addBindValue(existingId);
            if (!renameSystem.exec()) {
                return rollbackAndFail(
                    QStringLiteral("Migration: Failed to rename legacy system row: %1")
                        .arg(renameSystem.lastError().text()));
            }

            if (database.insertSystem(info) == 0) {
                return rollbackAndFail(
                    QStringLiteral("Migration: Failed to insert canonical system row for: %1").arg(info.name));
            }

            QSqlQuery updateFiles(db);
            updateFiles.prepare("UPDATE files SET system_id = ? WHERE system_id = ?");
            updateFiles.addBindValue(info.id);
            updateFiles.addBindValue(existingId);
            if (!updateFiles.exec()) {
                return rollbackAndFail(
                    QStringLiteral("Migration: Failed to update file system IDs: %1").arg(updateFiles.lastError().text()));
            }

            QSqlQuery updateGames(db);
            updateGames.prepare("UPDATE games SET system_id = ? WHERE system_id = ?");
            updateGames.addBindValue(info.id);
            updateGames.addBindValue(existingId);
            if (!updateGames.exec()) {
                return rollbackAndFail(
                    QStringLiteral("Migration: Failed to update game system IDs: %1").arg(updateGames.lastError().text()));
            }

            QSqlQuery deleteLegacy(db);
            deleteLegacy.prepare("DELETE FROM systems WHERE id = ?");
            deleteLegacy.addBindValue(existingId);
            if (!deleteLegacy.exec()) {
                return rollbackAndFail(
                    QStringLiteral("Migration: Failed to delete legacy system row: %1").arg(deleteLegacy.lastError().text()));
            }

            continue;
        }

        if (existingId == 0 && database.insertSystem(info) == 0) {
            return rollbackAndFail(
                QStringLiteral("Migration: Failed to backfill missing system row for: %1").arg(info.name));
        }
    }

    QSqlQuery repairFiles(db);
    if (!repairFiles.exec(R"(
        SELECT f.id, f.extension, f.current_path, f.is_compressed, f.archive_internal_path
        FROM files f
        LEFT JOIN systems s ON f.system_id = s.id
        WHERE f.system_id IS NULL OR s.id IS NULL
    )")) {
        return rollbackAndFail(
            QStringLiteral("Migration: Failed to scan files for system repair: %1").arg(repairFiles.lastError().text()));
    }
    while (repairFiles.next()) {
        const int fileId = repairFiles.value(0).toInt();
        const QString extension = repairFiles.value(1).toString();
        const QString currentPath = repairFiles.value(2).toString();
        const bool isCompressed = repairFiles.value(3).toBool();
        const QString archiveInternalPath = repairFiles.value(4).toString();

        const QString detectPath = isCompressed && !archiveInternalPath.isEmpty() ? archiveInternalPath : currentPath;
        const QString systemName = detector.detectSystem(extension, detectPath);
        const int systemId = database.getSystemId(systemName);
        if (systemId == 0)
            continue;

        QSqlQuery updateFile(db);
        updateFile.prepare(QStringLiteral("UPDATE files SET system_id = ? WHERE id = ?"));
        updateFile.addBindValue(systemId);
        updateFile.addBindValue(fileId);
        if (!updateFile.exec()) {
            return rollbackAndFail(
                QStringLiteral("Migration: Failed to repair file system ID: %1").arg(updateFile.lastError().text()));
        }
    }

    return true;
}

} // namespace remustwo
