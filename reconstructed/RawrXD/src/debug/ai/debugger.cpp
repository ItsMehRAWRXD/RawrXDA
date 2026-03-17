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
        // In a real implementation, we would parse the full output to extract
        // the file path, line number, and other debug information.
        // For this example, we'll just emit a signal with dummy data.
        QJsonObject debugInfo = collectDebugInfo();
        emit breakpointHit("dummy_file.cpp", 42, debugInfo);
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
    // In a real implementation, this would send commands to GDB to collect
    // locals, stack, registers, etc.
    // For this example, we'll return dummy data.
    QJsonObject debugInfo;
    debugInfo["locals"] = QJsonArray(); // Dummy array
    debugInfo["stack"] = QJsonArray();  // Dummy array
    debugInfo["registers"] = QJsonArray(); // Dummy array
    return debugInfo;
}

void AIDebugger::requestFixFromModel(const QJsonObject &debugInfo)
{
    // In a real implementation, this would send the debugInfo to a model
    // and receive a suggested fix.
    // For this example, we'll just emit a signal with a dummy diff.
    Q_UNUSED(debugInfo)
    QString dummyDiff = "--- a/dummy_file.cpp\n+++ b/dummy_file.cpp\n@@ -39,7 +39,7 @@\n int main() {\n     int a = 5;\n     int b = 10;\n-    int c = a - b;\n+    int c = a + b;\n     return 0;\n }\n";
    emit fixSuggested(dummyDiff);
}