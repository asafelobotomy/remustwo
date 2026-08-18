#include "archive_creator.h"

#include <archive.h>
#include <archive_entry.h>
#include <memory>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

namespace remustwo {

namespace {

    struct ArchiveInputEntry {
        QString sourcePath;
        QString archivePath;
    };

    QStringList collectRelativeFilePaths(const QString &rootDir) {
        QStringList entries;
        QDir root(rootDir);
        QDirIterator it(rootDir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            entries << root.relativeFilePath(it.filePath()).replace(QLatin1Char('\\'), QLatin1Char('/'));
        }
        entries.sort();
        return entries;
    }

} // namespace

CompressionResult ArchiveCreator::compressDirectoryContents(const QString &rootDir, const QString &outputArchive) {
    CompressionResult result;
    result.outputPath = outputArchive;

    if (!canCompress(ArchiveFormat::ZIP)) {
        result.error = QStringLiteral("ZIP compression is not available");
        return result;
    }

    QList<ArchiveInputEntry> entries;
    for (const QString &relative : collectRelativeFilePaths(rootDir)) {
        ArchiveInputEntry entry;
        entry.sourcePath = QDir(rootDir).absoluteFilePath(relative);
        entry.archivePath = relative;
        result.originalSize += QFileInfo(entry.sourcePath).size();
        result.inputFiles.append(entry.sourcePath);
        entries.append(entry);
    }
    if (entries.isEmpty()) {
        result.error = QStringLiteral("No files to compress");
        return result;
    }

    if (QFile::exists(outputArchive))
        QFile::remove(outputArchive);

    using ArchivePtr = std::unique_ptr<archive, decltype(&archive_write_free)>;
    ArchivePtr archive(archive_write_new(), archive_write_free);
    archive_write_set_format_zip(archive.get());
    const QByteArray outBytes = outputArchive.toUtf8();
    if (archive_write_open_filename(archive.get(), outBytes.constData()) != ARCHIVE_OK) {
        result.error = QStringLiteral("Failed to create archive: %1")
                           .arg(QString::fromUtf8(archive_error_string(archive.get())));
        return result;
    }

    using EntryPtr = std::unique_ptr<archive_entry, decltype(&archive_entry_free)>;
    for (const ArchiveInputEntry &input : entries) {
        QFile inputFile(input.sourcePath);
        if (!inputFile.open(QIODevice::ReadOnly)) {
            result.error = QStringLiteral("Failed to read %1").arg(input.sourcePath);
            return result;
        }
        const QFileInfo info(input.sourcePath);
        EntryPtr entry(archive_entry_new(), archive_entry_free);
        const QByteArray archivePathBytes = QDir::fromNativeSeparators(input.archivePath).toUtf8();
        archive_entry_set_pathname(entry.get(), archivePathBytes.constData());
        archive_entry_set_size(entry.get(), info.size());
        archive_entry_set_filetype(entry.get(), AE_IFREG);
        archive_entry_set_perm(entry.get(), 0644);
        if (archive_write_header(archive.get(), entry.get()) != ARCHIVE_OK) {
            result.error = QStringLiteral("Failed to write archive header");
            return result;
        }
        char buffer[65536];
        qint64 bytesRead = 0;
        while ((bytesRead = inputFile.read(buffer, sizeof(buffer))) > 0) {
            if (archive_write_data(archive.get(), buffer, static_cast<size_t>(bytesRead)) != bytesRead) {
                result.error = QStringLiteral("Failed to write archive data");
                return result;
            }
        }
        ++result.filesCompressed;
    }

    archive_write_close(archive.get());
    const QFileInfo outInfo(outputArchive);
    result.success = outInfo.exists() && result.filesCompressed > 0;
    result.compressedSize = outInfo.size();
    if (!result.success && result.error.isEmpty())
        result.error = QStringLiteral("Output archive not created");
    return result;
}

} // namespace remustwo
