#include "pbp_exporter.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace remustwo {

PBPExporter::PBPExporter(QObject *parent)
    : DiscConverter(parent)
    , m_psxPackagerPath("PSXPackager") { }

bool PBPExporter::isPSXPackagerAvailable() const {
    auto result = const_cast<PBPExporter *>(this)->runProcess(findTool(m_psxPackagerPath), QStringList() << "--version", 5000);
    return result.started;
}

QString PBPExporter::getPSXPackagerVersion() const {
    auto result = const_cast<PBPExporter *>(this)->runProcess(findTool(m_psxPackagerPath), QStringList() << "--version", 5000);
    QString output = result.stdOutput.isEmpty() ? result.stdError : result.stdOutput;
    QStringList lines = output.split('\n');
    return lines.isEmpty() ? QString() : lines.first().trimmed();
}

void PBPExporter::setPSXPackagerPath(const QString &path) {
    m_psxPackagerPath = path;
}

ConversionResult PBPExporter::exportToPBP(const QString &sourcePath, const QString &outputPath) {
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(sourcePath, "pbp") : outputPath;

    // PSXPackager <input> <output>
    QStringList args;
    args << sourcePath << output;

    return runToolConversion(findTool(m_psxPackagerPath), args, "PSXPackager", sourcePath, output);
}

} // namespace remustwo
