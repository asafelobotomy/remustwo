#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QUrl>

class LibraryWorker;

class LibraryController : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString libraryPath READ libraryPath CONSTANT)
    Q_PROPERTY(QString catalogPath READ catalogPath CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY statusChanged)

public:
    enum Roles { PathRole = Qt::UserRole + 1, TitleRole, CoverRole };

    explicit LibraryController(QObject *parent = nullptr);
    ~LibraryController() override;

    QString libraryPath() const {
        return m_libraryPath;
    }
    QString catalogPath() const {
        return m_catalogPath;
    }
    QString status() const {
        return m_status;
    }
    bool busy() const {
        return m_busy;
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int fileCount();
    Q_INVOKABLE QString matchFile(const QString &path);
    Q_INVOKABLE QString coverForRow(int row, bool online);
    Q_INVOKABLE void scanFolder(const QUrl &folder);
    Q_INVOKABLE void matchAll();
    Q_INVOKABLE void enrich(bool online, bool dryRun);
    Q_INVOKABLE void verify();
    Q_INVOKABLE void organize(
        const QUrl &dest, bool dryRun, bool bundle, bool includeArt, bool online, const QString &convert);

signals:
    void statusChanged();
    void busyChanged();
    void scanRequested(const QString &path);
    void matchAllRequested();
    void enrichRequested(bool online, bool dryRun);
    void verifyRequested();
    void organizeRequested(const QString &dest, bool dryRun, bool bundle, bool includeArt, bool online,
        const QString &convert);

private slots:
    void onJobFinished(const QString &message);
    void onJobFailed(const QString &error);

private:
    void reload();
    bool beginJob(const QString &message);
    QString toLocalPath(const QUrl &url) const;

    QString m_libraryPath;
    QString m_catalogPath;
    QString m_status;
    bool m_busy = false;
    QStringList m_paths;
    QStringList m_titles;
    QStringList m_covers;
    QStringList m_coverUrls;
    QStringList m_gameIds;
    QThread m_thread;
    LibraryWorker *m_worker = nullptr;
};
