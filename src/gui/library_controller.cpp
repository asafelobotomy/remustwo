#include "library_controller.h"

#include "../catalog/catalog.h"
#include "../catalog/catalog_match.h"
#include "../core/database.h"
#include "../core/library_paths.h"
#include "../core/library_scan.h"
#include "../core/rom_bundler.h"
#include "../core/verification_engine.h"
#include "../metadata/artwork_cache.h"
#include "../metadata/library_enrich.h"
#include "../metadata/library_organize.h"

#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>

class LibraryWorker : public QObject {
    Q_OBJECT

public:
    LibraryWorker(const QString &catalogPath, const QString &libraryPath, QObject *parent = nullptr)
        : QObject(parent)
        , m_catalogPath(catalogPath)
        , m_libraryPath(libraryPath) { }

public slots:
    void scan(const QString &path) {
        auto result = remustwo::scanLibrary(path, m_libraryPath);
        if (!result) {
            emit failed(result.error());
            return;
        }
        emit finished(QStringLiteral("Scanned %1 file(s), stored %2, hashed %3")
                          .arg(result->scanned)
                          .arg(result->stored)
                          .arg(result->hashed));
    }

    void matchAll() {
        auto result = remustwo::catalog::matchLibrary(m_catalogPath, m_libraryPath);
        if (!result) {
            emit failed(result.error());
            return;
        }
        emit finished(QStringLiteral("Matched %1 library file(s)").arg(*result));
    }

    void enrich(bool online, bool dryRun) {
        remustwo::EnrichOptions options;
        options.online = online;
        options.dryRun = dryRun;
        auto result = remustwo::enrichLibrary(m_catalogPath, m_libraryPath, options);
        if (!result) {
            emit failed(result.error());
            return;
        }
        emit finished(QStringLiteral("Enrich: %1 matched, %2 updated").arg(result->matchedGames).arg(result->updated));
    }

    void verify() {
        remustwo::Database db;
        if (!db.initialize(m_libraryPath)) {
            emit finished(QStringLiteral("verify: no library database yet (ok)"));
            return;
        }
        remustwo::VerificationEngine engine(&db);
        engine.setCompendiumDb(m_catalogPath);
        engine.verifyLibrary();
        const remustwo::VerificationSummary summary = engine.getLastSummary();
        emit finished(QStringLiteral("verify: %1 file(s), %2 verified, %3 mismatched")
                          .arg(summary.totalFiles)
                          .arg(summary.verified)
                          .arg(summary.mismatched));
    }

    void organize(const QString &dest, bool dryRun, bool bundle, bool includeArt, bool online, const QString &convert) {
        remustwo::OrganizeLibraryOptions options;
        options.dryRun = dryRun;
        options.bundle = bundle;
        options.includeArt = includeArt;
        options.online = online;
        options.convert = convert.compare(QStringLiteral("never"), Qt::CaseInsensitive) == 0
            ? remustwo::BundleConvertMode::Never
            : remustwo::BundleConvertMode::Auto;
        auto result = remustwo::organizeLibrary(m_catalogPath, m_libraryPath, dest, options);
        if (!result) {
            emit failed(result.error());
            return;
        }
        QString message = QStringLiteral("Organized %1 file(s)").arg(result->organized);
        if (dryRun)
            message.prepend(QStringLiteral("[dry-run] "));
        emit finished(message);
    }

signals:
    void finished(const QString &message);
    void failed(const QString &error);

private:
    QString m_catalogPath;
    QString m_libraryPath;
};

LibraryController::LibraryController(QObject *parent)
    : QAbstractListModel(parent)
    , m_libraryPath(remustwo::defaultLibraryPath())
    , m_catalogPath(remustwo::defaultCatalogPath())
    , m_status(QStringLiteral("Ready")) {
    m_worker = new LibraryWorker(m_catalogPath, m_libraryPath);
    m_worker->moveToThread(&m_thread);
    connect(this, &LibraryController::scanRequested, m_worker, &LibraryWorker::scan);
    connect(this, &LibraryController::matchAllRequested, m_worker, &LibraryWorker::matchAll);
    connect(this, &LibraryController::enrichRequested, m_worker, &LibraryWorker::enrich);
    connect(this, &LibraryController::verifyRequested, m_worker, &LibraryWorker::verify);
    connect(this, &LibraryController::organizeRequested, m_worker, &LibraryWorker::organize);
    connect(m_worker, &LibraryWorker::finished, this, &LibraryController::onJobFinished);
    connect(m_worker, &LibraryWorker::failed, this, &LibraryController::onJobFailed);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread.start();
    reload();
}

LibraryController::~LibraryController() {
    m_thread.quit();
    m_thread.wait(5000);
}

int LibraryController::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_paths.size();
}

QVariant LibraryController::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_paths.size())
        return {};
    switch (role) {
    case PathRole:
        return m_paths.at(index.row());
    case TitleRole:
        return m_titles.at(index.row());
    case CoverRole:
        return m_covers.at(index.row());
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryController::roleNames() const {
    return { { PathRole, "path" }, { TitleRole, "title" }, { CoverRole, "cover" } };
}

int LibraryController::fileCount() {
    reload();
    return m_paths.size();
}

QString LibraryController::matchFile(const QString &path) {
    auto result = remustwo::catalog::matchFile(m_catalogPath, path, m_libraryPath);
    if (!result) {
        m_status = result.error();
        emit statusChanged();
        return {};
    }
    m_status = QStringLiteral("Matched: %1").arg(result->title);
    emit statusChanged();
    reload();
    return result->title;
}

QString LibraryController::coverForRow(int row, bool online) {
    if (row < 0 || row >= m_gameIds.size())
        return {};
    const QString gameId = m_gameIds.at(row);
    const QString cached = remustwo::cachedArtworkPath(gameId);
    if (!cached.isEmpty())
        return QUrl::fromLocalFile(cached).toString();
    const QString url = m_coverUrls.at(row);
    if (url.isEmpty())
        return {};
    auto cachedResult = remustwo::cacheArtwork(QUrl(url), gameId, online);
    if (!cachedResult)
        return url;
    return QUrl::fromLocalFile(*cachedResult).toString();
}

void LibraryController::scanFolder(const QUrl &folder) {
    const QString path = toLocalPath(folder);
    if (path.isEmpty() || !beginJob(QStringLiteral("Scanning…")))
        return;
    emit scanRequested(path);
}

void LibraryController::matchAll() {
    if (!beginJob(QStringLiteral("Matching…")))
        return;
    emit matchAllRequested();
}

void LibraryController::enrich(bool online, bool dryRun) {
    if (!beginJob(QStringLiteral("Enriching…")))
        return;
    emit enrichRequested(online, dryRun);
}

void LibraryController::verify() {
    if (!beginJob(QStringLiteral("Verifying…")))
        return;
    emit verifyRequested();
}

void LibraryController::organize(
    const QUrl &dest, bool dryRun, bool bundle, bool includeArt, bool online, const QString &convert) {
    const QString path = toLocalPath(dest);
    if (path.isEmpty() || !beginJob(QStringLiteral("Organizing…")))
        return;
    emit organizeRequested(path, dryRun, bundle, includeArt, online, convert);
}

void LibraryController::onJobFinished(const QString &message) {
    m_busy = false;
    m_status = message;
    emit busyChanged();
    emit statusChanged();
    reload();
}

void LibraryController::onJobFailed(const QString &error) {
    m_busy = false;
    m_status = error;
    emit busyChanged();
    emit statusChanged();
}

bool LibraryController::beginJob(const QString &message) {
    if (m_busy)
        return false;
    m_busy = true;
    m_status = message;
    emit busyChanged();
    emit statusChanged();
    return true;
}

QString LibraryController::toLocalPath(const QUrl &url) const {
    if (url.isLocalFile())
        return url.toLocalFile();
    return url.toString();
}

void LibraryController::reload() {
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
    if (!m_busy)
        m_status = QStringLiteral("%1 matched file(s)").arg(m_paths.size());
    endResetModel();
    emit statusChanged();
}

#include "library_controller.moc"
