#ifndef AI_DEBUGGER_H
#define AI_DEBUGGER_H

// C++20, no Qt. Breakpoint → collect debug info → prompt → model → fix. Callbacks replace signals.

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

struct AIDebuggerImpl;

class AIDebugger
{
public:
    using BreakpointHitFn   = std::function<void(const std::string& filePath, int lineNumber, const std::string& debugInfoJson)>;
    using FixSuggestedFn   = std::function<void(const std::string& diff)>;
    using DebuggingFinishedFn = std::function<void()>;

    AIDebugger() = default;
    ~AIDebugger();

    void setOnBreakpointHit(BreakpointHitFn f)   { m_onBreakpointHit = std::move(f); }
    void setOnFixSuggested(FixSuggestedFn f)     { m_onFixSuggested = std::move(f); }
    void setOnDebuggingFinished(DebuggingFinishedFn f) { m_onDebuggingFinished = std::move(f); }

    bool startDebugging(const std::string& executablePath, const std::vector<std::string>& arguments = {});
    void setBreakpoint(const std::string& filePath, int lineNumber);
    void continueExecution();
    void stopDebugging();

private:
    void onGdbReadyRead();
    void onGdbFinished(int exitCode, int exitStatus);
    void parseGdbOutput(const std::string& output);
    void sendGdbCommand(const std::string& command);
    std::string collectDebugInfo();
    void requestFixFromModel(const std::string& debugInfoJson);

    std::unique_ptr<AIDebuggerImpl> m_impl;
    std::string m_executablePath;
    bool m_isRunning = false;
    std::map<std::string, int> m_breakpoints;

    BreakpointHitFn   m_onBreakpointHit;
    FixSuggestedFn    m_onFixSuggested;
    DebuggingFinishedFn m_onDebuggingFinished;
};

#endif // AI_DEBUGGER_H
