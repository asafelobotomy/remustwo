#include <QtTest>
#include <QTemporaryDir>

#include "rom_paths.h"
#include "../src/core/library_scan.h"

using namespace remustwo;

class LocalTestRomsTest : public QObject {
    Q_OBJECT

private slots:
    void scanLocalTestRomsOrSkip() {
        if (!test::hasLocalTestRoms())
            QSKIP("testroms/ has no local dumps; CI uses testdata/ only");

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto result = scanLibrary(test::testRomsDir(), dir.filePath(QStringLiteral("library.db")));
        QVERIFY2(result, qPrintable(result.error()));
        QVERIFY(result->scanned >= 0);
    }
};

QTEST_MAIN(LocalTestRomsTest)
#include "test_local_testroms.moc"
