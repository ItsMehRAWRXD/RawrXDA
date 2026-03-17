// ============================================================================
// AgentTranscript.h — Deterministic Execution Transcript
// ============================================================================
// Records every step of the agent loop for:
//   - Reproducibility (replay exact sessions)
//   - Audit (who/what/when was changed)
//   - Debugging (why did the agent make that decision)
//   - Provenance (what tools were used, in what order)
//
// Each transcript entry captures:
//   - Step number, timestamp
//   - LLM request/response (message + tool_calls)
//   - Tool execution result (ToolCallResult)
//   - Reasoning (model's stated intent)
//
// Pattern: Append-only log. Serializable to JSON.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

#include "ToolCallResult.h"

namespace RawrXD {
namespace Agent {

// ============================================================================
// Single step in the agent transcript
// ============================================================================
struct TranscriptStep {
    int stepNumber              = 0;
    uint64_t timestampMs        = 0;

    // What the model decided
    std::string modelResponse;      // Raw model output
    std::string reasoning;          // Extracted reasoning/thinking
    std::string toolCallName;       // Which tool was called (empty = final answer)
    nlohmann::json toolCallArgs;    // Arguments passed to tool

    // What happened
    ToolCallResult toolResult;      // Structured result from execution
    int64_t modelLatencyMs      = 0;    // Time waiting for model response
    int64_t toolLatencyMs       = 0;    // Time executing tool

    // Context
    int tokensSent              = 0;    // Approximate tokens in request
    int tokensReceived          = 0;    // Approximate tokens in response

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["step"] = stepNumber;
        j["timestamp_ms"] = timestampMs;
        if (!reasoning.empty()) j["reasoning"] = reasoning;
        if (!toolCallName.empty()) {
            nlohmann::json tool_call = nlohmann::json::object();
            tool_call["name"] = toolCallName;
            tool_call["args"] = toolCallArgs;
            j["tool_call"] = std::move(tool_call);
            j["tool_result"] = toolResult.toJson();
            j["tool_latency_ms"] = toolLatencyMs;
        } else {
            j["final_answer"] = modelResponse;
        }
        j["model_latency_ms"] = modelLatencyMs;
        if (tokensSent > 0) j["tokens_sent"] = tokensSent;
        if (tokensReceived > 0) j["tokens_received"] = tokensReceived;
        return j;
    }
};

// ============================================================================
// Complete session transcript
// ============================================================================
class AgentTranscript {
public:
    AgentTranscript() {
        m_sessionId = GenerateSessionId();
        m_startTimeMs = NowMs();
    }

    // ---- Step management ----
    void AddStep(const TranscriptStep& step) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_steps.push_back(step);
    }

    int StepCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<int>(m_steps.size());
    }

    const TranscriptStep* GetStep(int index) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (index < 0 || index >= static_cast<int>(m_steps.size())) return nullptr;
        return &m_steps[index];
    }

    // ---- Reset (non-copyable due to mutex, reset in-place) ----
    void Reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_steps.clear();
        m_filesRead.clear();
        m_filesWritten.clear();
        m_sessionId = GenerateSessionId();
        m_startTimeMs = NowMs();
        m_outcome.clear();
        m_initialPrompt.clear();
        m_modelName.clear();
        m_cwd.clear();
    }

    // ---- Session metadata ----
    void SetInitialPrompt(const std::string& prompt) { m_initialPrompt = prompt; }
    void SetModel(const std::string& model) { m_modelName = model; }
    void SetWorkingDirectory(const std::string& cwd) { m_cwd = cwd; }
    void SetOutcome(const std::string& outcome) { m_outcome = outcome; }
    const std::string& GetSessionId() const { return m_sessionId; }

    // ---- File tracking ----
    void RecordFileRead(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_filesRead.push_back(path);
    }

    void RecordFileWrite(const std::string& path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_filesWritten.push_back(path);
    }

    // ---- Statistics ----
    int64_t TotalDurationMs() const { return NowMs() - m_startTimeMs; }

    int64_t TotalModelLatencyMs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        int64_t total = 0;
        for (const auto& s : m_steps) total += s.modelLatencyMs;
        return total;
    }

    int64_t TotalToolLatencyMs() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        int64_t total = 0;
        for (const auto& s : m_steps) total += s.toolLatencyMs;
        return total;
    }

    int ToolCallCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        int count = 0;
        for (const auto& s : m_steps) {
            if (!s.toolCallName.empty()) ++count;
        }
        return count;
    }

    int ErrorCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        int count = 0;
        for (const auto& s : m_steps) {
            if (!s.toolCallName.empty() && s.toolResult.isError()) ++count;
        }
        return count;
    }

    // ---- Serialization ----
    nlohmann::json toJson() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        nlohmann::json j;
        j["session_id"] = m_sessionId;
        j["model"] = m_modelName;
        j["cwd"] = m_cwd;
        j["initial_prompt"] = m_initialPrompt;
        j["outcome"] = m_outcome;
        j["start_time_ms"] = m_startTimeMs;
        j["total_duration_ms"] = NowMs() - m_startTimeMs;
        j["total_steps"] = m_steps.size();
        j["tool_calls"] = ToolCallCount();
        j["errors"] = ErrorCount();

        nlohmann::json stepsArr = nlohmann::json::array();
        for (const auto& s : m_steps) stepsArr.push_back(s.toJson());
        j["steps"] = stepsArr;

        j["files_read"] = m_filesRead;
        j["files_written"] = m_filesWritten;
        return j;
    }

    // ---- Persistence ----
    bool SaveToFile(const std::string& path) const {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << toJson().dump(2);
        file.close();
        return true;
    }

private:
    static uint64_t NowMs() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    static std::string GenerateSessionId() {
        auto now = std::chrono::system_clock::now();
        auto epoch = now.time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
        char buf[64];
        snprintf(buf, sizeof(buf), "agent-%lld", static_cast<long long>(ms));
        return buf;
    }

    mutable std::mutex m_mutex;
    std::string m_sessionId;
    std::string m_modelName;
    std::string m_cwd;
    std::string m_initialPrompt;
    std::string m_outcome;
    uint64_t m_startTimeMs = 0;

    std::vector<TranscriptStep> m_steps;
    std::vector<std::string> m_filesRead;
    std::vector<std::string> m_filesWritten;
};

} // namespace Agent
} // namespace RawrXD
