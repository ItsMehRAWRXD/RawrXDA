// debug_adapter.hpp - DAP (Debug Adapter Protocol) Implementation
// Author: RAW RXD IDE Team
// License: MIT

#ifndef DEBUG_ADAPTER_HPP
#define DEBUG_ADAPTER_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <variant>
#include <functional>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>
#include <json.hpp>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #define pid_t int
#else
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <unistd.h>
    #include <signal.h>
    using pid_t = pid_t;
#endif

namespace rawrxd {

using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════════════
// DEBUG TYPES
// ═══════════════════════════════════════════════════════════════════════

enum class ThreadState {
    Running,
    Stopped,
    Exited
};

enum class BreakpointType {
    Line,
    Function,
    Address,
    Data,
    Conditional
};

enum class StepType {
    In,
    Over,
    Out,
    Until
};

enum class VariablePresentationHint {
    Property,
    Method,
    Class,
    Enum,
    Interface,
    Namespace,
    Package,
    String,
    Number,
    Boolean,
    Array,
    Object,
    StringLiteral,
    Constant,
    StructuredType
};

struct Source {
    std::string path;
    std::string name;
    std::string source_reference;
    std::string presentation_hint;
    std::vector<int> line_offsets;
    std::string origin;
    std::vector<std::string> sources;
    std::map<std::string, std::string> checksums;
};

struct SourceBreakpoint {
    int line;
    int end_line;
    std::string condition;
    std::string hit_condition;
    std::vector<std::string> log_message;
};

struct FunctionBreakpoint {
    std::string name;
    std::string condition;
    std::string hit_condition;
};

struct Breakpoint {
    std::string id;
    bool verified;
    std::string message;
    Source source;
    int line;
    int end_line;
    std::vector<SourceBreakpoint> source_breakpoint;
    std::vector<FunctionBreakpoint> function_breakpoint;
    int offset;
    int instruction_pointer_reference;
};

struct Thread {
    int id;
    std::string name;
    ThreadState state;
};

struct StackFrame {
    int id;
    std::string name;
    std::string presentation_hint;
    Source source;
    int line;
    int column;
    int end_line;
    int end_column;
    std::string module_id;
    std::string module_name;
    int stack_depth;
    std::string presentation;
};

struct Scope {
    std::string name;
    std::string presentation_hint;
    int variables_reference;
    int named_variables;
    int indexed_variables;
    int line;
    int column;
    int end_line;
    int end_column;
    bool expensive;
    bool immutable;
};

struct Variable {
    std::string name;
    std::string value;
    std::string type;
    VariablePresentationHint presentation_hint;
    int variables_reference;
    int named_variables;
    int indexed_variables;
    std::string memory_reference;
};

struct StackTrace {
    int total_frames;
    std::vector<StackFrame> frames;
};

struct ThreadList {
    std::vector<Thread> threads;
};

struct LaunchRequest {
    std::string no_debug;
    std::string program;
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    std::string cwd;
    std::string stop_at_entry;
    std::vector<SourceBreakpoint> init_commands;
    std::vector<std::string> pre_launch_commands;
    std::vector<std::string> launch_commands;
    std::vector<std::string> post_launch_commands;
};

struct AttachRequest {
    std::string host;
    int port;
    std::string restart;
    std::vector<std::string> init_commands;
    std::vector<std::string> postAttachCommands;
};

struct SetBreakpointsRequest {
    Source source;
    std::vector<SourceBreakpoint> breakpoints;
    std::string lines;
};

struct EvaluateRequest {
    std::string expression;
    int frame_id;
    std::string context;
    std::string format;
};

struct DebugAdapterConfig {
    std::string adapter_id;
    std::string type;  // cppdbg, cppdbg2, lldb
    std::string backend;  // native, LLDB, SDB
    std::vector<std::string> commands;
    std::map<std::string, std::string> custom;
};

// ═══════════════════════════════════════════════════════════════════════
// DEBUG ADAPTER CLASS
// ═══════════════════════════════════════════════════════════════════════

class DebugAdapter {
public:
    DebugAdapter();
    ~DebugAdapter();

    // Connection
    bool startAdapter(const std::string& adapter_path, const std::string& type);
    void stopAdapter();
    bool isRunning() const { return running_; }
    
    // Launch/Attach
    json launch(const LaunchRequest& request);
    json attach(const AttachRequest& request);
    json disconnect();
    
    // Threads
    json getThreads();
    json getThread(int thread_id);
    
    // Stack Trace
    json getStackTrace(int thread_id, int levels, int start_frame = 0);
    json getScopes(int frame_id);
    json getVariables(int variables_reference, const std::string& filter = "");
    
    // Breakpoints
    json setBreakpoints(const SetBreakpointsRequest& request);
    json setFunctionBreakpoints(const std::vector<FunctionBreakpoint>& breakpoints);
    json dataBreakpointInfo(const std::string& name, int variables_reference);
    json setDataBreakpoint(const std::string& data_id, const std::string& access_type, int bytes, int offset);
    
    // Execution Control
    json continue_(int thread_id);
    json pause(int thread_id);
    json next(int thread_id);
    json stepIn(int thread_id);
    json stepOut(int thread_id);
    json stepUntil(int thread_id, const std::string& target);
    
    // Evaluate
    json evaluate(const EvaluateRequest& request);
    json executeCommand(const std::string& command, const std::string& context = "repl");
    
    // Configuration
    void setConfig(const DebugAdapterConfig& config) { config_ = config; }
    DebugAdapterConfig getConfig() const { return config_; }
    
    // Event Callbacks
    void onStopped(std::function<void(const json&)> callback);
    void onContinued(std::function<void(int)> callback);
    void onExited(std::function<void(int)> callback);
    void onThread(std::function<void(const json&)> callback);
    void onOutput(std::function<void(const std::string&, const std::string&)> callback);
    void onBreakpoint(std::function<void(const json&)> callback);

private:
    DebugAdapterConfig config_;
    std::atomic<bool> running_{false};
    
    pid_t debuggee_pid_ = 0;
    int current_thread_id_ = 0;
    
    std::thread reader_thread_;
    std::mutex send_mutex_;
    
    std::function<void(const json&)> stopped_callback_;
    std::function<void(int)> continued_callback_;
    std::function<void(int)> exited_callback_;
    std::function<void(const json&)> thread_callback_;
    std::function<void(const std::string&, const std::string&)> output_callback_;
    std::function<void(const json&)> breakpoint_callback_;
    
    int seq_ = 1;
    std::map<int, std::promise<json>> pending_requests_;
    std::mutex pending_mutex_;
    
    // Protocol handling
    json sendRequest(const std::string& command, const json& args);
    json sendResponse(const json& request, const json& body);
    void handleEvent(const json& event);
    void readerLoop();
    std::string readMessage();
    void sendMessage(const std::string& msg);
};

// ═══════════════════════════════════════════════════════════════════════
// INLINE IMPLEMENTATIONS
// ═══════════════════════════════════════════════════════════════════════

inline DebugAdapter::DebugAdapter() {
    running_ = false;
    seq_ = 1;
}

inline DebugAdapter::~DebugAdapter() {
    stopAdapter();
}

inline bool DebugAdapter::startAdapter(const std::string& adapter_path, 
                                        const std::string& type) {
    // In production, this would spawn the debug adapter process
    // and establish STDIO communication
    running_ = true;
    reader_thread_ = std::thread([this] { readerLoop(); });
    return true;
}

inline void DebugAdapter::stopAdapter() {
    running_ = false;
    
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
}

inline json DebugAdapter::sendRequest(const std::string& command, const json& args) {
    json request = {
        {"seq", seq_++},
        {"type", "request"},
        {"command", command},
        {"arguments", args}
    };
    
    std::unique_lock<std::mutex> lock(pending_mutex_);
    auto it = pending_requests_.find(request["seq"].get<int>());
    lock.unlock();
    
    sendMessage(request.dump());
    
    if (it != pending_requests_.end()) {
        return it->second.get_future().get();
    }
    
    return json{};
}

inline void DebugAdapter::readerLoop() {
    while (running_) {
        try {
            std::string message = readMessage();
            if (message.empty()) continue;
            
            json msg = json::parse(message);
            std::string type = msg.value("type", "");
            
            if (type == "event") {
                handleEvent(msg);
            } else if (type == "response") {
                std::lock_guard<std::mutex> lock(pending_mutex_);
                int request_seq = msg.value("request_seq", -1);
                auto it = pending_requests_.find(request_seq);
                if (it != pending_requests_.end()) {
                    it->second.set_value(msg);
                    pending_requests_.erase(it);
                }
            }
        } catch (...) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

inline void DebugAdapter::handleEvent(const json& event) {
    std::string event_type = event.value("event", "");
    
    if (event_type == "stopped" && stopped_callback_) {
        stopped_callback_(event.value("body", json{}));
    } else if (event_type == "continued" && continued_callback_) {
        continued_callback_(event.value("body", json{}).value("threadId", 0));
    } else if (event_type == "exited" && exited_callback_) {
        exited_callback_(event.value("body", json{}).value("exitCode", 0));
    } else if (event_type == "thread" && thread_callback_) {
        thread_callback_(event.value("body", json{}));
    } else if (event_type == "output" && output_callback_) {
        auto body = event.value("body", json{});
        output_callback_(body.value("output", ""), body.value("category", "stdout"));
    } else if (event_type == "breakpoint" && breakpoint_callback_) {
        breakpoint_callback_(event.value("body", json{}));
    }
}

inline std::string DebugAdapter::readMessage() {
    // Read Content-Length header
    std::string header;
    std::string body;
    
    // In production, read from adapter process STDIO
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    return "";
}

inline void DebugAdapter::sendMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    // Write to adapter process STDIO
    // Content-Length: <size>\r\n\r\n<content>
}

inline json DebugAdapter::launch(const LaunchRequest& request) {
    json args = {
        {"noDebug", request.no_debug == "true"},
        {"program", request.program},
        {"args", request.args},
        {"env", request.env},
        {"cwd", request.cwd}
    };
    
    return sendRequest("launch", args);
}

inline json DebugAdapter::attach(const AttachRequest& request) {
    json args = {
        {"host", request.host},
        {"port", request.port}
    };
    
    return sendRequest("attach", args);
}

inline json DebugAdapter::disconnect() {
    json args = {
        {"restart", false},
        {"terminateDebuggee", true}
    };
    
    return sendRequest("disconnect", args);
}

inline json DebugAdapter::getThreads() {
    return sendRequest("threads", json::object());
}

inline json DebugAdapter::getStackTrace(int thread_id, int levels, int start_frame) {
    json args = {
        {"threadId", thread_id},
        {"levels", levels},
        {"startFrame", start_frame}
    };
    
    return sendRequest("stackTrace", args);
}

inline json DebugAdapter::getScopes(int frame_id) {
    json args = {{"frameId", frame_id}};
    return sendRequest("scopes", args);
}

inline json DebugAdapter::getVariables(int variables_reference, const std::string& filter) {
    json args = {{"variablesReference", variables_reference}};
    if (!filter.empty()) {
        args["filter"] = filter;
    }
    return sendRequest("variables", args);
}

inline json DebugAdapter::setBreakpoints(const SetBreakpointsRequest& request) {
    json args = {
        {"source", {
            {"path", request.source.path},
            {"name", request.source.name}
        }},
        {"breakpoints", request.breakpoints}
    };
    
    return sendRequest("setBreakpoints", args);
}

inline json DebugAdapter::continue_(int thread_id) {
    json args = {{"threadId", thread_id}};
    return sendRequest("continue", args);
}

inline json DebugAdapter::pause(int thread_id) {
    json args = {{"threadId", thread_id}};
    return sendRequest("pause", args);
}

inline json DebugAdapter::next(int thread_id) {
    json args = {{"threadId", thread_id}};
    return sendRequest("next", args);
}

inline json DebugAdapter::stepIn(int thread_id) {
    json args = {{"threadId", thread_id}};
    return sendRequest("stepIn", args);
}

inline json DebugAdapter::stepOut(int thread_id) {
    json args = {{"threadId", thread_id}};
    return sendRequest("stepOut", args);
}

inline json DebugAdapter::evaluate(const EvaluateRequest& request) {
    json args = {
        {"expression", request.expression},
        {"frameId", request.frame_id},
        {"context", request.context}
    };
    
    return sendRequest("evaluate", args);
}

inline void DebugAdapter::onStopped(std::function<void(const json&)> callback) {
    stopped_callback_ = callback;
}

inline void DebugAdapter::onContinued(std::function<void(int)> callback) {
    continued_callback_ = callback;
}

inline void DebugAdapter::onExited(std::function<void(int)> callback) {
    exited_callback_ = callback;
}

inline void DebugAdapter::onThread(std::function<void(const json&)> callback) {
    thread_callback_ = callback;
}

inline void DebugAdapter::onOutput(std::function<void(const std::string&, const std::string&)> callback) {
    output_callback_ = callback;
}

inline void DebugAdapter::onBreakpoint(std::function<void(const json&)> callback) {
    breakpoint_callback_ = callback;
}

} // namespace rawrxd

#endif // DEBUG_ADAPTER_HPP
