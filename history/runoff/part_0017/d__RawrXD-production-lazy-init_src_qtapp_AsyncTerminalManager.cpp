#include "AsyncTerminalManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QElapsedTimer>

namespace RawrXD {

// TerminalSession serialization implementation
QByteArray TerminalSession::toJson() const
{
    QJsonObject obj;
    obj["sessionId"] = sessionId;
    obj["shellType"] = shellType;
    obj["workingDirectory"] = workingDirectory;
    obj["created"] = created.toString(Qt::ISODate);
    obj["lastUsed"] = lastUsed.toString(Qt::ISODate);
    obj["pid"] = pid;
    
    QJsonArray historyArray;
    for (const QString& cmd : commandHistory) {
        historyArray.append(cmd);
    }
    obj["commandHistory"] = historyArray;
    
    QJsonObject envObj;
    for (auto it = environment.constBegin(); it != environment.constEnd(); ++it) {
        envObj[it.key()] = it.value();
    }
    obj["environment"] = envObj;
    
    return QJsonDocument(obj).toJson();
}

TerminalSession TerminalSession::fromJson(const QByteArray& json)
{
    TerminalSession session;
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) return session;
    
    QJsonObject obj = doc.object();
    session.sessionId = obj["sessionId"].toString();
    session.shellType = obj["shellType"].toString();
    session.workingDirectory = obj["workingDirectory"].toString();
    session.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
    session.lastUsed = QDateTime::fromString(obj["lastUsed"].toString(), Qt::ISODate);
    session.pid = obj["pid"].toInteger();
    
    QJsonArray historyArray = obj["commandHistory"].toArray();
    for (const QJsonValue& val : historyArray) {
        session.commandHistory.append(val.toString());
    }
    
    QJsonObject envObj = obj["environment"].toObject();
    for (auto it = envObj.constBegin(); it != envObj.constEnd(); ++it) {
        session.environment[it.key()] = it.value().toString();
    }
    
    return session;
}

AsyncTerminalManager::AsyncTerminalManager(QObject* parent)
    : QObject(parent)
    , m_threadPool(QThreadPool::globalInstance())
    , m_taskCounter(0)
{
    // Configure thread pool for terminal operations
    // Reserve some threads for terminal I/O (typically 4-8 is good)
    int maxThreads = QThread::idealThreadCount();
    if (maxThreads < 4) maxThreads = 4;
    m_threadPool->setMaxThreadCount(maxThreads);
    
    qDebug() << "[AsyncTerminalManager] Initialized with thread pool max threads:" << maxThreads;
}

AsyncTerminalManager::~AsyncTerminalManager()
{
    // Clean up all sessions
    QMutexLocker locker(&m_sessionsMutex);
    
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        SessionData* data = it.value();
        if (data->process) {
            data->process->terminate();
            data->process->waitForFinished(2000);
            data->process->kill();
            delete data->process;
        }
        delete data;
    }
    m_sessions.clear();
    
    // Wait for all pending thread pool tasks
    m_threadPool->waitForDone(5000);
}

QString AsyncTerminalManager::createSession(ShellType shellType, const QString& workingDir)
{
    // Generate unique session ID
    QString sessionId = QString("terminal_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QCoreApplication::applicationPid());
    
    SessionData* data = new SessionData();
    data->session.sessionId = sessionId;
    data->session.created = QDateTime::currentDateTime();
    data->session.lastUsed = data->session.created;
    data->session.workingDirectory = workingDir.isEmpty() ? QDir::currentPath() : workingDir;
    
    switch (shellType) {
        case PowerShell:
            data->session.shellType = "PowerShell";
            break;
        case CommandPrompt:
            data->session.shellType = "CMD";
            break;
        case Bash:
            data->session.shellType = "Bash";
            break;
        case Python:
            data->session.shellType = "Python";
            break;
        case NodeJS:
            data->session.shellType = "NodeJS";
            break;
    }
    
    QMutexLocker locker(&m_sessionsMutex);
    m_sessions[sessionId] = data;
    locker.unlock();
    
    qDebug() << "[AsyncTerminalManager] Created session:" << sessionId 
             << "Type:" << data->session.shellType;
    
    emit sessionCreated(sessionId, data->session.shellType);
    
    return sessionId;
}

QString AsyncTerminalManager::executeCommandAsync(const QString& sessionId, const QString& command, bool waitForCompletion)
{
    QString taskId = generateTaskId();
    
    TerminalTask task(sessionId, command, waitForCompletion, 30000);
    m_pendingTasks[taskId] = task;
    
    qDebug() << "[AsyncTerminalManager] Queuing async command:" << command 
             << "Session:" << sessionId << "TaskID:" << taskId;
    
    emit commandStarted(sessionId, taskId, command);
    
    // Execute in thread pool
    QFuture<TerminalResult> future = QtConcurrent::run([this, task, taskId, sessionId]() {
        TerminalResult result = executeInBackground(task);
        
        // Emit completion signal in main thread
        QMetaObject::invokeMethod(this, [this, sessionId, taskId, result]() {
            emit commandCompleted(sessionId, taskId, result);
            m_pendingTasks.remove(taskId);
        }, Qt::QueuedConnection);
        
        return result;
    });
    
    return taskId;
}

TerminalResult AsyncTerminalManager::executeCommandSync(const QString& sessionId, const QString& command, int timeoutMs)
{
    TerminalTask task(sessionId, command, true, timeoutMs);
    
    qDebug() << "[AsyncTerminalManager] Executing sync command:" << command 
             << "Session:" << sessionId;
    
    QString taskId = generateTaskId();
    emit commandStarted(sessionId, taskId, command);
    
    TerminalResult result = executeInBackground(task);
    
    emit commandCompleted(sessionId, taskId, result);
    
    return result;
}

TerminalResult AsyncTerminalManager::executeInBackground(const TerminalTask& task)
{
    QElapsedTimer timer;
    timer.start();
    
    TerminalResult result;
    
    // Get session data
    QMutexLocker sessionLocker(&m_sessionsMutex);
    if (!m_sessions.contains(task.sessionId)) {
        result.success = false;
        result.stderr = "Session not found: " + task.sessionId;
        return result;
    }
    
    SessionData* data = m_sessions[task.sessionId];
    sessionLocker.unlock();
    
    // Lock this specific session
    QMutexLocker dataLocker(&data->mutex);
    
    // Get or create process
    QProcess* process = getOrCreateProcess(task.sessionId);
    if (!process) {
        result.success = false;
        result.stderr = "Failed to create process for session";
        return result;
    }
    
    // Update session state
    data->session.lastUsed = QDateTime::currentDateTime();
    data->session.commandHistory.append(task.command);
    
    // Write command to process
    QString cmdWithNewline = task.command + "\n";
    process->write(cmdWithNewline.toUtf8());
    process->waitForBytesWritten(1000);
    
    if (task.waitForCompletion) {
        // Wait for command to complete (look for prompt or timeout)
        QByteArray stdout, stderr;
        bool timedOut = false;
        
        QElapsedTimer cmdTimer;
        cmdTimer.start();
        
        while (cmdTimer.elapsed() < task.timeoutMs) {
            process->waitForReadyRead(100);
            
            stdout += process->readAllStandardOutput();
            stderr += process->readAllStandardError();
            
            // Simple heuristic: command is done if we see a prompt-like pattern
            // This is shell-specific and could be improved
            if (stdout.contains("PS >") || stdout.contains(">") || stdout.contains("$")) {
                break;
            }
            
            QCoreApplication::processEvents();
        }
        
        result.stdout = QString::fromUtf8(stdout);
        result.stderr = QString::fromUtf8(stderr);
        result.success = process->state() == QProcess::Running;
        result.exitCode = 0;  // Process is still running
    } else {
        // Fire and forget - just return immediately
        result.success = true;
        result.exitCode = 0;
    }
    
    result.executionTimeMs = timer.elapsed();
    
    // Emit output signals
    if (!result.stdout.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, sessionId = task.sessionId, data = result.stdout.toUtf8()]() {
            emit stdoutReady(sessionId, data);
        }, Qt::QueuedConnection);
    }
    
    if (!result.stderr.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, sessionId = task.sessionId, data = result.stderr.toUtf8()]() {
            emit stderrReady(sessionId, data);
        }, Qt::QueuedConnection);
    }
    
    return result;
}

QProcess* AsyncTerminalManager::getOrCreateProcess(const QString& sessionId)
{
    SessionData* data = m_sessions[sessionId];
    
    if (data->process && data->process->state() == QProcess::Running) {
        return data->process;
    }
    
    // Create new process
    if (data->process) {
        delete data->process;
    }
    
    data->process = new QProcess();
    
    // Setup process environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = data->session.environment.constBegin(); 
         it != data->session.environment.constEnd(); ++it) {
        env.insert(it.key(), it.value());
    }
    data->process->setProcessEnvironment(env);
    data->process->setWorkingDirectory(data->session.workingDirectory);
    
    // Get shell command
    ShellType type = PowerShell;
    if (data->session.shellType == "CMD") type = CommandPrompt;
    else if (data->session.shellType == "Bash") type = Bash;
    else if (data->session.shellType == "Python") type = Python;
    else if (data->session.shellType == "NodeJS") type = NodeJS;
    
    QPair<QString, QStringList> shellCmd = getShellCommand(type);
    
    // Start process
    data->process->start(shellCmd.first, shellCmd.second);
    if (!data->process->waitForStarted(3000)) {
        qWarning() << "[AsyncTerminalManager] Failed to start process for session:" << sessionId;
        return nullptr;
    }
    
    data->session.pid = data->process->processId();
    
    qDebug() << "[AsyncTerminalManager] Started process for session:" << sessionId 
             << "PID:" << data->session.pid;
    
    return data->process;
}

QPair<QString, QStringList> AsyncTerminalManager::getShellCommand(ShellType type)
{
    QPair<QString, QStringList> cmd;
    
    switch (type) {
        case PowerShell:
            cmd.first = "pwsh.exe";
            cmd.second << "-NoLogo" << "-NoExit" << "-Command" << "-";
            break;
            
        case CommandPrompt:
            cmd.first = "cmd.exe";
            cmd.second << "/K";
            break;
            
        case Bash:
            cmd.first = "bash";
            cmd.second << "-i";  // Interactive
            break;
            
        case Python:
            cmd.first = "python";
            cmd.second << "-i";  // Interactive
            break;
            
        case NodeJS:
            cmd.first = "node";
            // Node REPL starts by default
            break;
    }
    
    return cmd;
}

void AsyncTerminalManager::closeSession(const QString& sessionId, bool saveState)
{
    QMutexLocker locker(&m_sessionsMutex);
    
    if (!m_sessions.contains(sessionId)) {
        return;
    }
    
    SessionData* data = m_sessions[sessionId];
    
    if (saveState) {
        // Save session state for potential restoration
        // Implementation would save to a configuration directory
        qDebug() << "[AsyncTerminalManager] Saving session state:" << sessionId;
    }
    
    if (data->process) {
        data->process->terminate();
        data->process->waitForFinished(2000);
        if (data->process->state() != QProcess::NotRunning) {
            data->process->kill();
        }
        delete data->process;
    }
    
    delete data;
    m_sessions.remove(sessionId);
    
    locker.unlock();
    
    qDebug() << "[AsyncTerminalManager] Closed session:" << sessionId;
    emit sessionClosed(sessionId);
}

QMap<QString, TerminalSession> AsyncTerminalManager::getActiveSessions() const
{
    QMutexLocker locker(&m_sessionsMutex);
    
    QMap<QString, TerminalSession> sessions;
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        sessions[it.key()] = it.value()->session;
    }
    
    return sessions;
}

bool AsyncTerminalManager::isSessionRunning(const QString& sessionId) const
{
    QMutexLocker locker(&m_sessionsMutex);
    
    if (!m_sessions.contains(sessionId)) {
        return false;
    }
    
    SessionData* data = m_sessions[sessionId];
    return data->process && data->process->state() == QProcess::Running;
}

qint64 AsyncTerminalManager::getSessionPid(const QString& sessionId) const
{
    QMutexLocker locker(&m_sessionsMutex);
    
    if (!m_sessions.contains(sessionId)) {
        return 0;
    }
    
    return m_sessions[sessionId]->session.pid;
}

void AsyncTerminalManager::setMaxConcurrentProcesses(int max)
{
    m_threadPool->setMaxThreadCount(max);
    qDebug() << "[AsyncTerminalManager] Set max concurrent processes to:" << max;
}

QString AsyncTerminalManager::getThreadPoolStats() const
{
    return QString("Active threads: %1, Max threads: %2")
        .arg(m_threadPool->activeThreadCount())
        .arg(m_threadPool->maxThreadCount());
}

QString AsyncTerminalManager::generateTaskId()
{
    return QString("task_%1_%2")
        .arg(++m_taskCounter)
        .arg(QDateTime::currentMSecsSinceEpoch());
}

bool AsyncTerminalManager::saveSessions(const QString& path)
{
    QDir dir(path);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "[AsyncTerminalManager] Failed to create sessions directory:" << path;
        return false;
    }
    
    QMutexLocker locker(&m_sessionsMutex);
    
    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        QString filename = QString("session_%1.json").arg(it.key());
        QString filepath = dir.filePath(filename);
        
        if (!saveSessionToFile(it.value()->session, filepath)) {
            qWarning() << "[AsyncTerminalManager] Failed to save session:" << it.key();
        }
    }
    
    qDebug() << "[AsyncTerminalManager] Saved" << m_sessions.size() << "sessions to:" << path;
    return true;
}

bool AsyncTerminalManager::restoreSessions(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) {
        qDebug() << "[AsyncTerminalManager] Sessions directory does not exist:" << path;
        return false;
    }
    
    QStringList filters;
    filters << "session_*.json";
    QStringList files = dir.entryList(filters, QDir::Files);
    
    int restored = 0;
    for (const QString& filename : files) {
        QString filepath = dir.filePath(filename);
        TerminalSession session = loadSessionFromFile(filepath);
        
        if (!session.sessionId.isEmpty()) {
            SessionData* data = new SessionData();
            data->session = session;
            // Don't auto-start the process - let user decide
            
            QMutexLocker locker(&m_sessionsMutex);
            m_sessions[session.sessionId] = data;
            locker.unlock();
            
            emit sessionCreated(session.sessionId, session.shellType);
            restored++;
        }
    }
    
    qDebug() << "[AsyncTerminalManager] Restored" << restored << "sessions from:" << path;
    return restored > 0;
}

bool AsyncTerminalManager::saveSessionToFile(const TerminalSession& session, const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(session.toJson());
    file.close();
    
    return true;
}

TerminalSession AsyncTerminalManager::loadSessionFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return TerminalSession();
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    return TerminalSession::fromJson(data);
}

} // namespace RawrXD
