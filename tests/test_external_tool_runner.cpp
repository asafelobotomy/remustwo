#include <QtTest/QtTest>
#include <QStandardPaths>

#include "../src/core/external_tool_runner.h"

using namespace remustwo;

class ExternalToolRunnerTest : public QObject {
    Q_OBJECT

private slots:
    void findToolReturnsNameWhenMissing();
    void findToolResolvesChdmanOrSkips();
};

void ExternalToolRunnerTest::findToolReturnsNameWhenMissing() {
    QCOMPARE(ExternalToolRunner::findTool(QStringLiteral("remustwo-missing-tool-xyz")),
        QStringLiteral("remustwo-missing-tool-xyz"));
}

void ExternalToolRunnerTest::findToolResolvesChdmanOrSkips() {
    const QString resolved = QStandardPaths::findExecutable(QStringLiteral("chdman"));
    if (resolved.isEmpty()) {
        QSKIP("chdman is not installed");
    }
    QCOMPARE(ExternalToolRunner::findTool(QStringLiteral("chdman")), resolved);
}

QTEST_MAIN(ExternalToolRunnerTest)
#include "test_external_tool_runner.moc"
