#pragma once
// ============================================================================
// Agentic SubmitInference Fix
// 
// CRITICAL FIX FOR: BackendError on SubmitInference
// Problem: Tool registry not injected into AIImplementation inference path
// Solution: Registry-aware bridge with structured output enforcement
// Impact: Unblocks tool execution loop and 44-tool ecosystem
// ============================================================================

#include "tool_registry_enhanced.h"
#include "json_parse_guard.hpp"
#include <exception>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agentic {

using json = nlohmann::json;

// ============================================================================
// Tool Call Structure
// ============================================================================

struct ToolCall {
    std::string id;
    std::string name;
    json arguments;
};

// ============================================================================
// Tool Call Record (for tracing)
// ============================================================================

struct ToolCallRecord {
    std::string toolName;
    std::string callId;
    bool success = false;
    std::string output;
    std::string error;
    uint64_t durationMs = 0;
};

// ============================================================================
// Agentic Inference Bridge
// ============================================================================

class AgenticInferenceBridge {
public:
    static constexpr int MAX_TOOL_ITERATIONS = 10;
    
    struct InferenceResult {
        bool success = false;
        std::string response;
        std::string error;
        int toolIterations = 0;
        bool usedTools = false;
        std::vector<ToolCallRecord> toolTrace;
    };
    
    // Main entry point - SubmitInference with tool support
    InferenceResult SubmitInferenceWithTools(
        const std::string& userMessage,
        const std::string& modelName,
        size_t max_tokens = 2048);
    
    // Simplified interface for direct inference
    bool SubmitInference(
        const std::string& prompt,
        std::string& outResponse,
        std::string& outError);
    
private:
    // Build tool-enabled request
    json BuildToolEnabledRequest(
        const std::string& userMessage,
        const std::string& modelName,
        const ToolRegistry& registry,
        size_t max_tokens);
    
    // Build follow-up request after tool execution
    json BuildFollowUpRequest(
        const std::string& modelName,
        const std::vector<json>& toolResults,
        size_t max_tokens);
    
    // Extract tool calls from response
    std::vector<ToolCall> ExtractToolCalls(const json& response);
    
    // Send request to Ollama
    bool SendToOllama(const json& request, std::string& outResponse);
    
    // Build system prompt with tool instructions
    std::string BuildSystemPrompt(size_t toolCount);
};

// ============================================================================
// JSON Parse Guard (Hardened Parser)
// ============================================================================

namespace JSON {
    class JSONParseGuard {
    public:
        static json SafeParse(const std::string& text,
                             std::function<void(const std::string&)> errorHandler = nullptr) {
            try {
                return json::parse(text);
            } catch (const std::exception& e) {
                if (errorHandler) {
                    errorHandler(std::string("Parse error: ") + e.what());
                }
                return json();
            }
        }
    };
    
    class JSONSchemaValidator {
    public:
        static std::string GetStringField(const json& obj,
                                          const std::string& field,
                                          const std::string& defaultValue = "") {
            if (!obj.is_object() || !obj.contains(field)) {
                return defaultValue;
            }
            const auto& value = obj[field];
            if (value.is_string()) {
                return value.get<std::string>();
            }
            return defaultValue;
        }
        
        static int GetIntField(const json& obj,
                              const std::string& field,
                              int defaultValue = 0) {
            if (!obj.is_object() || !obj.contains(field)) {
                return defaultValue;
            }
            const auto& value = obj[field];
            if (value.is_number_integer()) {
                return value.get<int>();
            }
            return defaultValue;
        }
        
        static bool GetBoolField(const json& obj,
                                const std::string& field,
                                bool defaultValue = false) {
            if (!obj.is_object() || !obj.contains(field)) {
                return defaultValue;
            }
            const auto& value = obj[field];
            if (value.is_boolean()) {
                return value.get<bool>();
            }
            return defaultValue;
        }
    };
}

} // namespace Agentic
} // namespace RawrXD
