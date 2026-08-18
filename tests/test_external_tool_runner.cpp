#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "../src/core/external_tool_runner.h"

using namespace remustwo;

class ExternalToolRunnerTest : public QObject {
    Q_OBJECT

private slots:
    void findToolReturnsNameWhenMissing();
    void findToolPassesThroughAbsolutePaths();
    void findToolHonorsRemustwoToolPath();
    void findToolResolvesChdmanOrSkips();
    void findToolResolvesMaxcsoWhenInstalled();
    void findToolResolvesDolphinToolOrSkips();
};

void ExternalToolRunnerTest::findToolReturnsNameWhenMissing() {
    QCOMPARE(ExternalToolRunner::findTool(QStringLiteral("remustwo-missing-tool-xyz")),
        QStringLiteral("remustwo-missing-tool-xyz"));
}

void ExternalToolRunnerTest::findToolPassesThroughAbsolutePaths() {
    QCOMPARE(ExternalToolRunner::findTool(QStringLiteral("/custom/not-installed-tool")),
        QStringLiteral("/custom/not-installed-tool"));
}

void ExternalToolRunnerTest::findToolHonorsRemustwoToolPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString toolPath = dir.filePath(QStringLiteral("remustwo-fake-tool"));
    QFile tool(toolPath);
    QVERIFY(tool.open(QIODevice::WriteOnly));
    QVERIFY(tool.write("#!/bin/sh\nexit 0\n") > 0);
    tool.close();
    QVERIFY(QFile::setPermissions(toolPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));

    const QByteArray previous = qgetenv("REMUSTWO_TOOL_PATH");
    qputenv("REMUSTWO_TOOL_PATH", QFile::encodeName(dir.path()));
    const QString resolved = ExternalToolRunner::findTool(QStringLiteral("remustwo-fake-tool"));
    if (previous.isNull())
        qunsetenv("REMUSTWO_TOOL_PATH");
    else
        qputenv("REMUSTWO_TOOL_PATH", previous);

    QCOMPARE(QFileInfo(resolved).canonicalFilePath(), QFileInfo(toolPath).canonicalFilePath());
}

void ExternalToolRunnerTest::findToolResolvesChdmanOrSkips() {
    const QString resolved = ExternalToolRunner::findTool(QStringLiteral("chdman"));
    if (resolved == QLatin1String("chdman")) {
        QSKIP("chdman is not installed");
    }
    QVERIFY(QFileInfo(resolved).isExecutable());
}

void ExternalToolRunnerTest::findToolResolvesMaxcsoWhenInstalled() {
    const QString resolved = ExternalToolRunner::findTool(QStringLiteral("maxcso"));
    if (resolved == QLatin1String("maxcso")) {
        QSKIP("maxcso is not installed");
    }
    QVERIFY(QFileInfo(resolved).isExecutable());
}

void ExternalToolRunnerTest::findToolResolvesDolphinToolOrSkips() {
    const QString resolved = ExternalToolRunner::findTool(QStringLiteral("dolphin-tool"));
    if (resolved == QLatin1String("dolphin-tool")) {
        QSKIP("dolphin-tool is not installed");
    }
    QVERIFY(QFileInfo(resolved).isAbsolute());
    QVERIFY(QFileInfo(resolved).isExecutable());
}

QTEST_MAIN(ExternalToolRunnerTest)
#include "test_external_tool_runner.moc"
