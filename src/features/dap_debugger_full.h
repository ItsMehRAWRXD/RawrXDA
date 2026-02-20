#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

enum class DebugEventType {
    Breakpoint,
    Step,
    ProcessExit,
    Exception
};

struct DebugEvent {
    DebugEventType type;
    unsigned long long address;
    std::string message;
};

struct StackFrame {
    unsigned long long address;
    std::string function;
    std::string file;
    int line;
};

using DebugCallback = std::function<void(const DebugEvent&)>;

class DAPDebugger {
public:
    DAPDebugger();
    ~DAPDebugger();
    
    // Debugger control
    bool start(const std::string& executable, const std::string& args = "");
    void stop();
    
    // Breakpoint management
    bool setBreakpoint(const std::string& file, int line);
    bool removeBreakpoint(const std::string& file, int line);
    
    // Execution control
    bool continue_();
    bool pause();
    bool step();
    bool stepOver();
    bool stepOut();
    
    // Inspection
    std::vector<StackFrame> getCallStack();
    
    // Events
    void setCallback(DebugCallback callback);
    
    // Status
    bool isDebugging() const;
    
private:
    class Impl;
    Impl* m_impl;
};
