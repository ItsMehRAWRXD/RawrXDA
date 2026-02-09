#include "ai_debugger.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QTemporaryFile>
#include <QDebug>

AIDebugger::AIDebugger(QObject *parent)
    : QObject(parent)
    , m_gdbProcess(new QProcess(this))
    , m_isRunning(false)
{
    connect(m_gdbProcess, &QProcess::readyReadStandardOutput, this, &AIDebugger::onGdbReadyRead);
    connect(m_gdbProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &AIDebugger::onGdbFinished);
}

AIDebugger::~AIDebugger()
{
    stopDebugging();
}

bool AIDebugger::startDebugging(const QString &executablePath, const QStringList &arguments)
{
    if (m_isRunning) {
        qWarning() << "Debugger is already running";
        return false;
    }

    m_executablePath = executablePath;

    // Start GDB with MI (Machine Interface) mode
    QStringList gdbArgs;
    gdbArgs << "--interpreter=mi" << executablePath;
    m_gdbProcess->start("gdb", gdbArgs);
    if (!m_gdbProcess->waitForStarted()) {
        qWarning() << "Failed to start GDB:" << m_gdbProcess->errorString();
        return false;
    }

    m_isRunning = true;
    qDebug() << "GDB started successfully";

    // Set arguments if provided
    if (!arguments.isEmpty()) {
        QString setArgsCommand = "-exec-arguments " + arguments.join(" ");
        sendGdbCommand(setArgsCommand);
    }

    return true;
}

void AIDebugger::setBreakpoint(const QString &filePath, int lineNumber)
{
    if (!m_isRunning) {
        qWarning() << "Debugger is not running";
        return;
    }

    QString command = QString("-break-insert %1:%2").arg(filePath).arg(lineNumber);
    sendGdbCommand(command);
    m_breakpoints[filePath] = lineNumber;
}

void AIDebugger::continueExecution()
{
    if (!m_isRunning) {
        qWarning() << "Debugger is not running";
        return;
    }

    sendGdbCommand("-exec-continue");
}

void AIDebugger::stopDebugging()
{
    if (!m_isRunning) {
        return;
    }

    m_isRunning = false;
    m_breakpoints.clear();

    // Send quit command to GDB
    sendGdbCommand("-gdb-exit");

    // Wait for GDB to finish
    if (!m_gdbProcess->waitForFinished(5000)) {
        m_gdbProcess->kill();
        m_gdbProcess->waitForFinished(1000);
    }

    qDebug() << "GDB stopped";
    emit debuggingFinished();
}

void AIDebugger::onGdbReadyRead()
{
    QByteArray output = m_gdbProcess->readAllStandardOutput();
    QString outputStr = QString::fromUtf8(output);
    parseGdbOutput(outputStr);
}

void AIDebugger::onGdbFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(exitStatus)

    m_isRunning = false;
    m_breakpoints.clear();
    qDebug() << "GDB process finished";
    emit debuggingFinished();
}

void AIDebugger::parseGdbOutput(const QString &output)
{
    // This is a simplified parser. A real implementation would need to handle
    // the GDB/MI (Machine Interface) protocol properly.
    // For now, we'll just look for breakpoint hit notifications.
    if (output.contains("*stopped,reason=\"breakpoint-hit\"")) {
        qDebug() << "Breakpoint hit detected";
        
        // Parse GDB/MI output for file and line information
        // Format: *stopped,reason="breakpoint-hit",frame={addr="0x...",func="...",args=[...],file="...",line="..."}
        QString filePath = "unknown";
        int lineNumber = 0;
        
        QRegularExpression fileRe(R"(file="([^"]+)")");
        QRegularExpression lineRe(R"(,line="(\d+)")");
        
        auto fileMatch = fileRe.match(output);
        if (fileMatch.hasMatch()) {
            filePath = fileMatch.captured(1);
        }
        auto lineMatch = lineRe.match(output);
        if (lineMatch.hasMatch()) {
            lineNumber = lineMatch.captured(1).toInt();
        }
        
        QJsonObject debugInfo = collectDebugInfo();
        emit breakpointHit(filePath, lineNumber, debugInfo);
        requestFixFromModel(debugInfo);
    }
}

void AIDebugger::sendGdbCommand(const QString &command)
{
    if (!m_isRunning) {
        return;
    }

    qDebug() << "Sending GDB command:" << command;
    m_gdbProcess->write((command + "\n").toUtf8());
}

QJsonObject AIDebugger::collectDebugInfo()
{
    // Query GDB for locals, stack, and registers via MI commands
    QJsonObject debugInfo;
    
    if (m_isRunning && m_gdbProcess && m_gdbProcess->state() == QProcess::Running) {
        // Request local variables: -stack-list-locals --all-values
        m_gdbProcess->write("-stack-list-locals --all-values\n");
        m_gdbProcess->waitForReadyRead(2000);
        QString localsOutput = QString::fromUtf8(m_gdbProcess->readAllStandardOutput());
        
        // Parse locals from GDB/MI response
        QJsonArray locals;
        QRegularExpression varRe(R"(\{name="([^"]+)",value="([^"]*)"})");
        auto it = varRe.globalMatch(localsOutput);
        while (it.hasNext()) {
            auto match = it.next();
            QJsonObject var;
            var["name"] = match.captured(1);
            var["value"] = match.captured(2);
            locals.append(var);
        }
        debugInfo["locals"] = locals;
        
        // Request stack trace: -stack-list-frames
        m_gdbProcess->write("-stack-list-frames\n");
        m_gdbProcess->waitForReadyRead(2000);
        QString stackOutput = QString::fromUtf8(m_gdbProcess->readAllStandardOutput());
        
        QJsonArray stack;
        QRegularExpression frameRe(R"(frame=\{level="(\d+)",addr="([^"]*)",func="([^"]*)"(?:,file="([^"]*)")?(?:,line="(\d+)")?)");
        auto frameIt = frameRe.globalMatch(stackOutput);
        while (frameIt.hasNext()) {
            auto match = frameIt.next();
            QJsonObject frame;
            frame["level"] = match.captured(1).toInt();
            frame["address"] = match.captured(2);
            frame["function"] = match.captured(3);
            if (!match.captured(4).isEmpty()) frame["file"] = match.captured(4);
            if (!match.captured(5).isEmpty()) frame["line"] = match.captured(5).toInt();
            stack.append(frame);
        }
        debugInfo["stack"] = stack;
        
        // Request registers: -data-list-register-values x
        m_gdbProcess->write("-data-list-register-values x\n");
        m_gdbProcess->waitForReadyRead(2000);
        QString regsOutput = QString::fromUtf8(m_gdbProcess->readAllStandardOutput());
        
        QJsonArray registers;
        QRegularExpression regRe(R"(\{number="(\d+)",value="([^"]*)"})");
        auto regIt = regRe.globalMatch(regsOutput);
        while (regIt.hasNext()) {
            auto match = regIt.next();
            QJsonObject reg;
            reg["number"] = match.captured(1).toInt();
            reg["value"] = match.captured(2);
            registers.append(reg);
        }
        debugInfo["registers"] = registers;
    } else {
        // GDB not running — return empty arrays
        debugInfo["locals"] = QJsonArray();
        debugInfo["stack"] = QJsonArray();
        debugInfo["registers"] = QJsonArray();
    }
    
    return debugInfo;
}

void AIDebugger::requestFixFromModel(const QJsonObject &debugInfo)
{
    // Compose a prompt from the debug context for the AI model
    QString prompt = "Debug context:\n";
    
    // Include stack trace
    QJsonArray stack = debugInfo["stack"].toArray();
    if (!stack.isEmpty()) {
        prompt += "Stack trace:\n";
        for (const auto& frame : stack) {
            QJsonObject f = frame.toObject();
            prompt += QString("  #%1 %2 at %3:%4\n")
                .arg(f["level"].toInt())
                .arg(f["function"].toString())
                .arg(f["file"].toString())
                .arg(f["line"].toInt());
        }
    }
    
    // Include local variables
    QJsonArray locals = debugInfo["locals"].toArray();
    if (!locals.isEmpty()) {
        prompt += "Local variables:\n";
        for (const auto& local : locals) {
            QJsonObject var = local.toObject();
            prompt += QString("  %1 = %2\n").arg(var["name"].toString(), var["value"].toString());
        }
    }
    
    prompt += "\nSuggest a fix as a unified diff.";
    
    // Emit the debug context as a signal for the AI fix pipeline
    // The connected model service will generate the fix suggestion
    emit fixSuggested(prompt);
}