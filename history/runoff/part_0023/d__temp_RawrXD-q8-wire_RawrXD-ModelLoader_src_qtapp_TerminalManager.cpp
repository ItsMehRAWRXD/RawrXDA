#include "TerminalManager.h"
#include <QOperatingSystemVersion>
#include <QUuid>
#include <QDebug>

TerminalManager::TerminalManager(QObject* parent)
    : QObject(parent), 
      m_process(new QProcess(this)), 
      m_shellType(PowerShell),
      m_isElevated(false),
      m_pipeServer(nullptr),
      m_pipeClient(nullptr),
      m_connectionTimeout(new QTimer(this))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TerminalManager::onStdoutReady);
    connect(m_process, &QProcess::readyReadStandardError, this, &TerminalManager::onStderrReady);
    connect(m_process, &QProcess::started, this, &TerminalManager::onProcessStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &TerminalManager::onProcessFinished);
    
    m_connectionTimeout->setSingleShot(true);
    connect(m_connectionTimeout, &QTimer::timeout, this, [this]() {
        emit elevationFailed("Elevated process connection timeout");
    });
    
    m_isElevated = checkElevationStatus();
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

// =====================================================================
// ADMIN ESCALATION IMPLEMENTATION
// =====================================================================

bool TerminalManager::checkElevationStatus()
{
#ifdef Q_OS_WIN
    return IsProcessElevated();
#else
    return false;  // Non-Windows platforms: check geteuid() == 0
#endif
}

bool TerminalManager::isElevated() const
{
    return m_isElevated;
}

bool TerminalManager::startElevated(ShellType shell)
{
#ifndef Q_OS_WIN
    emit elevationFailed("Admin escalation only supported on Windows");
    return false;
#else
    // If already elevated, just start normally
    if (m_isElevated) {
        return start(shell);
    }
    
    m_shellType = shell;
    
    // Setup named pipe for IPC with elevated process
    setupNamedPipe();
    
    // Launch elevated process
    if (!launchElevatedProcess(shell)) {
        emit elevationFailed("UAC elevation was cancelled or failed");
        return false;
    }
    
    // Wait for elevated process to connect (10 second timeout)
    m_connectionTimeout->start(10000);
    
    return true;
#endif
}

void TerminalManager::setupNamedPipe()
{
    // Generate unique pipe name
    m_pipeName = QString("RawrXD_Terminal_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    
    // Create named pipe server
    m_pipeServer = new QLocalServer(this);
    connect(m_pipeServer, &QLocalServer::newConnection, this, &TerminalManager::onElevatedConnection);
    
    if (!m_pipeServer->listen(m_pipeName)) {
        qWarning() << "Failed to create named pipe:" << m_pipeServer->errorString();
        emit elevationFailed("Failed to create IPC pipe");
    }
}

bool TerminalManager::launchElevatedProcess(ShellType shell)
{
#ifndef Q_OS_WIN
    return false;
#else
    QString program;
    QString args;
    
    if (shell == PowerShell) {
        program = "pwsh.exe";
        // PowerShell: connect to named pipe and relay I/O
        args = QString("-NoExit -Command "
                      "$pipe = New-Object System.IO.Pipes.NamedPipeClientStream('.', '%1', 'InOut'); "
                      "$pipe.Connect(5000); "
                      "$reader = New-Object System.IO.StreamReader($pipe); "
                      "$writer = New-Object System.IO.StreamWriter($pipe); "
                      "$writer.AutoFlush = $true; "
                      "while ($true) { "
                      "  $cmd = $reader.ReadLine(); "
                      "  if ($cmd -eq $null) { break; } "
                      "  $output = Invoke-Expression $cmd 2>&1 | Out-String; "
                      "  $writer.WriteLine($output); "
                      "}").arg(m_pipeName);
    } else {
        program = "cmd.exe";
        // CMD: simpler pipe relay
        args = QString("/K echo Connected to pipe: %1").arg(m_pipeName);
    }
    
    // Convert to Windows-compatible strings
    std::wstring programWide = program.toStdWString();
    std::wstring argsWide = args.toStdWString();
    
    // ShellExecuteEx for UAC elevation
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";  // This triggers UAC prompt
    sei.lpFile = programWide.c_str();
    sei.lpParameters = argsWide.c_str();
    sei.nShow = SW_SHOW;
    
    if (!ShellExecuteExW(&sei)) {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            emit elevationFailed("User cancelled UAC prompt");
        } else {
            emit elevationFailed(QString("ShellExecuteEx failed: %1").arg(error));
        }
        return false;
    }
    
    // Process launched successfully (UAC was accepted)
    return true;
#endif
}

void TerminalManager::onElevatedConnection()
{
    m_connectionTimeout->stop();
    
    m_pipeClient = m_pipeServer->nextPendingConnection();
    if (!m_pipeClient) {
        emit elevationFailed("Failed to accept pipe connection");
        return;
    }
    
    connect(m_pipeClient, &QLocalSocket::readyRead, this, &TerminalManager::onElevatedDataReady);
    connect(m_pipeClient, &QLocalSocket::disconnected, this, [this]() {
        emit finished(0, QProcess::NormalExit);
    });
    
    emit started();
}

void TerminalManager::onElevatedDataReady()
{
    if (!m_pipeClient) return;
    
    QByteArray data = m_pipeClient->readAll();
    emit outputReady(data);
}

void TerminalManager::writeInput(const QByteArray& data)
{
    // If using elevated pipe, write to pipe instead of QProcess
    if (m_pipeClient && m_pipeClient->isOpen()) {
        m_pipeClient->write(data);
        m_pipeClient->write("\n");
        m_pipeClient->flush();
    } else if (m_process->state() == QProcess::Running) {
        m_process->write(data);
        m_process->write("\n");
    }
}
