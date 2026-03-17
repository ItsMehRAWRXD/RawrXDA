#include "sandbox.h"
#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QDir>
#include <QDebug>

Sandbox::Sandbox(QObject *parent)
    : QObject(parent)
{
}

Sandbox::~Sandbox()
{
}

void Sandbox::setAllowList(const QStringList &allowList)
{
    m_allowList = allowList;
}

bool Sandbox::executeCommand(const QString &command, const QStringList &arguments)
{
    // Check if command is in allow-list
    if (!m_allowList.isEmpty() && !m_allowList.contains(command)) {
        qWarning() << "Command not in allow-list:" << command;
        return false;
    }
    
    // Clear previous output
    m_output.clear();
    
    // Execute command based on platform
#ifdef Q_OS_WIN
    return executeCommandWindows(command, arguments);
#else
    return executeCommandLinux(command, arguments);
#endif
}

QString Sandbox::getOutput() const
{
    return m_output;
}

bool Sandbox::executeCommandWindows(const QString &command, const QStringList &arguments)
{
    QProcess process;
    process.start(command, arguments);
    
    if (!process.waitForStarted()) {
        qWarning() << "Failed to start process:" << command << process.errorString();
        return false;
    }
    
    if (!process.waitForFinished(30000)) { // 30 second timeout
        qWarning() << "Process timed out:" << command;
        process.kill();
        return false;
    }
    
    m_output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();
    
    if (!errorOutput.isEmpty()) {
        qWarning() << "Process error output:" << errorOutput;
    }
    
    return process.exitCode() == 0;
}

bool Sandbox::executeCommandLinux(const QString &command, const QStringList &arguments)
{
    // This is a simplified implementation. A real implementation would use chroot.
    // For this example, we'll just execute the command directly.
    QProcess process;
    process.start(command, arguments);
    
    if (!process.waitForStarted()) {
        qWarning() << "Failed to start process:" << command << process.errorString();
        return false;
    }
    
    if (!process.waitForFinished(30000)) { // 30 second timeout
        qWarning() << "Process timed out:" << command;
        process.kill();
        return false;
    }
    
    m_output = process.readAllStandardOutput();
    QByteArray errorOutput = process.readAllStandardError();
    
    if (!errorOutput.isEmpty()) {
        qWarning() << "Process error output:" << errorOutput;
    }
    
    return process.exitCode() == 0;
}