#include "external_tool_runner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace remustwo {

namespace {

bool isRunnableFile(const QString &path) {
    const QFileInfo info(path);
    return info.isFile() && info.isExecutable();
}

void addDir(QStringList &dirs, const QString &path) {
    if (path.isEmpty())
        return;
    const QString cleaned = QDir::cleanPath(path);
    if (cleaned.isEmpty() || dirs.contains(cleaned))
        return;
    if (QFileInfo(cleaned).isDir())
        dirs.append(cleaned);
}

void addEnvPathList(QStringList &dirs, const QString &value) {
    const QChar sep = QDir::listSeparator();
    for (const QString &part : value.split(sep, Qt::SkipEmptyParts))
        addDir(dirs, part);
}

void addAncestorSiblings(QStringList &dirs, const QString &start) {
    if (start.isEmpty())
        return;
    QDir dir(start);
    for (int i = 0; i < 8; ++i) {
        addDir(dirs, dir.absoluteFilePath(QStringLiteral("remustwo-maxcso")));
        const QString here = dir.absolutePath();
        if (here == QDir::rootPath() || !dir.cdUp())
            break;
    }
}

QString searchDirs(const QString &name, const QStringList &dirs) {
    if (dirs.isEmpty())
        return {};
    return QStandardPaths::findExecutable(name, dirs);
}

QString hostProgram(const QString &name) {
    const QString onPath = QStandardPaths::findExecutable(name);
    if (!onPath.isEmpty())
        return onPath;
    for (const QString &dir : { QStringLiteral("/usr/bin"), QStringLiteral("/usr/local/bin"), QStringLiteral("/bin") }) {
        const QString candidate = QDir(dir).filePath(name);
        if (isRunnableFile(candidate))
            return QFileInfo(candidate).absoluteFilePath();
    }
    return {};
}

bool isSafeToken(const QString &token) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9._+-]+$"));
    return re.match(token).hasMatch();
}

bool skipFlatpakAppId(const QString &appId) {
    return appId.contains(QLatin1String(".Sdk")) || appId.contains(QLatin1String(".Platform"))
        || appId.contains(QLatin1String(".Debug")) || appId.endsWith(QLatin1String(".Builder"))
        || appId == QLatin1String("org.flatpak.Builder");
}

QString wrapperDir() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.cache");
    return base + QStringLiteral("/remustwo/tool-wrappers");
}

QString wrapperPathFor(const QString &name) {
#ifdef Q_OS_WIN
    return QDir(wrapperDir()).filePath(name + QStringLiteral(".cmd"));
#else
    return QDir(wrapperDir()).filePath(name);
#endif
}

bool writeWrapper(const QString &path, const QByteArray &content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile existing(path);
    if (existing.exists() && existing.open(QIODevice::ReadOnly) && existing.readAll() == content) {
        existing.close();
        return QFile::setPermissions(path,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    if (file.write(content) != content.size())
        return false;
    if (!file.commit())
        return false;
    return QFile::setPermissions(path,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup
            | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
}

QString makeFlatpakWrapper(const QString &toolName, const QString &appId) {
    if (!isSafeToken(toolName) || !isSafeToken(appId))
        return {};
    const QString flatpak = hostProgram(QStringLiteral("flatpak"));
    if (flatpak.isEmpty())
        return {};

    QByteArray script;
#ifdef Q_OS_WIN
    script = "@echo off\r\n\"";
    script += flatpak.toUtf8();
    script += "\" run --command=";
    script += toolName.toUtf8();
    script += " ";
    script += appId.toUtf8();
    script += " %*\r\n";
#else
    script = "#!/bin/sh\nexec \"";
    script += QFile::encodeName(flatpak);
    script += "\" run --command=";
    script += toolName.toUtf8();
    script += " ";
    script += appId.toUtf8();
    script += " \"$@\"\n";
#endif
    const QString path = wrapperPathFor(toolName);
    if (!writeWrapper(path, script))
        return {};
    return path;
}

QStringList flatpakAppRoots() {
    QStringList roots;
    addDir(roots, QDir::homePath() + QStringLiteral("/.local/share/flatpak/app"));
    addDir(roots, QStringLiteral("/var/lib/flatpak/app"));
    return roots;
}

bool flatpakAppInstalled(const QString &appId) {
    for (const QString &root : flatpakAppRoots()) {
        if (QFileInfo(QDir(root).filePath(appId)).isDir())
            return true;
    }
    return false;
}

bool flatpakAppHasCommand(const QString &appId, const QString &toolName) {
    for (const QString &root : flatpakAppRoots()) {
        const QDir appDir(QDir(root).filePath(appId));
        const QStringList candidates = {
            appDir.filePath(QStringLiteral("current/active/files/bin/") + toolName),
            appDir.filePath(QStringLiteral("current/active/files/usr/bin/") + toolName),
        };
        for (const QString &candidate : candidates) {
            if (QFileInfo::exists(candidate))
                return true;
        }
    }
    return false;
}

QString preferredFlatpakApp(const QString &toolName) {
    if (toolName == QLatin1String("dolphin-tool") || toolName == QLatin1String("dolphin-emu"))
        return QStringLiteral("org.DolphinEmu.dolphin-emu");
    if (toolName == QLatin1String("chdman"))
        return QStringLiteral("org.mamedev.MAME");
    return {};
}

QString findFlatpakAppForTool(const QString &toolName) {
    const QString preferred = preferredFlatpakApp(toolName);
    if (!preferred.isEmpty() && flatpakAppInstalled(preferred)
        && (flatpakAppHasCommand(preferred, toolName) || toolName == QLatin1String("dolphin-tool"))) {
        return preferred;
    }

    QString fallback;
    for (const QString &root : flatpakAppRoots()) {
        const QStringList apps = QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &appId : apps) {
            if (!isSafeToken(appId) || skipFlatpakAppId(appId))
                continue;
            if (!flatpakAppHasCommand(appId, toolName))
                continue;
            if (appId.contains(toolName, Qt::CaseInsensitive)
                || (toolName.contains(QLatin1String("dolphin")) && appId.contains(QLatin1String("dolphin"), Qt::CaseInsensitive))
                || (toolName == QLatin1String("chdman") && appId.contains(QLatin1String("mame"), Qt::CaseInsensitive))) {
                return appId;
            }
            if (fallback.isEmpty())
                fallback = appId;
        }
    }
    return fallback;
}

QString findSnapAlias(const QString &toolName) {
    const QStringList snapBins = {
        QStringLiteral("/snap/bin"),
        QDir::homePath() + QStringLiteral("/snap/bin"),
    };
    for (const QString &dirPath : snapBins) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        const QString direct = dir.filePath(toolName);
        if (isRunnableFile(direct))
            return QFileInfo(direct).absoluteFilePath();
        const QStringList matches = dir.entryList({ QStringLiteral("*.") + toolName }, QDir::Files);
        for (const QString &match : matches) {
            const QString candidate = dir.filePath(match);
            if (isRunnableFile(candidate))
                return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return {};
}

QStringList userSearchDirs() {
    QStringList dirs;
    addEnvPathList(dirs, qEnvironmentVariable("REMUSTWO_TOOL_PATH"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/.local/bin"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/bin"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/.linuxbrew/bin"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/.nix-profile/bin"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/.guix-profile/bin"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/.cargo/bin"));
    addDir(dirs, QStringLiteral("/home/linuxbrew/.linuxbrew/bin"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/git/remustwo-maxcso"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/src/remustwo-maxcso"));

    if (QCoreApplication::instance()) {
        const QString appDir = QCoreApplication::applicationDirPath();
        addDir(dirs, appDir);
        addDir(dirs, appDir + QStringLiteral("/tools"));
        addAncestorSiblings(dirs, appDir);
    }
    addAncestorSiblings(dirs, QDir::currentPath());
    return dirs;
}

QStringList systemSearchDirs() {
    QStringList dirs;
    addDir(dirs, QStringLiteral("/usr/games"));
    addDir(dirs, QStringLiteral("/usr/local/games"));
    addDir(dirs, QStringLiteral("/usr/local/bin"));
    addDir(dirs, QStringLiteral("/opt/bin"));
    addDir(dirs, QStringLiteral("/opt/local/bin"));
    addDir(dirs, QStringLiteral("/opt/homebrew/bin"));
    addDir(dirs, QStringLiteral("/nix/var/nix/profiles/default/bin"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/bin"));
    addDir(dirs, QStringLiteral("/var/lib/flatpak/exports/bin"));
    addDir(dirs, QStringLiteral("/snap/bin"));
    addDir(dirs, QDir::homePath() + QStringLiteral("/snap/bin"));
    addDir(dirs, QStringLiteral("/Applications/Dolphin.app/Contents/MacOS"));
    addDir(dirs, QStringLiteral("/opt/homebrew/opt/mame/bin"));
    addDir(dirs, QStringLiteral("/usr/local/opt/mame/bin"));

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString programFiles = env.value(QStringLiteral("ProgramFiles"));
    const QString programFilesX86 = env.value(QStringLiteral("ProgramFiles(x86)"));
    const QString localAppData = env.value(QStringLiteral("LOCALAPPDATA"));
    addDir(dirs, programFiles + QStringLiteral("/Dolphin"));
    addDir(dirs, programFiles + QStringLiteral("/MAME"));
    addDir(dirs, programFilesX86 + QStringLiteral("/Dolphin"));
    addDir(dirs, programFilesX86 + QStringLiteral("/MAME"));
    addDir(dirs, localAppData + QStringLiteral("/Programs/Dolphin"));
    addDir(dirs, localAppData + QStringLiteral("/Programs/MAME"));
    return dirs;
}

} // namespace

QString ExternalToolRunner::findTool(const QString &name) {
    if (name.isEmpty())
        return name;
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')))
        return name;

    const QFileInfo direct(name);
    if (direct.isAbsolute() && direct.isExecutable())
        return name;

    const QString fromUserDirs = searchDirs(name, userSearchDirs());
    if (!fromUserDirs.isEmpty())
        return fromUserDirs;

    const QString onPath = QStandardPaths::findExecutable(name);
    if (!onPath.isEmpty())
        return onPath;

    const QString fromSystemDirs = searchDirs(name, systemSearchDirs());
    if (!fromSystemDirs.isEmpty())
        return fromSystemDirs;

    const QString snapAlias = findSnapAlias(name);
    if (!snapAlias.isEmpty())
        return snapAlias;

    const QString flatpakApp = findFlatpakAppForTool(name);
    if (!flatpakApp.isEmpty()) {
        const QString wrapper = makeFlatpakWrapper(name, flatpakApp);
        if (!wrapper.isEmpty())
            return wrapper;
    }

    return name;
}

ExternalToolRunner::ExternalToolRunner(QObject *parent)
    : QObject(parent) { }

void ExternalToolRunner::cancel() {
    m_cancelled = true;
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->terminate();
        m_process->waitForFinished(3000);
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
        }
    }
}

bool ExternalToolRunner::isRunning() const {
    return m_process && m_process->state() == QProcess::Running;
}

ExternalToolRunner::ProcessResult ExternalToolRunner::runProcess(
    const QString &program, const QStringList &args, int timeoutMs) {
    ProcessResult result;
    QProcess process;

    process.start(program, args);
    result.started = process.waitForStarted(timeoutMs);
    if (!result.started) {
        result.exitCode = -1;
        if (result.stdError.isEmpty()) {
            result.stdError = QStringLiteral("Failed to start process: %1").arg(program);
        }
        return result;
    }

    result.finished = process.waitForFinished(timeoutMs);
    if (!result.finished) {
        // Ensure timed-out processes do not continue running in the background
        process.kill();
        process.waitForFinished(3000);
        if (result.stdError.isEmpty()) {
            result.stdError = QStringLiteral("Process timed out after %1 ms: %2").arg(timeoutMs).arg(program);
        }
    }
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.stdOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.stdError = QString::fromUtf8(process.readAllStandardError());
    return result;
}

ExternalToolRunner::ProcessResult ExternalToolRunner::runProcessTracked(
    const QString &program, const QStringList &args, int timeoutMs) {
    // Reset cancellation state so a previous cancel() call does not suppress
    // the timeout-kill path on the next tracked run.
    m_cancelled = false;
    ProcessResult result;
    QProcess process;
    m_process = &process;

    process.start(program, args);
    result.started = process.waitForStarted(10000);
    if (!result.started) {
        m_process = nullptr;
        result.exitCode = -1;
        if (result.stdError.isEmpty()) {
            result.stdError = QStringLiteral("Failed to start process: %1").arg(program);
        }
        return result;
    }

    result.finished = process.waitForFinished(timeoutMs);
    if (!result.finished && !m_cancelled) {
        // Timed out without an explicit cancel request; kill the child process
        process.kill();
        process.waitForFinished(3000);
        if (result.stdError.isEmpty()) {
            result.stdError = QStringLiteral("Process timed out after %1 ms: %2").arg(timeoutMs).arg(program);
        }
    }
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.stdOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.stdError = QString::fromUtf8(process.readAllStandardError());
    m_process = nullptr;
    return result;
}

} // namespace remustwo
