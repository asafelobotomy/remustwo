#include "archive_extractor.h"

#include <QDir>
#include <QFileInfo>

namespace remustwo {

ArchiveExtractor::ArchiveExtractor(QObject *parent)
    : QObject(parent) { }

bool ArchiveExtractor::canExtract(ArchiveFormat format) const {
#ifdef REMUSTWO_HAS_LIBARCHIVE
    return format != ArchiveFormat::Unknown;
#else
    Q_UNUSED(format);
    return false;
#endif
}

bool ArchiveExtractor::canExtract(const QString &path) const {
    return canExtract(detectFormat(path));
}

#ifndef REMUSTWO_HAS_LIBARCHIVE
ArchiveInfo ArchiveExtractor::getArchiveInfo(const QString &path) {
    ArchiveInfo info;
    info.path = path;
    info.format = detectFormat(path);
    return info;
}
#endif

ArchiveFormat ArchiveExtractor::detectFormat(const QString &path) {
    const QString name = QFileInfo(path).fileName().toLower();
    if (name.endsWith(QStringLiteral(".tar.gz")) || name.endsWith(QStringLiteral(".tgz")))
        return ArchiveFormat::TarGz;
    if (name.endsWith(QStringLiteral(".tar.bz2")) || name.endsWith(QStringLiteral(".tbz2"))
        || name.endsWith(QStringLiteral(".bz2")))
        return ArchiveFormat::TarBz2;
    if (name.endsWith(QStringLiteral(".tar.xz")) || name.endsWith(QStringLiteral(".xz")))
        return ArchiveFormat::TarXz;
    if (name.endsWith(QStringLiteral(".tar")))
        return ArchiveFormat::Tar;
    if (name.endsWith(QStringLiteral(".gz")))
        return ArchiveFormat::GZip;
    if (name.endsWith(QStringLiteral(".zip")))
        return ArchiveFormat::ZIP;
    if (name.endsWith(QStringLiteral(".7z")))
        return ArchiveFormat::SevenZip;
    if (name.endsWith(QStringLiteral(".rar")))
        return ArchiveFormat::RAR;
    return ArchiveFormat::Unknown;
}

QString ArchiveExtractor::normalizeArchiveMemberPath(const QString &path) {
    QString normalized = QDir::fromNativeSeparators(path.trimmed());
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (normalized.isEmpty() || normalized.startsWith(QLatin1Char('/')))
        return {};
    while (normalized.startsWith(QStringLiteral("./")))
        normalized.remove(0, 2);
    if (normalized.isEmpty() || normalized == QLatin1Char('.'))
        return {};
    const QStringList parts = normalized.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part == QStringLiteral(".."))
            return {};
    }
    return normalized;
}

#ifndef REMUSTWO_HAS_LIBARCHIVE
ExtractionResult ArchiveExtractor::extract(const QString &archivePath, const QString &, bool) {
    ExtractionResult result;
    result.archivePath = archivePath;
    result.error
        = QStringLiteral("Archive extraction requires libarchive (build with -DREMUSTWO_ENABLE_LIBARCHIVE=ON)");
    return result;
}

ExtractionResult ArchiveExtractor::extractFile(const QString &archivePath, const QString &, const QString &) {
    return extract(archivePath);
}

QList<ExtractionResult> ArchiveExtractor::batchExtract(const QStringList &, const QString &, bool) {
    return {};
}

QByteArray ArchiveExtractor::readMemberPrefix(const QString &, const QString &, qint64) {
    return {};
}

ExtractionResult ArchiveExtractor::extractToDir(const QString &archivePath, const QString &, const QString &) {
    return extract(archivePath);
}
#endif

} // namespace remustwo
