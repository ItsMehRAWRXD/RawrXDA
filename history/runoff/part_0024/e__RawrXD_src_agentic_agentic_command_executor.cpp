#include "agentic_command_executor.h"
#include <QMessageBox>
#include <QDebug>

AgenticCommandExecutor::AgenticCommandExecutor(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
    // Set default auto-approve list
    m_autoApproveList << "npm test" << "cargo check" << "pytest" << "python -m pytest"
                      << "cargo build" << "make" << "cmake --build";

    // Connect process signals
    connect(m_process, &QProcess::readyReadStandardOutput, this, &AgenticCommandExecutor::onProcessReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &AgenticCommandExecutor::onProcessReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AgenticCommandExecutor::onProcessFinished);
}

AgenticCommandExecutor::~AgenticCommandExecutor()
{
}

void AgenticCommandExecutor::setAutoApproveList(const QStringList &commands)
{
    m_autoApproveList = commands;
}

void AgenticCommandExecutor::executeCommand(const QString &command, const QStringList &arguments, bool requireApproval)
{
    // Check if approval is required
    if (requireApproval && !isAutoApproved(command)) {
        if (!requestApproval(command)) {
            qDebug() << "Command execution rejected by user:" << command;
            return;
        }
    }

    m_output.clear();
    emit executionStarted(command);

    // Start the process
    qDebug() << "Executing command:" << command << "with arguments:" << arguments;
    m_process->start(command, arguments);

    if (!m_process->waitForStarted()) {
        qWarning() << "Failed to start process:" << command << m_process->errorString();
        emit executionFinished(false, -1);
    }
}

QString AgenticCommandExecutor::getOutput() const
{
    return m_output;
}

void AgenticCommandExecutor::cancelCommand()
{
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
        qDebug() << "Command execution canceled";
    }
}

void AgenticCommandExecutor::onProcessReadyReadStandardOutput()
{
    QString output = m_process->readAllStandardOutput();
    m_output.append(output);
    emit outputReceived(output);
}

void AgenticCommandExecutor::onProcessReadyReadStandardError()
{
    QString errorOutput = m_process->readAllStandardError();
    m_output.append(errorOutput);
    emit outputReceived(errorOutput);
}

void AgenticCommandExecutor::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    bool success = (exitStatus == QProcess::NormalExit && exitCode == 0);
    qDebug() << "Process finished with exit code:" << exitCode << "success:" << success;
    emit executionFinished(success, exitCode);
}

bool AgenticCommandExecutor::isAutoApproved(const QString &command)
{
    // Check if command is in the auto-approve list
    for (const QString &approvedCmd : m_autoApproveList) {
        if (command.contains(approvedCmd, Qt::CaseInsensitive)) {
            qDebug() << "Command auto-approved:" << command;
            return true;
        }
    }
    return false;
}

bool AgenticCommandExecutor::requestApproval(const QString &command)
{
    // Show a dialog asking for user approval
    QMessageBox::StandardButton result = QMessageBox::question(
        nullptr,
        "Command Execution",
        QString("Execute this command?\n\n%1").arg(command),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    return result == QMessageBox::Yes;
}