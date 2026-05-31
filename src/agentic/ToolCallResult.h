// ============================================================================
// ToolCallResult.h — Structured Tool Execution Result
// ============================================================================
// Every tool returns a ToolCallResult, not a bool. This enables:
//   - Deterministic LLM feedback (success/output/error split)
//   - Replayable transcripts (full input/output capture)
//   - Audit logging (metadata: timing, tool name, args hash)
//   - Diff history (proposed vs actual changes)
//
// Pattern: PatchResult-style structured results, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agent {

// ============================================================================
// Tool execution outcome
// ============================================================================
enum class ToolOutcome {
    Success,            // Tool executed correctly
    PartialSuccess,     // Tool ran but with warnings
    ValidationFailed,   // Argument or schema validation failed
    SandboxBlocked,     // Sandbox denied execution
    ExecutionError,     // Tool ran but failed
    Timeout,            // Tool exceeded time limit
    Cancelled,          // User or consent callback cancelled
    NotFound,           // Tool name not in registry
    RateLimited         // Too many executions in window
};

// ============================================================================
// ToolCallResult — Structured output for every tool invocation
// ============================================================================
struct ToolCallResult {
    ToolOutcome outcome     = ToolOutcome::ExecutionError;
    std::string output;         // Successful output (file content, search results, etc.)
    std::string error;          // Error message if outcome != Success
    nlohmann::json metadata;    // Timing, byte counts, line counts, etc.

    // Duration tracking (set by executor)
    int64_t durationMs          = 0;

    // Tool identification (set by executor)
    std::string toolName;
    nlohmann::json argsUsed;

    // For file operations — track what changed
    std::string filePath;       // Primary file affected
    size_t bytesRead            = 0;
    size_t bytesWritten         = 0;
    int linesAffected           = 0;

    // ---- Factory methods ----
    static ToolCallResult Ok(const std::string& output) {
        ToolCallResult r;
        r.outcome = ToolOutcome::Success;
        r.output = output;
        return r;
    }

    static ToolCallResult Ok(const std::string& output, const nlohmann::json& meta) {
        ToolCallResult r;
        r.outcome = ToolOutcome::Success;
        r.output = output;
        r.metadata = meta;
        return r;
    }

    static ToolCallResult Error(const std::string& errorMsg, ToolOutcome oc = ToolOutcome::ExecutionError) {
        ToolCallResult r;
        r.outcome = oc;
        r.error = errorMsg;
        return r;
    }

    static ToolCallResult Validation(const std::string& errorMsg) {
        return Error(errorMsg, ToolOutcome::ValidationFailed);
    }

    static ToolCallResult Sandbox(const std::string& errorMsg) {
        return Error(errorMsg, ToolOutcome::SandboxBlocked);
    }

    static ToolCallResult TimedOut(const std::string& partialOutput) {
        ToolCallResult r;
        r.outcome = ToolOutcome::Timeout;
        r.output = partialOutput;
        r.error = "Execution exceeded time limit";
        return r;
    }

    static ToolCallResult NotFound(const std::string& toolName) {
        ToolCallResult r;
        r.outcome = ToolOutcome::NotFound;
        r.error = "Tool not found: " + toolName;
        r.toolName = toolName;
        return r;
    }

    // ---- Predicates ----
    bool isSuccess() const { return outcome == ToolOutcome::Success || outcome == ToolOutcome::PartialSuccess; }
    bool isError() const { return !isSuccess(); }

    // ---- Serialization for LLM feedback ----
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["success"] = isSuccess();
        j["outcome"] = outcomeString();
        if (!output.empty()) j["output"] = output;
        if (!error.empty())  j["error"] = error;
        if (!metadata.empty()) j["metadata"] = metadata;
        if (durationMs > 0) j["duration_ms"] = durationMs;
        if (!filePath.empty()) j["file"] = filePath;
        if (bytesRead > 0) j["bytes_read"] = bytesRead;
        if (bytesWritten > 0) j["bytes_written"] = bytesWritten;
        if (linesAffected > 0) j["lines_affected"] = linesAffected;
        return j;
    }

    // ---- Build message for LLM tool_result injection ----
    nlohmann::json toLLMMessage(const std::string& callId = "") const {
        nlohmann::json msg;
        msg["role"] = "tool";
        if (!callId.empty()) msg["tool_call_id"] = callId;
        msg["content"] = isSuccess() ? output : ("Error: " + error);
        return msg;
    }

    const char* outcomeString() const {
        switch (outcome) {
            case ToolOutcome::Success:          return "success";
            case ToolOutcome::PartialSuccess:   return "partial_success";
            case ToolOutcome::ValidationFailed: return "validation_failed";
            case ToolOutcome::SandboxBlocked:   return "sandbox_blocked";
            case ToolOutcome::ExecutionError:   return "execution_error";
            case ToolOutcome::Timeout:          return "timeout";
            case ToolOutcome::Cancelled:        return "cancelled";
            case ToolOutcome::NotFound:         return "not_found";
            case ToolOutcome::RateLimited:      return "rate_limited";
            default:                            return "unknown";
        }
    }
};

} // namespace Agent
} // namespace RawrXD
