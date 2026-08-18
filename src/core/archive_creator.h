#pragma once

#include "archive_extractor.h"

#include <QString>
#include <QStringList>

namespace remustwo {

struct CompressionResult {
    bool success = false;
    QString outputPath;
    QString error;
    qint64 originalSize = 0;
    qint64 compressedSize = 0;
    int filesCompressed = 0;
    QStringList inputFiles;
};

class ArchiveCreator {
public:
    bool canCompress(ArchiveFormat format = ArchiveFormat::ZIP) const;

    CompressionResult compressDirectoryContents(const QString &rootDir, const QString &outputArchive);
};

} // namespace remustwo
