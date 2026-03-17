#include "real_time_terminal_pool.hpp"
#include <QDebug>
#include <QDateTime>

// TerminalSession Implementation

TerminalSession::TerminalSession(
    int sessionId,
    ShellType shellType,
    const QString& workingDir,
    QObject* parent)
    : QObject(parent),
      m_sessionId(sessionId),
      m_shellType(shellType),
      m_workingDir(workingDir),
      m_state(Idle),
      m_exitCode(-1),
      m_maxHistoryLines(5000) {
    
    m_process = std::make_unique<QProcess>(this);
    
    connect(m_process.get(), &QProcess::readyReadStandardOutput,
            this, &TerminalSession::onReadyReadStandardOutput);
    connect(m_process.get(), &QProcess::readyReadStandardError,
            this, &TerminalSession::onReadyReadStandardError);
        connect(m_process.get(), &QProcess::finished,
            this, &TerminalSession::onProcessFinished);
        connect(m_process.get(), &QProcess::errorOccurred,
            this, &TerminalSession::onProcessError);
    
    qDebug() << "[TerminalSession" << sessionId << "] Created with shell type" << shellType;
}

TerminalSession::~TerminalSession() {
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
    qDebug() << "[TerminalSession" << m_sessionId << "] Destroyed";
}

bool TerminalSession::start() {
    QMutexLocker lock(&m_mutex);
    
    if (m_state == Running) {
        return true;
    }
    
    QString program;
    QStringList arguments;
    
    if (m_shellType == PowerShell) {
        program = "pwsh.exe";
        arguments << "-NoExit";
    } else if (m_shellType == CommandPrompt) {
        program = "cmd.exe";
        arguments << "/k";
    } else if (m_shellType == Bash) {
        program = "bash";
        arguments << "-i";
    } else if (m_shellType == WSL) {
        program = "wsl";
    }
    
    if (!m_workingDir.isEmpty()) {
        m_process->setWorkingDirectory(m_workingDir);
    }
    
    m_process->start(program, arguments);
    
    if (m_process->waitForStarted()) {
        m_state = Running;
        emit started();
        emit stateChanged(Running);
        qDebug() << "[TerminalSession" << m_sessionId << "] Started successfully";
        return true;
    } else {
        m_state = Error;
        emit stateChanged(Error);
        emit errorReceived("Failed to start terminal");
        qDebug() << "[TerminalSession" << m_sessionId << "] Failed to start";
        return false;
    }
}

bool TerminalSession::isRunning() const {
    QMutexLocker lock(&m_mutex);
    return m_state == Running && m_process->state() == QProcess::Running;
}

void TerminalSession::executeCommand(const QString& command) {
    QMutexLocker lock(&m_mutex);
    
    if (!isRunning()) {
        return;
    }
    
    QString fullCommand = command + "\n";
    m_process->write(fullCommand.toLocal8Bit());
    m_process->waitForBytesWritten();
    
    qDebug() << "[TerminalSession" << m_sessionId << "] Executed:" << command;
}

void TerminalSession::stop() {
    QMutexLocker lock(&m_mutex);
    
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
        m_state = Completed;
        emit stateChanged(Completed);
    }
}

void TerminalSession::kill() {
    QMutexLocker lock(&m_mutex);
    
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_state = Completed;
        emit stateChanged(Completed);
    }
}

QString TerminalSession::getOutput() const {
    QMutexLocker lock(&m_mutex);
    return m_output;
}

QString TerminalSession::getError() const {
    QMutexLocker lock(&m_mutex);
    return m_error;
}

int TerminalSession::getExitCode() const {
    QMutexLocker lock(&m_mutex);
    return m_exitCode;
}

TerminalSession::State TerminalSession::getState() const {
    QMutexLocker lock(&m_mutex);
    return m_state;
}

QJsonObject TerminalSession::getStatistics() const {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject stats;
    stats["sessionId"] = m_sessionId;
    stats["state"] = (int)m_state;
    stats["outputLines"] = m_outputHistory.size();
    stats["exitCode"] = m_exitCode;
    
    return stats;
}

void TerminalSession::onReadyReadStandardOutput() {
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromLocal8Bit(data);
    
    {
        QMutexLocker lock(&m_mutex);
        m_output += output;
        m_outputHistory.append(output);
        
        if (m_outputHistory.size() > m_maxHistoryLines) {
            m_outputHistory.pop_front();
        }
    }
    
    emit outputReceived(output);
    qDebug() << "[TerminalSession" << m_sessionId << "] Output received" << output.length() << "bytes";
}

void TerminalSession::onReadyReadStandardError() {
    QByteArray data = m_process->readAllStandardError();
    QString error = QString::fromLocal8Bit(data);
    
    {
        QMutexLocker lock(&m_mutex);
        m_error += error;
    }
    
    emit errorReceived(error);
    qDebug() << "[TerminalSession" << m_sessionId << "] Error received" << error.length() << "bytes";
}

void TerminalSession::onProcessFinished(int exitCode, QProcess::ExitStatus /*exitStatus*/) {
    QMutexLocker lock(&m_mutex);
    m_exitCode = exitCode;
    m_state = Completed;
    
    emit finished(exitCode);
    emit stateChanged(Completed);
    qDebug() << "[TerminalSession" << m_sessionId << "] Finished with exit code" << exitCode;
}

void TerminalSession::onProcessError(QProcess::ProcessError /*error*/) {
    QMutexLocker lock(&m_mutex);
    m_state = Error;
    
    emit stateChanged(Error);
    emit errorReceived("Process error occurred");
    qDebug() << "[TerminalSession" << m_sessionId << "] Process error:" << m_process->errorString();
}

// RealTimeTerminalPool Implementation

RealTimeTerminalPool::RealTimeTerminalPool(int poolSize, QObject* parent)
    : QObject(parent),
      m_nextTerminalId(0),
      m_activeTerminalId(-1),
      m_poolSize(poolSize) {
    qDebug() << "[RealTimeTerminalPool] Initialized with pool size" << poolSize;
}

RealTimeTerminalPool::~RealTimeTerminalPool() {
    closeAllTerminals();
    qDebug() << "[RealTimeTerminalPool] Destroyed";
}

int RealTimeTerminalPool::createTerminal(TerminalSession::ShellType shellType, const QString& workingDir) {
    QMutexLocker lock(&m_terminalsMutex);
    
    if (m_terminals.size() >= (size_t)m_poolSize) {
        qWarning() << "[RealTimeTerminalPool] Pool size limit reached";
        return -1;
    }
    
    int id = m_nextTerminalId++;
    auto session = std::make_unique<TerminalSession>(id, shellType, workingDir, this);
    
    if (session->start()) {
        m_terminals[id] = std::move(session);
        m_activeTerminalId = id;
        
        qDebug() << "[RealTimeTerminalPool] Created terminal" << id;
        emit terminalCreated(id);
        
        return id;
    }
    
    return -1;
}

bool RealTimeTerminalPool::removeTerminal(int terminalId) {
    QMutexLocker lock(&m_terminalsMutex);
    
    auto it = m_terminals.find(terminalId);
    if (it != m_terminals.end()) {
        m_terminals.erase(it);
        
        if (m_activeTerminalId == terminalId) {
            m_activeTerminalId = m_terminals.empty() ? -1 : m_terminals.begin()->first;
        }
        
        qDebug() << "[RealTimeTerminalPool] Removed terminal" << terminalId;
        emit terminalClosed(terminalId);
        
        return true;
    }
    
    return false;
}

int RealTimeTerminalPool::getActiveTerminal() const {
    QMutexLocker lock(&m_terminalsMutex);
    return m_activeTerminalId;
}

void RealTimeTerminalPool::setActiveTerminal(int terminalId) {
    QMutexLocker lock(&m_terminalsMutex);
    
    auto it = m_terminals.find(terminalId);
    if (it != m_terminals.end()) {
        m_activeTerminalId = terminalId;
        qDebug() << "[RealTimeTerminalPool] Set active terminal to" << terminalId;
    }
}

bool RealTimeTerminalPool::executeCommand(int terminalId, const QString& command) {
    QMutexLocker lock(&m_terminalsMutex);
    
    auto it = m_terminals.find(terminalId);
    if (it != m_terminals.end()) {
        it->second->executeCommand(command);
        emit commandStarted(terminalId, command);
        return true;
    }
    
    return false;
}

bool RealTimeTerminalPool::executeCommandSync(int terminalId, const QString& command, QString& output) {
    QMutexLocker lock(&m_terminalsMutex);
    
    auto it = m_terminals.find(terminalId);
    if (it != m_terminals.end()) {
        it->second->executeCommand(command);
        output = it->second->getOutput();
        return true;
    }
    
    return false;
}

QString RealTimeTerminalPool::getTerminalOutput(int terminalId) const {
    QMutexLocker lock(&m_terminalsMutex);
    
    auto it = m_terminals.find(terminalId);
    if (it != m_terminals.end()) {
        return it->second->getOutput();
    }
    
    return QString();
}

QString RealTimeTerminalPool::getTerminalError(int terminalId) const {
    QMutexLocker lock(&m_terminalsMutex);
    
    auto it = m_terminals.find(terminalId);
    if (it != m_terminals.end()) {
        return it->second->getError();
    }
    
    return QString();
}

int RealTimeTerminalPool::getTerminalCount() const {
    QMutexLocker lock(&m_terminalsMutex);
    return m_terminals.size();
}

QJsonObject RealTimeTerminalPool::getPoolStatistics() const {
    QMutexLocker lock(&m_terminalsMutex);
    
    QJsonObject stats;
    stats["poolSize"] = m_poolSize;
    stats["activeTerminals"] = (int)m_terminals.size();
    stats["activeTerminalId"] = m_activeTerminalId;
    
    return stats;
}

void RealTimeTerminalPool::closeAllTerminals() {
    QMutexLocker lock(&m_terminalsMutex);
    
    for (auto& pair : m_terminals) {
        pair.second->stop();
    }
    
    m_terminals.clear();
    m_activeTerminalId = -1;
    
    qDebug() << "[RealTimeTerminalPool] Closed all terminals";
}
