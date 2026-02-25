#include "ai_debugger.h"
AIDebugger::AIDebugger()
    
    , m_gdbProcess(new void*(this))
    , m_isRunning(false)
{  // Signal connection removed\n  // Signal connection removed\n}

AIDebugger::~AIDebugger()
{
    stopDebugging();
}

bool AIDebugger::startDebugging(const std::string &executablePath, const std::stringList &arguments)
{
    if (m_isRunning) {
        return false;
    }

    m_executablePath = executablePath;

    // Start GDB with MI (Machine Interface) mode
    std::stringList gdbArgs;
    gdbArgs << "--interpreter=mi" << executablePath;
    m_gdbProcess->start("gdb", gdbArgs);
    if (!m_gdbProcess->waitForStarted()) {
        return false;
    }

    m_isRunning = true;

    // Set arguments if provided
    if (!arguments.empty()) {
        std::string setArgsCommand = "-exec-arguments " + arguments.join(" ");
        sendGdbCommand(setArgsCommand);
    }

    return true;
}

void AIDebugger::setBreakpoint(const std::string &filePath, int lineNumber)
{
    if (!m_isRunning) {
        return;
    }

    std::string command = std::string("-break-insert %1:%2");
    sendGdbCommand(command);
    m_breakpoints[filePath] = lineNumber;
}

void AIDebugger::continueExecution()
{
    if (!m_isRunning) {
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

    debuggingFinished();
}

void AIDebugger::onGdbReadyRead()
{
    std::vector<uint8_t> output = m_gdbProcess->readAllStandardOutput();
    std::string outputStr = std::string::fromUtf8(output);
    parseGdbOutput(outputStr);
}

void AIDebugger::onGdbFinished(int exitCode, void*::ExitStatus exitStatus)
{
    (void)(exitCode)
    (void)(exitStatus)

    m_isRunning = false;
    m_breakpoints.clear();
    debuggingFinished();
}

void AIDebugger::parseGdbOutput(const std::string &output)
{
    // This is a simplified parser. A real implementation would need to handle
    // the GDB/MI (Machine Interface) protocol properly.
    // For now, we'll just look for breakpoint hit notifications.
    if (output.contains("*stopped,reason=\"breakpoint-hit\"")) {
        // In a real implementation, we would parse the full output to extract
        // the file path, line number, and other debug information.
        // For this example, we'll just a signal with dummy data.
        void* debugInfo = collectDebugInfo();
        breakpointHit("dummy_file.cpp", 42, debugInfo);
        requestFixFromModel(debugInfo);
    }
}

void AIDebugger::sendGdbCommand(const std::string &command)
{
    if (!m_isRunning) {
        return;
    }

    m_gdbProcess->write((command + "\n").toUtf8());
}

void* AIDebugger::collectDebugInfo()
{
    // In a real implementation, this would send commands to GDB to collect
    // locals, stack, registers, etc.
    // For this example, we'll return dummy data.
    void* debugInfo;
    debugInfo["locals"] = void*(); // Dummy array
    debugInfo["stack"] = void*();  // Dummy array
    debugInfo["registers"] = void*(); // Dummy array
    return debugInfo;
}

void AIDebugger::requestFixFromModel(const void* &debugInfo)
{
    // In a real implementation, this would send the debugInfo to a model
    // and receive a suggested fix.
    // For this example, we'll just a signal with a dummy diff.
    (void)(debugInfo)
    std::string dummyDiff = "--- a/dummy_file.cpp\n+++ b/dummy_file.cpp\n@@ -39,7 +39,7 @@\n int main() {\n     int a = 5;\n     int b = 10;\n-    int c = a - b;\n+    int c = a + b;\n     return 0;\n }\n";
    fixSuggested(dummyDiff);
}

