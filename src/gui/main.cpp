#include "../catalog/catalog.h"
#include "../catalog/catalog_match.h"
#include "../core/database.h"
#include "../metadata/artwork_cache.h"

#include <QAbstractListModel>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>

class LibraryListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString libraryPath READ libraryPath CONSTANT)
    Q_PROPERTY(QString catalogPath READ catalogPath CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY statusChanged)

public:
    enum Roles { PathRole = Qt::UserRole + 1, TitleRole, CoverRole };

    explicit LibraryListModel(QObject *parent = nullptr)
        : QAbstractListModel(parent)
        , m_libraryPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/library.db"))
        , m_catalogPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/catalog.db"))
        , m_status(QStringLiteral("Ready")) {
        reload();
    }

    QString libraryPath() const {
        return m_libraryPath;
    }
    QString catalogPath() const {
        return m_catalogPath;
    }
    QString status() const {
        return m_status;
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid())
            return 0;
        return m_paths.size();
    }

    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_paths.size())
            return { };
        switch (role) {
        case PathRole:
            return m_paths.at(index.row());
        case TitleRole:
            return m_titles.at(index.row());
        case CoverRole:
            return m_covers.at(index.row());
        default:
            return { };
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return { { PathRole, "path" }, { TitleRole, "title" }, { CoverRole, "cover" } };
    }

    Q_INVOKABLE int fileCount() {
        reload();
        return m_paths.size();
    }

    Q_INVOKABLE QString matchFile(const QString &path) {
        auto result = remustwo::catalog::matchFile(m_catalogPath, path, m_libraryPath);
        if (!result) {
            m_status = result.error();
            emit statusChanged();
            return { };
        }
        m_status = QStringLiteral("Matched: %1").arg(result->title);
        emit statusChanged();
        reload();
        return result->title;
    }

    Q_INVOKABLE QString coverForRow(int row, bool online) {
        if (row < 0 || row >= m_gameIds.size())
            return { };
        const QString gameId = m_gameIds.at(row);
        const QString cached = remustwo::cachedArtworkPath(gameId);
        if (!cached.isEmpty())
            return QUrl::fromLocalFile(cached).toString();
        const QString url = m_coverUrls.at(row);
        if (url.isEmpty())
            return { };
        auto cachedResult = remustwo::cacheArtwork(QUrl(url), gameId, online);
        if (!cachedResult)
            return url;
        return QUrl::fromLocalFile(*cachedResult).toString();
    }

signals:
    void statusChanged();

private:
    void reload() {
        beginResetModel();
        m_paths.clear();
        m_titles.clear();
        m_covers.clear();
        m_coverUrls.clear();
        m_gameIds.clear();
        remustwo::Database db;
        if (!db.initialize(m_libraryPath)) {
            m_status = QStringLiteral("No library database yet");
            endResetModel();
            emit statusChanged();
            return;
        }
        const auto files = db.getFilesEligibleForOrganize();
        QSqlDatabase catalog;
        if (QFileInfo::exists(m_catalogPath)) {
            auto opened = remustwo::catalog::open(m_catalogPath, QStringLiteral("gui_catalog"));
            if (opened)
                catalog = *opened;
        }
        for (const remustwo::FileRecord &file : files) {
            m_paths.append(file.currentPath);
            m_titles.append(file.baseTitle.isEmpty() ? file.filename : file.baseTitle);
            m_gameIds.append(file.catalogGameId);
            QString coverUrl;
            if (catalog.isOpen() && !file.catalogGameId.isEmpty()) {
                QSqlQuery query(catalog);
                query.prepare(QStringLiteral("SELECT cover_url FROM games WHERE game_id = ?"));
                query.addBindValue(file.catalogGameId);
                if (query.exec() && query.next())
                    coverUrl = query.value(0).toString();
            }
            m_coverUrls.append(coverUrl);
            const QString cached = remustwo::cachedArtworkPath(file.catalogGameId);
            m_covers.append(cached.isEmpty() ? coverUrl : QUrl::fromLocalFile(cached).toString());
        }
        if (catalog.isOpen()) {
            const QString conn = catalog.connectionName();
            catalog.close();
            QSqlDatabase::removeDatabase(conn);
        }
        m_status = QStringLiteral("%1 matched file(s)").arg(m_paths.size());
        endResetModel();
        emit statusChanged();
    }

    QString m_libraryPath;
    QString m_catalogPath;
    QString m_status;
    QStringList m_paths;
    QStringList m_titles;
    QStringList m_covers;
    QStringList m_coverUrls;
    QStringList m_gameIds;
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("remustwo-gui"));
    QCoreApplication::setOrganizationName(QStringLiteral("remustwo"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.5.0"));

    LibraryListModel model;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("libraryModel"), &model);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}

#include "main.moc"
