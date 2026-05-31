#pragma once
/**
 * @file debugger_client.h
 * @brief Native Win32 debugger interface for process debugging
 * Batch 3 - Item 34: Debugger interface
 */

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <windows.h>

namespace RawrXD::Debugger {

enum class DebugEventType {
    ProcessCreated,
    ThreadCreated,
    ModuleLoaded,
    ModuleUnloaded,
    Exception,
    Breakpoint,
    StepComplete,
    OutputDebugString,
    ProcessExited
};

struct Breakpoint {
    uintptr_t address;
    uint8_t originalByte;
    bool enabled;
    std::string condition;
};

struct StackFrame {
    uintptr_t instructionPointer;
    uintptr_t framePointer;
    uintptr_t stackPointer;
    std::string functionName;
    std::string filePath;
    uint32_t lineNumber;
};

struct Variable {
    std::string name;
    std::string type;
    std::string value;
    bool expandable;
};

struct DebugEvent {
    DebugEventType type;
    uint32_t processId;
    uint32_t threadId;
    uintptr_t address;
    std::string message;
    uint32_t exitCode;
};

class DebuggerClient {
public:
    DebuggerClient();
    ~DebuggerClient();

    // Lifecycle
    bool launch(const std::string& executable, const std::string& args = "", const std::string& cwd = "");
    bool attach(uint32_t processId);
    void detach();
    void terminate();
    bool isAttached() const;

    // Execution control
    bool continueExecution();
    bool stepInto();
    bool stepOver();
    bool stepOut();
    bool pause();

    // Breakpoints
    bool setBreakpoint(uintptr_t address, const std::string& condition = "");
    bool setBreakpoint(const std::string& filePath, uint32_t lineNumber);
    bool removeBreakpoint(uintptr_t address);
    bool enableBreakpoint(uintptr_t address, bool enabled);
    std::vector<Breakpoint> getBreakpoints() const;

    // Stack inspection
    std::vector<StackFrame> getCallStack() const;
    std::vector<Variable> getLocalVariables() const;
    std::vector<Variable> getGlobalVariables(const std::string& moduleName) const;

    // Memory operations
    bool readMemory(uintptr_t address, void* buffer, size_t size);
    bool writeMemory(uintptr_t address, const void* buffer, size_t size);
    std::string evaluateExpression(const std::string& expression);

    // Event handling
    using EventCallback = std::function<void(const DebugEvent&)>;
    void setEventCallback(EventCallback callback);

    // Modules
    struct ModuleInfo {
        std::string name;
        std::string path;
        uintptr_t baseAddress;
        size_t size;
    };
    std::vector<ModuleInfo> getLoadedModules() const;

private:
    HANDLE m_process;
    HANDLE m_thread;
    uint32_t m_pid;
    std::thread m_debugThread;
    std::atomic<bool> m_attached{false};
    EventCallback m_eventCallback;
    std::map<uintptr_t, Breakpoint> m_breakpoints;
    mutable std::mutex m_mutex;

    void handleDebugEvent(const DEBUG_EVENT& evt);
    bool setBreakpointInternal(uintptr_t address);
    bool removeBreakpointInternal(uintptr_t address);
};

// Global instance
DebuggerClient& getDebuggerClient();

} // namespace RawrXD::Debugger
