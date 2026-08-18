#include "archive_creator.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace remustwo {

bool ArchiveCreator::canCompress(ArchiveFormat format) const {
#ifdef REMUSTWO_HAS_LIBARCHIVE
    return format == ArchiveFormat::ZIP;
#else
    Q_UNUSED(format);
    return false;
#endif
}

#ifndef REMUSTWO_HAS_LIBARCHIVE
CompressionResult ArchiveCreator::compressDirectoryContents(const QString &rootDir, const QString &outputArchive) {
    Q_UNUSED(rootDir);
    CompressionResult result;
    result.outputPath = outputArchive;
    result.error = QStringLiteral("Archive creation requires libarchive (build with -DREMUSTWO_ENABLE_LIBARCHIVE=ON)");
    return result;
}
#endif

} // namespace remustwo
