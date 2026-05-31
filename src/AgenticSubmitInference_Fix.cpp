// ============================================================================
// Agentic SubmitInference Fix Implementation
// 
// CRITICAL FIX FOR: BackendError on SubmitInference
// Problem: Tool registry not injected into AIImplementation inference path
// Solution: Registry-aware bridge with structured output enforcement
// Impact: Unblocks tool execution loop and 44-tool ecosystem
// ============================================================================

#include "AgenticSubmitInference_Fix.h"
#include "tool_registry_enhanced.h"
#include <windows.h>
#include <sstream>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <mutex>
#include <vector>

extern "C" {
bool NativeInferenceClient_Initialize(const wchar_t* modelPath);
void NativeInferenceClient_Shutdown(void);
int64_t NativeInferenceClient_Infer(const char* prompt, char* outBuf, size_t outSize);
}

namespace RawrXD {
namespace Agentic {

using json = nlohmann::json;
using JSONGuard = JSON::JSONParseGuard;
using JSONValidator = JSON::JSONSchemaValidator;

namespace {

struct LocalInferenceSessionState {
    std::mutex  mtx;
    bool        ready = false;
    std::string modelPath;
};

LocalInferenceSessionState& LocalSession() {
    static LocalInferenceSessionState s;
    return s;
}

std::string GetEnvOrEmpty(const char* name) {
    if (!name || !*name) {
        return {};
    }
    std::string value;
    value.resize(32767);
    const DWORD n = GetEnvironmentVariableA(name, value.data(), static_cast<DWORD>(value.size()));
    if (n == 0 || n >= value.size()) {
        return {};
    }
    value.resize(static_cast<size_t>(n));
    return value;
}

bool FileExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) {
        return {};
    }
    const int need = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (need <= 0) {
        return {};
    }
    std::wstring out;
    out.resize(static_cast<size_t>(need - 1));
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), need);
    return out;
}

std::string ResolveLocalModelPath(const std::string& requestedModel) {
    // 1) explicit absolute file path in request model name
    if (requestedModel.find(".gguf") != std::string::npos && FileExists(requestedModel)) {
        return requestedModel;
    }

    // 2) operator-configured environment variable
    const std::string envModel = GetEnvOrEmpty("RAWRXD_LOCAL_MODEL_PATH");
    if (FileExists(envModel)) {
        return envModel;
    }

    // 3) conservative local defaults (common in this workspace)
    static const char* kCandidates[] = {
        "d:/phi3mini.gguf",
        "d:/ministral3.gguf",
        "d:/codestral22b.gguf",
        "d:/gptoss20b_link.gguf"
    };
    for (const char* c : kCandidates) {
        if (FileExists(c)) {
            return std::string(c);
        }
    }
    return {};
}

bool EnsureLocalSession(const std::string& requestedModel, std::string& outError) {
    auto& state = LocalSession();
    std::lock_guard<std::mutex> lock(state.mtx);

    const std::string resolvedModelPath = ResolveLocalModelPath(requestedModel);
    if (resolvedModelPath.empty()) {
        outError = "local_inference_unavailable: no GGUF model resolved (set RAWRXD_LOCAL_MODEL_PATH)";
        return false;
    }

    if (state.ready && state.modelPath == resolvedModelPath) {
        return true;
    }

    // Reinitialize to guarantee non-stale mapped session after model switch.
    NativeInferenceClient_Shutdown();
    state.ready = false;
    state.modelPath.clear();

    const std::wstring widePath = Utf8ToWide(resolvedModelPath);
    if (widePath.empty() || !NativeInferenceClient_Initialize(widePath.c_str())) {
        outError = "local_inference_unavailable: NativeInferenceClient_Initialize failed for " + resolvedModelPath;
        return false;
    }

    state.ready = true;
    state.modelPath = resolvedModelPath;
    return true;
}

std::string BuildPromptFromRequest(const json& request) {
    if (request.contains("messages") && request["messages"].is_array()) {
        std::ostringstream oss;
        for (const auto& msg : request["messages"]) {
            if (!msg.is_object()) {
                continue;
            }
            const std::string role = JSONValidator::GetStringField(msg, "role", "user");
            const std::string content = JSONValidator::GetStringField(msg, "content", "");
            if (content.empty()) {
                continue;
            }
            oss << role << ": " << content << "\n";
        }
        return oss.str();
    }
    return JSONValidator::GetStringField(request, "prompt", "");
}

} // namespace

// ============================================================================
// Main Entry Point - SubmitInference with Tool Support
// ============================================================================

AgenticInferenceBridge::InferenceResult AgenticInferenceBridge::SubmitInferenceWithTools(
    const std::string& userMessage,
    const std::string& modelName,
    size_t max_tokens)
{
    InferenceResult result;
    result.toolIterations = 0;
    result.usedTools = false;
    result.success = false;

    // ========== LAZY INITIALIZATION ==========
    // Ensure registry is initialized BEFORE inference
    auto& registry = ToolRegistry::Instance();
    if (!registry.IsInitialized()) {
        if (!registry.Initialize()) {
            result.error = "BackendError: ToolRegistry initialization failed";
            OutputDebugStringA("[AgenticInference] ERROR: Registry init failed\n");
            return result;
        }
    }

    // ========== BUILD TOOL-ENABLED REQUEST ==========
    json request = BuildToolEnabledRequest(userMessage, modelName, registry, max_tokens);
    
    if (request.empty()) {
        result.error = "BackendError: Failed to build request";
        return result;
    }

    // ========== TOOL EXECUTION LOOP ==========
    for (int iteration = 0; iteration < MAX_TOOL_ITERATIONS; iteration++) {
        result.toolIterations = iteration + 1;
        
        // Send to LLM
        std::string rawResponse;
        if (!SendToOllama(request, rawResponse)) {
            result.error = "BackendError: Ollama connection failed";
            OutputDebugStringA("[AgenticInference] ERROR: Ollama request failed\n");
            return result;
        }

        // ========== PARSE RESPONSE WITH HARDENING ==========
        json responseJson = JSONGuard::SafeParse(rawResponse, 
            [](const std::string& err) {
                OutputDebugStringA(("[AgenticInference] Parse error: " + err + "\n").c_str());
            });

        if (responseJson.empty()) {
            result.error = "BackendError: Invalid response JSON";
            OutputDebugStringA("[AgenticInference] ERROR: Response JSON parse failed\n");
            return result;
        }

        // ========== EXTRACT TOOL CALLS ==========
        std::vector<ToolCall> toolCalls = ExtractToolCalls(responseJson);

        if (toolCalls.empty()) {
            // No tools needed - this is the final response
            result.response = JSONValidator::GetStringField(responseJson, "response", "");
            if (result.response.empty() && responseJson.contains("message")) {
                result.response = JSONValidator::GetStringField(responseJson["message"], "content", "");
            }
            result.success = true;
            
            if (iteration > 0) {
                result.usedTools = true;
            }
            
            return result;
        }

        // ========== EXECUTE TOOLS ==========
        if (iteration == 0) {
            result.usedTools = true;
        }

        std::vector<json> toolResults;
        
        for (const auto& toolCall : toolCalls) {
            ToolCallRecord record;
            record.toolName = toolCall.name;
            record.callId = toolCall.id;

            try {
                // Parse arguments with hardening
                std::string argsJson = toolCall.arguments.is_string() 
                    ? toolCall.arguments.get<std::string>()
                    : toolCall.arguments.dump();

                json parsedArgs = JSONGuard::SafeParse(argsJson);
                if (parsedArgs.empty()) {
                    parsedArgs = json::object();
                }

                // Execute tool (with registry dispatch table)
                ToolExecutionResult execResult = registry.ExecuteTool(
                    toolCall.name, 
                    parsedArgs);

                record.success = execResult.success;
                record.output = execResult.output;
                record.error = execResult.error;
                record.durationMs = execResult.durationMs;

                // Build tool result for next inference turn
                toolResults.push_back({
                    {"tool_call_id", toolCall.id},
                    {"role", "tool"},
                    {"content", execResult.success ? execResult.output : execResult.error}
                });

                result.toolTrace.push_back(record);

            } catch (const std::exception& e) {
                record.success = false;
                record.output = "";
                record.error = std::string("Tool execution error: ") + e.what();
                
                toolResults.push_back({
                    {"tool_call_id", toolCall.id},
                    {"role", "tool"},
                    {"content", record.error}
                });

                result.toolTrace.push_back(record);
            }
        }

        // ========== BUILD FOLLOW-UP REQUEST ==========
        request = BuildFollowUpRequest(modelName, toolResults, max_tokens);
        
        if (request.empty()) {
            result.error = "BackendError: Failed to build follow-up request";
            return result;
        }
    }

    // ========== MAX ITERATIONS EXCEEDED ==========
    result.error = "BackendError: Max tool iterations (" + std::to_string(MAX_TOOL_ITERATIONS) + ") exceeded";
    result.success = false;
    return result;
}

// ============================================================================
// Simplified Interface
// ============================================================================

bool AgenticInferenceBridge::SubmitInference(
    const std::string& prompt,
    std::string& outResponse,
    std::string& outError)
{
    InferenceResult result = SubmitInferenceWithTools(prompt, "default", 2048);
    
    if (result.success) {
        outResponse = result.response;
        return true;
    } else {
        outError = result.error;
        return false;
    }
}

// ============================================================================
// Private Implementation
// ============================================================================

json AgenticInferenceBridge::BuildToolEnabledRequest(
    const std::string& userMessage,
    const std::string& modelName,
    const ToolRegistry& registry,
    size_t max_tokens)
{
    try {
        json req = {
            {"model", modelName},
            {"stream", false},
            {"temperature", 0.3},  // Slightly lower for tool calls
            {"top_p", 0.95}
        };

        if (max_tokens > 0) {
            req["num_predict"] = static_cast<int>(max_tokens);
        }

        // ========== CRITICAL: Include tool schemas ==========
        // This is what was missing, causing BackendError
        json toolSchemas = json::array();
        for (const auto& schema : registry.GetToolSchemas()) {
            toolSchemas.push_back({
                {"type", "function"},
                {"function", {
                    {"name", schema.name},
                    {"description", schema.description},
                    {"parameters", schema.parameters}
                }}
            });
        }
        req["tools"] = toolSchemas;

        // ========== BUILD MESSAGE HISTORY ==========
        json messages = json::array();
        
        // System prompt with tool instructions
        messages.push_back({
            {"role", "system"},
            {"content", BuildSystemPrompt(registry.GetToolCount())}
        });
        
        // User message
        messages.push_back({
            {"role", "user"},
            {"content", userMessage}
        });
        
        req["messages"] = messages;

        return req;

    } catch (const std::exception& e) {
        OutputDebugStringA(("[AgenticInference] BuildToolEnabledRequest error: " + std::string(e.what()) + "\n").c_str());
        return json();
    }
}

json AgenticInferenceBridge::BuildFollowUpRequest(
    const std::string& modelName,
    const std::vector<json>& toolResults,
    size_t max_tokens)
{
    try {
        json req = {
            {"model", modelName},
            {"stream", false},
            {"temperature", 0.3},
            {"top_p", 0.95}
        };

        if (max_tokens > 0) {
            req["num_predict"] = static_cast<int>(max_tokens);
        }

        // Build messages with tool results
        json messages = json::array();
        
        // Add tool results as messages
        for (const auto& result : toolResults) {
            messages.push_back(result);
        }
        
        req["messages"] = messages;

        return req;

    } catch (const std::exception& e) {
        OutputDebugStringA(("[AgenticInference] BuildFollowUpRequest error: " + std::string(e.what()) + "\n").c_str());
        return json();
    }
}

std::vector<ToolCall> AgenticInferenceBridge::ExtractToolCalls(const json& response)
{
    std::vector<ToolCall> calls;
    
    try {
        // Check for tool_calls in response
        if (response.contains("tool_calls") && response["tool_calls"].is_array()) {
            for (const auto& tc : response["tool_calls"]) {
                ToolCall call;
                call.id = JSONValidator::GetStringField(tc, "id", "");
                call.name = JSONValidator::GetStringField(tc, "name", "");
                
                if (tc.contains("arguments")) {
                    call.arguments = tc["arguments"];
                }
                
                if (!call.name.empty()) {
                    calls.push_back(std::move(call));
                }
            }
        }
        
        // Check for function_call (OpenAI format)
        if (response.contains("function_call")) {
            const auto& fc = response["function_call"];
            ToolCall call;
            call.id = JSONValidator::GetStringField(fc, "id", "call_" + std::to_string(calls.size()));
            call.name = JSONValidator::GetStringField(fc, "name", "");
            
            if (fc.contains("arguments")) {
                call.arguments = fc["arguments"];
            }
            
            if (!call.name.empty()) {
                calls.push_back(std::move(call));
            }
        }
        
        // Check message content for tool calls
        if (response.contains("message") && response["message"].is_object()) {
            const auto& msg = response["message"];
            
            if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
                for (const auto& tc : msg["tool_calls"]) {
                    ToolCall call;
                    call.id = JSONValidator::GetStringField(tc, "id", "");
                    call.name = JSONValidator::GetStringField(tc["function"], "name", "");
                    
                    if (tc["function"].contains("arguments")) {
                        call.arguments = tc["function"]["arguments"];
                    }
                    
                    if (!call.name.empty()) {
                        calls.push_back(std::move(call));
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        OutputDebugStringA(("[AgenticInference] ExtractToolCalls error: " + std::string(e.what()) + "\n").c_str());
    }
    
    return calls;
}

bool AgenticInferenceBridge::SendToOllama(const json& request, std::string& outResponse)
{
    try {
        const std::string modelName = JSONValidator::GetStringField(request, "model", "default");
        const std::string prompt = BuildPromptFromRequest(request);

        if (prompt.empty()) {
            OutputDebugStringA("[AgenticInference] ERROR: Empty prompt in request\n");
            return false;
        }

        std::string ensureError;
        if (!EnsureLocalSession(modelName, ensureError)) {
            OutputDebugStringA(("[AgenticInference] ERROR: " + ensureError + "\n").c_str());
            return false;
        }

        std::vector<char> outBuf(64 * 1024, '\0');
        const int64_t written = NativeInferenceClient_Infer(prompt.c_str(), outBuf.data(), outBuf.size());
        if (written < 0) {
            OutputDebugStringA("[AgenticInference] ERROR: NativeInferenceClient_Infer failed\n");
            return false;
        }

        json response;
        response["model"] = modelName;
        response["response"] = std::string(outBuf.data());
        response["done"] = true;
        outResponse = response.dump();
        return true;
        
    } catch (const std::exception& e) {
        OutputDebugStringA(("[AgenticInference] SendToOllama error: " + std::string(e.what()) + "\n").c_str());
        return false;
    }
}

std::string AgenticInferenceBridge::BuildSystemPrompt(size_t toolCount)
{
    std::ostringstream oss;
    oss << "You are an AI assistant with access to " << toolCount << " tools.\n";
    oss << "When you need to use a tool, respond with a tool call in the following format:\n";
    oss << "{\n";
    oss << "  \"tool_calls\": [\n";
    oss << "    {\n";
    oss << "      \"name\": \"tool_name\",\n";
    oss << "      \"arguments\": {\"param\": \"value\"}\n";
    oss << "    }\n";
    oss << "  ]\n";
    oss << "}\n";
    oss << "\n";
    oss << "Available tools:\n";
    
    auto& registry = ToolRegistry::Instance();
    for (const auto& name : registry.GetToolNames()) {
        oss << "  - " << name << "\n";
    }
    
    oss << "\nAlways use tools when you need to perform actions like reading files, executing commands, or searching code.\n";
    
    return oss.str();
}

} // namespace Agentic
} // namespace RawrXD
