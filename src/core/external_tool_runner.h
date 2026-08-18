#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

namespace remustwo {

/**
 * @brief Base class for wrappers around external CLI tools (chdman, 7z, unzip, etc.)
 *
 * Provides shared process execution, cancellation, and status tracking.
 */
class ExternalToolRunner : public QObject {
    Q_OBJECT

public:
    explicit ExternalToolRunner(QObject *parent = nullptr);

    virtual void cancel();
    bool isRunning() const;

    /// Resolve a CLI tool to an executable. Searches REMUSTWO_TOOL_PATH, ~/.local/bin,
    /// PATH (.deb/AUR/pacman/Homebrew), /usr/games, Snap aliases, Nix/Guix, and Flatpak
    /// (writes a cache wrapper for extra commands such as dolphin-tool). Returns @p name
    /// unchanged when nothing is found, or when @p name is already a path.
    static QString findTool(const QString &name);

protected:
    struct ProcessResult {
        bool started = false;
        bool finished = false;
        int exitCode = -1;
        QProcess::ExitStatus exitStatus = QProcess::NormalExit;
        QString stdOutput;
        QString stdError;
    };

    virtual ProcessResult runProcess(const QString &program, const QStringList &args, int timeoutMs);
    virtual ProcessResult runProcessTracked(const QString &program, const QStringList &args, int timeoutMs);

    bool m_cancelled = false;
    QProcess *m_process = nullptr;
};

} // namespace remustwo
