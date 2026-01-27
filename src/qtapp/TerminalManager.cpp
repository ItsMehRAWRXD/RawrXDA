#include "TerminalManager.h"
#include <QOperatingSystemVersion>

TerminalManager::TerminalManager(QObject* parent)
    : QObject(parent), m_process(new QProcess(this)), m_shellType(PowerShell)
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalManager::onStdoutReady);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalManager::onStderrReady);
    connect(m_process, &QProcess::started, this, &TerminalManager::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &TerminalManager::onProcessFinished);
}

TerminalManager::~TerminalManager() = default;

bool TerminalManager::start(ShellType shell)
{
    if (m_process->state() != QProcess::NotRunning) {
        return false; // already running
    }

    m_shellType = shell;
    QString program;
    QStringList args;

    if (m_shellType == PowerShell) {
        // prefer modern pwsh.exe when available
        program = "pwsh.exe";
        args << "-NoExit" << "-Command" << "-";
    } else {
        program = "cmd.exe";
        args << "/K"; // keep cmd interactive
    }

    m_process->start(program, args);
    return m_process->waitForStarted(3000);
}

void TerminalManager::stop()
{
    if (m_process->state() == QProcess::Running) {
        m_process->terminate();
        if (!m_process->waitForFinished(2000)) {
            m_process->kill();
        }
    }
}

qint64 TerminalManager::pid() const
{
    return m_process->processId();
}

bool TerminalManager::isRunning() const
{
    return m_process->state() == QProcess::Running;
}

void TerminalManager::writeInput(const QByteArray& data)
{
    if (m_process->state() == QProcess::Running) {
        m_process->write(data);
        m_process->write("\n");
        // Qt6 removed flush() - write() already flushes automatically
    }
}

void TerminalManager::onStdoutReady()
{
    auto data = m_process->readAllStandardOutput();
    emit outputReady(data);
}

void TerminalManager::onStderrReady()
{
    auto data = m_process->readAllStandardError();
    emit errorReady(data);
}

void TerminalManager::onProcessStarted()
{
    emit started();
}

void TerminalManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    emit finished(exitCode, status);
}
