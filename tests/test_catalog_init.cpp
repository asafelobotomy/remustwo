#include <QtTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSqlQuery>

#include "../src/catalog/catalog.h"

using namespace remustwo;

class CatalogInitTest : public QObject {
    Q_OBJECT

private slots:
    void initCreatesSchemaAndSeeds() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("catalog.db"));

        auto result = catalog::init(dbPath);
        QVERIFY2(result, qPrintable(result.error()));

        auto dbResult = catalog::open(dbPath, QStringLiteral("catalog_test"));
        QVERIFY(dbResult);
        QSqlDatabase db = std::move(*dbResult);

        QSqlQuery sigCount(db);
        QVERIFY(sigCount.exec(QStringLiteral("SELECT COUNT(*) FROM game_signatures")));
        QVERIFY(sigCount.next());
        QCOMPARE(sigCount.value(0).toInt(), 0);

        QSqlQuery sysCount(db);
        QVERIFY(sysCount.exec(QStringLiteral("SELECT COUNT(*) FROM systems")));
        QVERIFY(sysCount.next());
        QVERIFY(sysCount.value(0).toInt() > 0);

        db.close();
        QSqlDatabase::removeDatabase(QStringLiteral("catalog_test"));
    }
};

QTEST_MAIN(CatalogInitTest)
#include "test_catalog_init.moc"
