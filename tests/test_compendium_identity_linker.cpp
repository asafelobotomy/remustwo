#include <QtTest/QtTest>

#include "../src/catalog/compendium_identity_linker.h"
#include "../src/core/constants/system_ids.h"

using namespace remustwo;
using namespace remustwo::Compendium;

class CompendiumIdentityLinkerTest : public QObject {
    Q_OBJECT

private:
    static SourceRecordEnvelope envelope(const QString &title, const QString &sha1, const QString &serial, int systemId,
        const QString &region) {
        SourceRecordEnvelope rec;
        rec.titleRaw = title;
        rec.hashes.sha1 = sha1;
        rec.serials.append(serial);
        rec.resolvedSystemId = systemId;
        rec.resolvedRegionCode = region;
        rec.externalKey = title;
        return rec;
    }

private slots:
    void serialDoesNotMergeBetaWithRetail();
    void serialMergesMultiDiscSet();
};

void CompendiumIdentityLinkerTest::serialDoesNotMergeBetaWithRetail() {
    QList<SourceRecordEnvelope> records;
    records.append(envelope(QStringLiteral("Castlevania - Bloodlines (USA)"),
        QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), QStringLiteral("T-95076-00"),
        Constants::Systems::ID_GENESIS, QStringLiteral("USA")));
    records.append(envelope(QStringLiteral("Castlevania - Bloodlines (USA) (Beta 1)"),
        QStringLiteral("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"), QStringLiteral("T-95076-00"),
        Constants::Systems::ID_GENESIS, QStringLiteral("USA")));

    IdentityLinker linker;
    QCOMPARE(linker.link(records), 2);
    QVERIFY(!records[0].linkedGameId.isEmpty());
    QVERIFY(records[0].linkedGameId != records[1].linkedGameId);
}

void CompendiumIdentityLinkerTest::serialMergesMultiDiscSet() {
    QList<SourceRecordEnvelope> records;
    records.append(envelope(QStringLiteral("Metal Gear Solid (USA) (Disc 1)"),
        QStringLiteral("cccccccccccccccccccccccccccccccccccccccc"), QStringLiteral("SLUS-00594"),
        Constants::Systems::ID_PSX, QStringLiteral("USA")));
    records.append(envelope(QStringLiteral("Metal Gear Solid (USA) (Disc 2)"),
        QStringLiteral("dddddddddddddddddddddddddddddddddddddddd"), QStringLiteral("SLUS-00594"),
        Constants::Systems::ID_PSX, QStringLiteral("USA")));

    IdentityLinker linker;
    QCOMPARE(linker.link(records), 1);
    QCOMPARE(records[0].linkedGameId, records[1].linkedGameId);
}

QTEST_MAIN(CompendiumIdentityLinkerTest)
#include "test_compendium_identity_linker.moc"
