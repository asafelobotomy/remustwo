#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QUrl>

#include "../catalog/catalog_match.h"
#include "../core/database.h"

class LibraryModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString libraryPath READ libraryPath CONSTANT)
    Q_PROPERTY(QString catalogPath READ catalogPath CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)

public:
    explicit LibraryModel(QObject *parent = nullptr)
        : QObject(parent)
        , m_libraryPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library.db"))
        , m_catalogPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/catalog.db"))
        , m_status(QStringLiteral("Ready")) { }

    QString libraryPath() const {
        return m_libraryPath;
    }
    QString catalogPath() const {
        return m_catalogPath;
    }
    QString status() const {
        return m_status;
    }

    Q_INVOKABLE int fileCount() {
        remustwo::Database db;
        if (!db.initialize(m_libraryPath)) {
            m_status = QStringLiteral("No library database yet");
            emit statusChanged();
            return 0;
        }
        m_status = QStringLiteral("%1 files in library").arg(db.getAllFiles().size());
        emit statusChanged();
        return db.getAllFiles().size();
    }

    Q_INVOKABLE QString matchFile(const QString &path) {
        auto result = remustwo::catalog::matchFile(m_catalogPath, path);
        if (!result) {
            m_status = result.error();
            emit statusChanged();
            return { };
        }
        m_status = QStringLiteral("Matched: %1").arg(result->title);
        emit statusChanged();
        return result->title;
    }

signals:
    void statusChanged();

private:
    QString m_libraryPath;
    QString m_catalogPath;
    QString m_status;
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("remustwo-gui"));
    QCoreApplication::setOrganizationName(QStringLiteral("remustwo"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.4.0"));

    LibraryModel model;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("libraryModel"), &model);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}

#include "main.moc"
