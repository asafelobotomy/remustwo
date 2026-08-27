#include <QtTest/QtTest>

#include "../src/core/chd_header.h"
#include "../src/core/constants/systems.h"
#include "../src/core/extended_hashes.h"
#include "../src/core/ra_hasher.h"

#include <QTemporaryDir>
#include <QFile>

using namespace remustwo::Constants::Systems;

class ExtendedHashesTest : public QObject {
    Q_OBJECT

private slots:
    void chdHeaderPopulatesChdSha1();
    void raMd5PopulatesForMappedSystem();
};

void ExtendedHashesTest::chdHeaderPopulatesChdSha1() {
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "temp dir");

    const QString path = dir.filePath(QStringLiteral("disc.chd"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));

    QByteArray header(124, '\0');
    header.replace(0, 8, "MComprHD");
    header[12] = 5;
    header[13] = 0;
    header[14] = 0;
    header[15] = 0;
    const QByteArray sha1 = QByteArray::fromHex("0123456789abcdef0123456789abcdef01234567");
    header.replace(64, sha1.size(), sha1);
    QVERIFY(file.write(header) == header.size());
    file.close();

    remustwo::HashResult hashes;
    hashes.success = true;
    remustwo::populateExtendedHashes(hashes, { path, ID_PSX, QStringLiteral(".chd") });
    QCOMPARE(hashes.chdSha1, QStringLiteral("0123456789abcdef0123456789abcdef01234567"));
}

void ExtendedHashesTest::raMd5PopulatesForMappedSystem() {
    QTemporaryDir dir;
    QVERIFY2(dir.isValid(), "temp dir");

    const QString path = dir.filePath(QStringLiteral("game.gba"));
    const QByteArray payload = QByteArrayLiteral("remustwo extended hash test");
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(payload) == payload.size());
    }

    remustwo::HashResult hashes;
    hashes.success = true;
    remustwo::populateExtendedHashes(hashes, { path, ID_GBA, QStringLiteral(".gba") });
    QCOMPARE(hashes.raMd5, remustwo::RaHasher::md5ForPayload(payload, ID_GBA, QStringLiteral(".gba")));
}

QTEST_MAIN(ExtendedHashesTest)
#include "test_extended_hashes.moc"
