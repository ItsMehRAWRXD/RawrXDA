#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "AgenticSubmitInference_Fix.h"

#include "NativeInferenceClient.h"
#include "AgentToolHandlers.h"
#include "ToolCallResult.h"
#include "../ai/inference_retry_shim.h"
#include "../engine/global_runtime_orchestrator.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <memory>
#include <thread>
#include <chrono>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace RawrXD {
namespace Agentic {

using json = nlohmann::json;

namespace {

// Process-wide retry shim. Per-endpoint circuit-breaker state is keyed by
// host:port:model so a stuck remote model trips its own circuit without
// affecting other endpoints.
rxd::ai::InferenceRetryShim& GlobalRetryShim() {
    static rxd::ai::InferenceRetryShim shim(rxd::ai::RetryPolicy{
        /*max_retries=*/4,
        /*base_ms=*/40,
        /*max_backoff_ms=*/2000,
        /*circuit_threshold=*/4,
        /*circuit_reset_ms=*/15000,
        /*jitter_frac=*/0.25
    });
    return shim;
}

// Map a NativeInferenceClient failure message to the shim's status code.
// Fatal classes (auth, schema, bad request) are returned NonRetryable so we
// fail fast instead of burning budget on retries that will never succeed.
rxd::ai::InferenceStatus ClassifyFailure(const std::string& errLower) {
    static const char* kFatal[] = {
        "unauthorized", "forbidden", "401", "403",
        "invalid request", "bad request", "400",
        "not found", "404",
        "unsupported model", "validation failed",
        "tool_registry_init_failed"
    };
    for (const char* needle : kFatal) {
        if (errLower.find(needle) != std::string::npos) {
            return rxd::ai::InferenceStatus::NonRetryable;
        }
    }
    return rxd::ai::InferenceStatus::Retryable;
}

std::string LowerCopy(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

std::string EndpointTag(const Agent::NativeInferenceConfig& cfg) {
    return cfg.host + ":" + std::to_string(cfg.port) + "|" + cfg.chat_model;
}

// Submit one ChatSync attempt against a session that may need recreation.
// On a retryable failure we drop the current session pointer so the next
// attempt re-creates it from scratch — this is the "backend handle survives
// model reloads" guarantee.
rxd::ai::InferenceStatus SubmitOneAttempt(
    std::unique_ptr<Agent::NativeInferenceClient>& session,
    const Agent::NativeInferenceConfig&            clientConfig,
    const std::vector<Agent::ChatMessage>&         messages,
    const json&                                    tools,
    Agent::InferenceResult&                        out_result)
{
    if (!session) {
        try {
            session.reset(new Agent::NativeInferenceClient(clientConfig));
        } catch (const std::exception& e) {
            out_result = Agent::InferenceResult::error(
                std::string("session_create_failed: ") + e.what());
            return ClassifyFailure(LowerCopy(out_result.error_message));
        } catch (...) {
            out_result = Agent::InferenceResult::error(
                "session_create_failed: unknown exception");
            return rxd::ai::InferenceStatus::Retryable;
        }
    }

    try {
        out_result = session->ChatSync(messages, tools);
    } catch (const std::exception& e) {
        out_result = Agent::InferenceResult::error(
            std::string("chat_sync_exception: ") + e.what());
        session.reset(); // stale handle suspected; force recreate next attempt
        return rxd::ai::InferenceStatus::Retryable;
    } catch (...) {
        out_result = Agent::InferenceResult::error(
            "chat_sync_exception: unknown backend error");
        session.reset();
        return rxd::ai::InferenceStatus::Retryable;
    }

    if (out_result.success) return rxd::ai::InferenceStatus::OK;

    const std::string elow = LowerCopy(out_result.error_message);
    const auto status = ClassifyFailure(elow);

    // Phase 4: Handle "blocked" as a retryable event if it's transient
    if (elow.find("blocked") != std::string::npos || elow.find("throttled") != std::string::npos) {
        return rxd::ai::InferenceStatus::Retryable;
    }

    if (status == rxd::ai::InferenceStatus::Retryable) {
        // Drop session so the next attempt rebuilds it; protects against
        // half-open WinHTTP handles after the backend cycled.
        session.reset();
    }
    return status;
}

/**
 * Pre-flight check: Ensure tool registry is fully initialized before attempting ChatSync.
 * Returns true if registry is ready, false if unrecoverable error.
 */
bool EnsureToolRegistryReady(int maxRetries = 3) {
    // Attempt to access the registry instance. If it fails, retry.
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        try {
            auto& registry = Agent::AgentToolHandlers::Instance();
            
            // Probe: Try to get any schema to verify initialization
            const auto schemas = registry.GetAllSchemas();
            if (!schemas.empty()) {
                return true;  // Registry is ready
            }
            
            // Registry exists but is empty; force re-initialization
            if (attempt < maxRetries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (attempt + 1)));
                continue;
            }
        } catch (const std::exception& e) {
            if (attempt < maxRetries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (attempt + 1)));
                continue;
            }
            return false;  // Unrecoverable error
        }
    }
    return false;
}

std::string TrimAscii(std::string value) {
    const auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

bool ValidateQualityGate(const AgenticInferenceBridge::InferenceResult& result,
                        const std::string& latestResponse,
                        std::string& reason) {
    const std::string trimmed = TrimAscii(latestResponse);
    if (trimmed.empty()) {
        reason = "empty_response";
        return false;
    }

    if (!result.usedTools) {
        return true;
    }

    int successCount = 0;
    int failureCount = 0;
    for (const auto& record : result.toolTrace) {
        if (record.success) {
            ++successCount;
        } else {
            ++failureCount;
        }
    }

    if (successCount == 0 && failureCount > 0) {
        reason = "all_tool_calls_failed";
        return false;
    }

    return true;
}

Agent::ChatMessage BuildQualityRecoveryMessage(const std::string& reason) {
    Agent::ChatMessage qualityMessage;
    qualityMessage.role = "user";
    qualityMessage.content =
        "QUALITY_VALIDATION_FAILED: " + reason +
        ". Continue autonomously: inspect the last tool outputs, pick valid registered tools only, and"
        " produce either (a) corrected tool calls or (b) a final answer that explicitly reports unresolved failures.";
    qualityMessage.tool_call_id.clear();
    qualityMessage.tool_calls = json();
    return qualityMessage;
}

std::string BuildToolMessageContent(const RawrXD::Agent::ToolCallResult& result) {
    if (result.isSuccess()) {
        return result.output.empty() ? "Tool completed successfully." : result.output;
    }

    if (!result.error.empty()) {
        return "Error: " + result.error;
    }

    if (!result.output.empty()) {
        return "Error: " + result.output;
    }

    return "Error: Tool execution failed.";
}

int ClampMaxTokens(size_t maxTokens) {
    if (maxTokens == 0) {
        return 4096;
    }

    if (maxTokens > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return std::numeric_limits<int>::max();
    }

    return static_cast<int>(maxTokens);
}

} // namespace

AgenticInferenceBridge::InferenceResult AgenticInferenceBridge::SubmitInferenceWithTools(
    const std::string& userMessage,
    const std::string& modelName,
    size_t max_tokens,
    const RuntimeConfig& runtime)
{
    InferenceResult bridgeResult;

    // PRE-FLIGHT: Ensure tool registry is ready before attempting inference loop
    if (!EnsureToolRegistryReady(3)) {
        bridgeResult.success = false;
        bridgeResult.error = "tool_registry_init_failed: Unable to initialize agent tool handlers after 3 retries";
        bridgeResult.usedTools = false;
        bridgeResult.toolIterations = 0;
        return bridgeResult;
    }

    Agent::NativeInferenceConfig clientConfig;
    clientConfig.host = runtime.host.empty() ? "127.0.0.1" : runtime.host;
    clientConfig.port = runtime.port == 0 ? 11434 : runtime.port;  // Default Ollama port
    clientConfig.chat_model = modelName.empty() ? "headless-default" : modelName;
    clientConfig.temperature = runtime.temperature;
    clientConfig.max_tokens = ClampMaxTokens(max_tokens);

    // Session is owned by this call. Each retry attempt may swap it out via
    // SubmitOneAttempt() if the previous handle went stale (model reload,
    // backend cycled, half-open WinHTTP, etc.).
    std::unique_ptr<Agent::NativeInferenceClient> client;
    const std::string circuitTag = EndpointTag(clientConfig);

    std::vector<Agent::ChatMessage> messages;
    messages.push_back({"system",
                        Agent::AgentToolHandlers::GetSystemPrompt(
                            runtime.workingDirectory.empty() ? "." : runtime.workingDirectory,
                            {}),
                        "",
                        json()});
    messages.push_back({"user", userMessage, "", json()});

    const json tools = Agent::AgentToolHandlers::GetAllSchemas();
    const int maxIterations = std::max(1, runtime.maxToolIterations);
    std::string latestResponse;

    for (int step = 0; step < maxIterations; ++step) {
        bridgeResult.toolIterations = step + 1;

        // Phase 5: Adaptive Recovery & Safety Gating
        // If risk is too high, we block or enter a poll-recovery loop
        while (GlobalRuntimeOrchestrator::Get().AssessRisk() > 0.85f) {
            // Check context size before stalling; if we're near limit, aborting is safer
            // than stalling a high-context session.
            // (Placeholder for future: CheckContextPressure() > 0.90)

            // Log throttling for observability
            // GlobalLogger().Warn("Agentic loop throttled at step %d due to risk > 0.85", step);

            // Poll orchestrator every 500ms for recovery
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // If we've pulsed back to a safe zone (< 0.40), resume immediately
            if (GlobalRuntimeOrchestrator::Get().AssessRisk() < 0.40f) {
                break; 
            }
        }

        // Phase 4: Risk-Gated Tool Expansion Gate
        // Double-check just in case we broke from a timeout/limit
        if (GlobalRuntimeOrchestrator::Get().AssessRisk() > 0.85f) {
            bridgeResult.success = false;
            bridgeResult.error = "system_throttled: High resource pressure (risk > 0.85). Aborted tool loop step " + std::to_string(step);
            return bridgeResult;
        }

        Agent::InferenceResult llmResult;
        const auto shimStatus = GlobalRetryShim().Execute(
            [&]() { return SubmitOneAttempt(client, clientConfig, messages, tools, llmResult); },
            circuitTag);

        if (shimStatus != rxd::ai::InferenceStatus::OK) {
            const char* statusTag =
                (shimStatus == rxd::ai::InferenceStatus::CircuitOpen)  ? "circuit_open"
              : (shimStatus == rxd::ai::InferenceStatus::NonRetryable) ? "non_retryable"
                                                                       :  "retries_exhausted";
            std::string detail = llmResult.error_message.empty()
                                     ? std::string("agentic inference failed")
                                     : llmResult.error_message;

            // Map blocked status to a clear user message
            if (shimStatus == rxd::ai::InferenceStatus::Retryable && detail.find("blocked") != std::string::npos) {
                bridgeResult.error = "system_throttled: " + detail;
            } else {
                bridgeResult.error = std::string(statusTag) + ": " + detail;
            }
            return bridgeResult;
        }

        if (!llmResult.response.empty()) {
            latestResponse = llmResult.response;
            bridgeResult.response = llmResult.response;
        }

        if (!llmResult.has_tool_calls || llmResult.tool_calls.empty()) {
            std::string qualityReason;
            if (!ValidateQualityGate(bridgeResult, latestResponse, qualityReason)) {
                if (step + 1 < maxIterations) {
                    messages.push_back(BuildQualityRecoveryMessage(qualityReason));
                    continue;
                }

                bridgeResult.error = "quality validation failed: " + qualityReason;
                return bridgeResult;
            }

            bridgeResult.success = true;
            if (bridgeResult.response.empty()) {
                bridgeResult.response = latestResponse;
            }
            return bridgeResult;
        }

        bridgeResult.usedTools = true;

        Agent::ChatMessage assistantMessage;
        assistantMessage.role = "assistant";
        assistantMessage.content = llmResult.response;
        assistantMessage.tool_calls = json::array();

        for (size_t i = 0; i < llmResult.tool_calls.size(); ++i) {
            const auto& toolCall = llmResult.tool_calls[i];
            const std::string callId = "call_" + std::to_string(step) + "_" + std::to_string(i);

            json toolCallJson;
            toolCallJson["id"] = callId;
            toolCallJson["type"] = "function";
            toolCallJson["function"] = {{"name", toolCall.first}, {"arguments", toolCall.second}};
            assistantMessage.tool_calls.push_back(toolCallJson);
        }

        messages.push_back(std::move(assistantMessage));

        for (size_t i = 0; i < llmResult.tool_calls.size(); ++i) {
            const auto& toolCall = llmResult.tool_calls[i];
            const std::string callId = "call_" + std::to_string(step) + "_" + std::to_string(i);

            // Attempt tool execution with graceful error handling
            Agent::ToolCallResult toolResult;
            try {
                toolResult = Agent::AgentToolHandlers::Instance().Execute(toolCall.first, toolCall.second);
            } catch (const std::exception& e) {
                // Tool execution threw; mark as failure but continue
                toolResult.error = std::string("tool_exception: ") + e.what();
                toolResult.output.clear();
            } catch (...) {
                // Catch-all for untyped exceptions (backend errors, etc.)
                toolResult.error = "tool_exception: unknown backend error during tool execution (check registry)";
                toolResult.output.clear();
            }

            InferenceResult::ToolCallRecord record;
            record.toolName = toolCall.first;
            record.callId = callId;
            record.success = toolResult.isSuccess();
            record.output = toolResult.isSuccess()
                                ? (toolResult.output.empty() ? "Tool completed successfully." : toolResult.output)
                                : (toolResult.error.empty() ? toolResult.output : toolResult.error);
            bridgeResult.toolTrace.push_back(std::move(record));

            Agent::ChatMessage toolMessage;
            toolMessage.role = "tool";
            toolMessage.content = BuildToolMessageContent(toolResult);
            toolMessage.tool_call_id = callId;
            toolMessage.tool_calls = json();
            messages.push_back(std::move(toolMessage));
        }
    }

    if (!latestResponse.empty()) {
        std::string qualityReason;
        if (ValidateQualityGate(bridgeResult, latestResponse, qualityReason)) {
            bridgeResult.success = true;
            bridgeResult.response = latestResponse + "\n\n[INFO] Agent step limit reached.";
            return bridgeResult;
        }

        bridgeResult.error = "quality validation failed after step limit: " + qualityReason;
        return bridgeResult;
    }

    bridgeResult.error = "agent step limit reached without a final response";
    return bridgeResult;
}

} // namespace Agentic
} // namespace RawrXD
