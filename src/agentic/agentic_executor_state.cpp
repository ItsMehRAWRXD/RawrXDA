// agentic_executor_state.cpp
// Implementation of state machine types and capture utilities

#include "agentic_executor_state.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <processthreadsapi.h>
#endif

using json = nlohmann::json;

// ========== Helper: Hex conversion ==========
namespace {
    inline std::string to_hex(uint64_t val)
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << val;
        return oss.str();
    }
}

// ========== ExecutionStateSnapshot ==========

std::string ExecutionStateSnapshot::toJson() const
{
    json j;
    j["timestamp_ms"] = timestamp_ms;
    j["iteration_count"] = iteration_count;
    j["retry_count"] = retry_count;
    j["memory_bytes_used"] = memory_bytes_used;
    j["context_window_tokens"] = context_window_tokens;
    j["stack_pointer"] = to_hex(stack_pointer);
    j["instruction_pointer"] = to_hex(instruction_pointer);
    j["process_id"] = process_id;
    j["current_directory"] = current_directory;
    j["modified_files"] = modified_files;
    j["created_processes"] = created_processes;
    j["last_error_message"] = last_error_message;
    j["last_error_code"] = last_error_code;
    j["execution_hash"] = execution_hash;
    return j.dump();
}

ExecutionStateSnapshot ExecutionStateSnapshot::fromJson(const std::string& json_str)
{
    ExecutionStateSnapshot snap;
    try {
        auto j = json::parse(json_str);
        snap.timestamp_ms = j.value("timestamp_ms", 0ULL);
        snap.iteration_count = j.value("iteration_count", 0U);
        snap.retry_count = j.value("retry_count", 0U);
        snap.memory_bytes_used = j.value("memory_bytes_used", 0ULL);
        snap.context_window_tokens = j.value("context_window_tokens", 0ULL);
        snap.process_id = j.value("process_id", 0ULL);
        snap.current_directory = j.value("current_directory", "");
        snap.last_error_message = j.value("last_error_message", "");
        snap.last_error_code = j.value("last_error_code", 0);
        snap.execution_hash = j.value("execution_hash", "");
        
        if (j.contains("modified_files") && j["modified_files"].is_array()) {
            snap.modified_files = j["modified_files"].get<std::vector<std::string>>();
        }
        if (j.contains("created_processes") && j["created_processes"].is_array()) {
            snap.created_processes = j["created_processes"].get<std::vector<std::string>>();
        }
    } catch (...) {
        // Silently fail, return default
    }
    return snap;
}

// ========== AgentStep ==========

std::string AgentStep::toJson() const
{
    json j;
    j["step_id"] = step_id;
    j["action"] = action;
    j["description"] = description;
    json paramsJson = json::object();
    for (const auto& [key, value] : params) {
        paramsJson[key] = value;
    }
    j["params"] = std::move(paramsJson);
    j["timeout_ms"] = timeout_ms;
    j["max_retries"] = max_retries;
    j["success_criteria"] = success_criteria;
    j["created_at_ms"] = created_at_ms;
    j["started_at_ms"] = started_at_ms;
    j["completed_at_ms"] = completed_at_ms;
    j["state"] = static_cast<uint8_t>(state);
    j["success"] = success;
    j["error_message"] = error_message;
    j["execution_attempt"] = execution_attempt;
    j["input_hash"] = input_hash;
    j["output_hash"] = output_hash;
    return j.dump();
}

AgentStep AgentStep::fromJson(const std::string& json_str)
{
    AgentStep step;
    try {
        auto j = json::parse(json_str);
        step.step_id = j.value("step_id", "");
        step.action = j.value("action", "");
        step.description = j.value("description", "");
        
        if (j.contains("params") && j["params"].is_object()) {
            for (auto it = j["params"].begin(); it != j["params"].end(); ++it) {
                if (it.value().is_string()) {
                    step.params[it.key()] = it.value().get<std::string>();
                }
            }
        }
        
        step.timeout_ms = j.value("timeout_ms", 30000);
        step.max_retries = j.value("max_retries", 2);
        step.success_criteria = j.value("success_criteria", "");
        step.created_at_ms = j.value("created_at_ms", 0ULL);
        step.started_at_ms = j.value("started_at_ms", 0ULL);
        step.completed_at_ms = j.value("completed_at_ms", 0ULL);
        
        uint8_t state_val = j.value("state", 0U);
        step.state = static_cast<AgentExecutionState>(state_val);
        
        step.success = j.value("success", false);
        step.error_message = j.value("error_message", "");
        step.execution_attempt = j.value("execution_attempt", 0);
        step.input_hash = j.value("input_hash", "");
        step.output_hash = j.value("output_hash", "");
    } catch (...) {
        // Return default step on parse error
    }
    return step;
}

// ========== AgentExecutionContext ==========

std::string AgentExecutionContext::toJson() const
{
    json j;
    j["state"] = static_cast<uint8_t>(state);
    j["snapshot"] = json::parse(snapshot.toJson());
    
    json steps = json::array();
    for (const auto& step : completed_steps) {
        steps.push_back(json::parse(step.toJson()));
    }
    j["completed_steps"] = steps;
    
    j["workspace_root"] = workspace_root;
    j["goal"] = goal;
    j["max_iterations"] = max_iterations;
    j["current_iteration"] = current_iteration;
    j["last_failure_reason"] = last_failure_reason;
    j["correction_plan"] = correction_plan;
    
    return j.dump();
}

// ========== ExecutionStateCapture ==========

ExecutionStateCapture::ExecutionStateCapture()
{
    m_snapshot.timestamp_ms = 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
}

void ExecutionStateCapture::markStackPointer()
{
#ifdef _WIN32
    // On x64, we can capture rsp via inline asm (requires /O2 or asm)
    // For now, use a simplified approach via exception handling
    __try {
        int dummy;
        m_snapshot.stack_pointer = reinterpret_cast<uint64_t>(&dummy);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        m_snapshot.stack_pointer = 0;
    }
#endif
}

void ExecutionStateCapture::markInstructionPointer()
{
#ifdef _WIN32
    // GetCurrentThreadId gets thread, not instruction pointer directly
    // RIP capture requires platform-specific debug APIs or assembly
    // Store thread ID for now; MASM can inject actual RIP
    HANDLE thread = GetCurrentThread();
    m_snapshot.process_id = GetCurrentThreadId();
#endif
}

void ExecutionStateCapture::recordFileModification(const std::string& path)
{
    m_snapshot.modified_files.push_back(path);
}

void ExecutionStateCapture::recordProcessCreation(uint64_t pid)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << pid;
    m_snapshot.created_processes.push_back(oss.str());
}

void ExecutionStateCapture::recordError(int32_t code, const std::string& message)
{
    m_snapshot.last_error_code = code;
    m_snapshot.last_error_message = message;
}

ExecutionStateSnapshot ExecutionStateCapture::snapshot() const
{
    return m_snapshot;
}
