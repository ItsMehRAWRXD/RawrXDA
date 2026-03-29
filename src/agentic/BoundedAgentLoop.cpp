// ============================================================================
// BoundedAgentLoop.cpp — Step-Limited Autonomous Agent Loop Implementation
// ============================================================================
// The deterministic, auditable action layer.
//
// Core Loop:
//   1. Build system prompt with tool schemas
//   2. Send user prompt + history to LLM
//   3. If LLM returns tool_call → execute tool, append result, goto 2
//   4. If LLM returns text → final answer, stop
//   5. If step >= MAX_STEPS → forced stop
//
// Every step is recorded in the AgentTranscript for replay and audit.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "BoundedAgentLoop.h"
#include "AgentOllamaClient.h"

#include <sstream>
#include <thread>
#include <chrono>

// SCAFFOLD_295: BoundedAgentLoop integration


// SCAFFOLD_060: BoundedAgentLoop and cycle limit


using RawrXD::Agent::BoundedAgentLoop;
using RawrXD::Agent::AgentLoopState;
using RawrXD::Agent::ToolCallResult;
using RawrXD::Agent::ToolOutcome;
using RawrXD::Agent::TranscriptStep;
using RawrXD::Agent::LLMChatRequest;
using RawrXD::Agent::LLMChatResponse;
using json = nlohmann::json;

namespace {

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

// Approximate token count (4 chars per token heuristic)
int EstimateTokens(const std::string& text) {
    return static_cast<int>(text.size() / 4);
}

} // anonymous namespace

// ============================================================================
// Construction / Configuration
// ============================================================================

BoundedAgentLoop::BoundedAgentLoop() = default;
BoundedAgentLoop::~BoundedAgentLoop() = default;

void BoundedAgentLoop::Configure(const AgentLoopConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void BoundedAgentLoop::SetLLMBackend(LLMChatFunction backend) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_llmBackend = std::move(backend);
}

void BoundedAgentLoop::SetProgressCallback(AgentProgressCallback callback) {
    m_progressCallback = std::move(callback);
}

void BoundedAgentLoop::SetCompleteCallback(AgentCompleteCallback callback) {
    m_completeCallback = std::move(callback);
}

void BoundedAgentLoop::Cancel() {
    m_cancelled.store(true);
}

// ============================================================================
// Synchronous execution
// ============================================================================

std::string BoundedAgentLoop::Execute(const std::string& userPrompt) {
    m_cancelled.store(false);
    m_currentStep.store(0);
    m_state.store(AgentLoopState::Idle);

    // Reset transcript — AgentTranscript is non-copyable (has mutex),
    // so we use the Reset() method to clear state in-place.
    m_transcript.Reset();
    m_transcript.SetInitialPrompt(userPrompt);
    m_transcript.SetModel(m_config.model);
    m_transcript.SetWorkingDirectory(m_config.workingDirectory);

    std::string result = RunLoop(userPrompt);

    // Save transcript if path configured
    if (!m_config.transcriptPath.empty()) {
        m_transcript.SaveToFile(m_config.transcriptPath);
    }

    return result;
}

// ============================================================================
// Async execution
// ============================================================================

void BoundedAgentLoop::ExecuteAsync(const std::string& userPrompt) {
    std::thread([this, userPrompt]() {
        std::string result = Execute(userPrompt);
        if (m_completeCallback) {
            m_completeCallback(result, m_transcript);
        }
    }).detach();
}

// ============================================================================
// Core loop — bounded by MAX_STEPS
// ============================================================================

std::string BoundedAgentLoop::RunLoop(const std::string& userPrompt) {
    // Set up LLM backend (defaults to native client path)
    LLMChatFunction llm;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        llm = m_llmBackend;
    }
    if (!llm) {
        std::string baseUrl = m_config.ollamaBaseUrl;
        llm = [baseUrl](const LLMChatRequest& req) {
            return NativeChat(req, baseUrl);
        };
    }

    // Build initial message history
    std::vector<json> messages;
    messages.push_back(BuildSystemMessage());
    messages.push_back(BuildUserMessage(userPrompt));

    json toolSchemas = AgentToolHandlers::GetAllSchemas();

    std::string finalAnswer;
    int step = 0;

    while (step < m_config.maxSteps) {
        if (m_cancelled.load()) {
            m_state.store(AgentLoopState::Complete);
            m_transcript.SetOutcome("cancelled");
            return "[Agent cancelled by user]";
        }

        m_currentStep.store(step + 1);

        // Notify progress
        if (m_progressCallback) {
            m_progressCallback(step + 1, m_config.maxSteps,
                "Thinking...", "Waiting for model response");
        }

        // ---- Step 1: Send to LLM ----
        m_state.store(AgentLoopState::WaitingForModel);

        LLMChatRequest request;
        request.messages = messages;
        request.tools = toolSchemas;
        request.model = m_config.model;
        request.temperature = 0.1f;
        request.maxTokens = 2048;

        auto modelStart = NowMs();
        LLMChatResponse response = llm(request);
        auto modelDuration = static_cast<int64_t>(NowMs() - modelStart);

        if (!response.success) {
            // LLM call failed
            TranscriptStep ts;
            ts.stepNumber = step + 1;
            ts.timestampMs = NowMs();
            ts.modelLatencyMs = modelDuration;
            ts.modelResponse = response.error;
            m_transcript.AddStep(ts);

            m_state.store(AgentLoopState::Error);
            m_transcript.SetOutcome("llm_error: " + response.error);
            return "[Agent error: " + response.error + "]";
        }

        // ---- Step 2: Check if tool call or final answer ----
        if (response.hasToolCall) {
            // ---- Execute tool ----
            if (m_progressCallback) {
                m_progressCallback(step + 1, m_config.maxSteps,
                    "Executing: " + response.toolName,
                    response.toolArgs.dump());
            }

            m_state.store(AgentLoopState::ExecutingTool);

            auto toolStart = NowMs();
            ToolCallResult toolResult;

            if (m_config.dryRun) {
                toolResult = ToolCallResult::Ok("[DRY RUN] Would execute: " + response.toolName);
            } else {
                toolResult = DispatchTool(response.toolName, response.toolArgs);
            }

            auto toolDuration = static_cast<int64_t>(NowMs() - toolStart);
            toolResult.durationMs = toolDuration;
            toolResult.toolName = response.toolName;
            toolResult.argsUsed = response.toolArgs;

            // ---- Record in transcript ----
            TranscriptStep ts;
            ts.stepNumber = step + 1;
            ts.timestampMs = NowMs();
            ts.reasoning = response.reasoning;
            ts.toolCallName = response.toolName;
            ts.toolCallArgs = response.toolArgs;
            ts.toolResult = toolResult;
            ts.modelLatencyMs = modelDuration;
            ts.toolLatencyMs = toolDuration;
            ts.tokensSent = EstimateTokens(json(request.messages).dump());
            ts.tokensReceived = EstimateTokens(response.content);
            m_transcript.AddStep(ts);

            // Track file operations
            if (!toolResult.filePath.empty()) {
                if (response.toolName == "read_file" || response.toolName == "search_code") {
                    m_transcript.RecordFileRead(toolResult.filePath);
                } else {
                    m_transcript.RecordFileWrite(toolResult.filePath);
                }
            }

            // ---- Append messages for next iteration ----
            messages.push_back(BuildAssistantToolCallMessage(response));
            messages.push_back(BuildToolResultMessage(response.toolCallId, toolResult));

            // ---- Auto-verify after file mutations ----
            if (m_config.autoVerify && !m_config.dryRun &&
                (response.toolName == "write_file" || response.toolName == "replace_in_file") &&
                !toolResult.filePath.empty()) {

                m_state.store(AgentLoopState::Verifying);
                json diagArgs = nlohmann::json::object({{"file", toolResult.filePath}});
                auto diagResult = AgentToolHandlers::GetDiagnostics(diagArgs);

                if (diagResult.isSuccess() && !diagResult.output.empty() &&
                    diagResult.output != "No diagnostics") {
                    // Inject diagnostics as additional context
                    json diagMsg = nlohmann::json::object();
                    diagMsg["role"] = "system";
                    diagMsg["content"] = "Post-edit diagnostics for " + toolResult.filePath +
                                         ":\n" + diagResult.output;
                    messages.push_back(diagMsg);
                }
            }

        } else {
            // ---- Final answer ----
            finalAnswer = response.content;

            TranscriptStep ts;
            ts.stepNumber = step + 1;
            ts.timestampMs = NowMs();
            ts.modelResponse = response.content;
            ts.reasoning = response.reasoning;
            ts.modelLatencyMs = modelDuration;
            ts.tokensSent = EstimateTokens(json(request.messages).dump());
            ts.tokensReceived = EstimateTokens(response.content);
            m_transcript.AddStep(ts);

            m_state.store(AgentLoopState::Complete);
            m_transcript.SetOutcome("completed");

            if (m_progressCallback) {
                m_progressCallback(step + 1, m_config.maxSteps,
                    "Complete", finalAnswer.substr(0, 200));
            }

            return finalAnswer;
        }

        ++step;
    }

    // Step limit reached
    m_state.store(AgentLoopState::StepLimitReached);
    m_transcript.SetOutcome("step_limit_reached");

    if (m_progressCallback) {
        m_progressCallback(m_config.maxSteps, m_config.maxSteps,
            "Step limit reached", "Agent used all " + std::to_string(m_config.maxSteps) + " steps");
    }

    return "[Agent reached step limit (" + std::to_string(m_config.maxSteps) +
           " steps). Last tool: " +
           (m_transcript.StepCount() > 0
                ? m_transcript.GetStep(m_transcript.StepCount() - 1)->toolCallName
                : "none") + "]";
}

// ============================================================================
// Tool dispatch — routes tool name to handler
// ============================================================================

ToolCallResult BoundedAgentLoop::DispatchTool(const std::string& name, const json& args) {
    if (name == "read_file")        return AgentToolHandlers::ToolReadFile(args);
    if (name == "write_file")       return AgentToolHandlers::WriteFile(args);
    if (name == "replace_in_file")  return AgentToolHandlers::ReplaceInFile(args);
    if (name == "list_dir")         return AgentToolHandlers::ListDir(args);
    if (name == "execute_command")  return AgentToolHandlers::ExecuteCommand(args);
    if (name == "search_code")      return AgentToolHandlers::SearchCode(args);
    if (name == "get_diagnostics")  return AgentToolHandlers::GetDiagnostics(args);

    return ToolCallResult::NotFound(name);
}

// ============================================================================
// Message builders
// ============================================================================

json BoundedAgentLoop::BuildSystemMessage() {
    return json::object({
        {"role", "system"},
        {"content", json(AgentToolHandlers::GetSystemPrompt(
            m_config.workingDirectory, m_config.openFiles))}
    });
}

json BoundedAgentLoop::BuildUserMessage(const std::string& prompt) {
    return json::object({
        {"role", "user"},
        {"content", json(prompt)}
    });
}

json BoundedAgentLoop::BuildToolResultMessage(const std::string& callId,
                                               const ToolCallResult& result) {
    json msg;
    msg["role"] = "tool";
    if (!callId.empty()) msg["tool_call_id"] = callId;

    // Give the LLM a clean view of the result
    if (result.isSuccess()) {
        // Truncate very long outputs to stay within context window
        std::string output = result.output;
        if (output.size() > 16384) {
            output = output.substr(0, 16384) + "\n[OUTPUT TRUNCATED at 16KB]";
        }
        msg["content"] = output;
    } else {
        msg["content"] = "Error (" + std::string(result.outcomeString()) + "): " + result.error;
    }

    return msg;
}

json BoundedAgentLoop::BuildAssistantToolCallMessage(const LLMChatResponse& response) {
    json msg;
    msg["role"] = "assistant";

    // Include both content and tool_calls per OpenAI format
    if (!response.content.empty()) {
        msg["content"] = response.content;
    } else {
        msg["content"] = nullptr;
    }

    json toolCall;
    toolCall["id"] = response.toolCallId.empty() ? "call_0" : response.toolCallId;
    toolCall["type"] = "function";
    toolCall["function"] = nlohmann::json::object({
        {"name", response.toolName},
        {"arguments", response.toolArgs.dump()}
    });
    json toolCalls = json::array();
    toolCalls.push_back(toolCall);
    msg["tool_calls"] = toolCalls;

    return msg;
}

// ============================================================================
// Default LLM backend — native client path (no direct HTTP transport)
// ============================================================================

LLMChatResponse BoundedAgentLoop::NativeChat(const LLMChatRequest& request,
                                               const std::string& baseUrl) {
    LLMChatResponse response;

    // Parse host:port from config URL for backward compatibility with existing config.
    RawrXD::Agent::OllamaConfig cfg;
    cfg.timeout_ms = 300000;
    cfg.temperature = request.temperature;
    cfg.max_tokens = request.maxTokens;
    cfg.chat_model = request.model;

    size_t colonSlash = baseUrl.find("://");
    std::string hostPort = (colonSlash == std::string::npos) ? baseUrl : baseUrl.substr(colonSlash + 3);
    size_t slashPos = hostPort.find('/');
    if (slashPos != std::string::npos) {
        hostPort = hostPort.substr(0, slashPos);
    }
    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        cfg.host = hostPort.substr(0, colonPos);
        try {
            cfg.port = static_cast<uint16_t>(std::stoi(hostPort.substr(colonPos + 1)));
        } catch (...) {
            cfg.port = 11434;
        }
    } else if (!hostPort.empty()) {
        cfg.host = hostPort;
    }

    std::vector<RawrXD::Agent::ChatMessage> nativeMessages;
    nativeMessages.reserve(request.messages.size());
    for (const auto& msg : request.messages) {
        RawrXD::Agent::ChatMessage native;
        if (msg.contains("role") && msg["role"].is_string()) {
            native.role = msg["role"].get<std::string>();
        }
        if (msg.contains("content") && msg["content"].is_string()) {
            native.content = msg["content"].get<std::string>();
        }
        if (msg.contains("tool_call_id") && msg["tool_call_id"].is_string()) {
            native.tool_call_id = msg["tool_call_id"].get<std::string>();
        }
        if (msg.contains("tool_calls")) {
            native.tool_calls = msg["tool_calls"];
        }
        nativeMessages.push_back(std::move(native));
    }

    RawrXD::Agent::AgentOllamaClient client(cfg);
    InferenceResult native = client.ChatSync(nativeMessages, request.tools);
    if (!native.success) {
        response.success = false;
        response.error = native.error_message.empty() ? "Native inference failed" : native.error_message;
        return response;
    }

    response.success = true;
    response.content = native.response;
    response.promptTokens = static_cast<int>(native.prompt_tokens);
    response.completionTokens = static_cast<int>(native.completion_tokens);

    if (native.has_tool_calls && !native.tool_calls.empty()) {
        response.hasToolCall = true;
        response.toolName = native.tool_calls.front().first;
        response.toolArgs = native.tool_calls.front().second;
        response.toolCallId = "call_" + std::to_string(NowMs());
    }

    return response;
}
