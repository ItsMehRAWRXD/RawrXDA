#include "TerminalManager.h"

TerminalManager::TerminalManager(void* parent)
    : void(parent), m_process(new QProcess(this)), m_shellType(PowerShell)
{
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

TerminalManager::~TerminalManager() = default;

bool TerminalManager::start(ShellType shell)
{
    if (m_process->state() != QProcess::NotRunning) {
        return false; // already running
    }

    m_shellType = shell;
    std::string program;
    std::vector<std::string> args;

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

void TerminalManager::writeInput(const std::vector<uint8_t>& data)
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
    outputReady(data);
}

void TerminalManager::onStderrReady()
{
    auto data = m_process->readAllStandardError();
    errorReady(data);
}

void TerminalManager::onProcessStarted()
{
    started();
}

void TerminalManager::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    finished(exitCode, status);
}

