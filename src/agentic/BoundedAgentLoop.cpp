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

#include <sstream>
#include <thread>
#include <chrono>
#include <windows.h>
#include <winhttp.h>

// SCAFFOLD_295: BoundedAgentLoop integration


// SCAFFOLD_060: BoundedAgentLoop and cycle limit


#pragma comment(lib, "winhttp.lib")

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

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

std::string ToUtf8(const std::wstring& s) {
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, out.data(), len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
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
    // Set up LLM backend (default to Ollama if not configured)
    LLMChatFunction llm;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        llm = m_llmBackend;
    }
    if (!llm) {
        std::string baseUrl = m_config.ollamaBaseUrl;
        llm = [baseUrl](const LLMChatRequest& req) {
            return OllamaChat(req, baseUrl);
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
                json diagArgs = {{"file", toolResult.filePath}};
                auto diagResult = AgentToolHandlers::GetDiagnostics(diagArgs);

                if (diagResult.isSuccess() && !diagResult.output.empty() &&
                    diagResult.output != "No diagnostics") {
                    // Inject diagnostics as additional context
                    json diagMsg;
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
    return {
        {"role", "system"},
        {"content", AgentToolHandlers::GetSystemPrompt(
            m_config.workingDirectory, m_config.openFiles)}
    };
}

json BoundedAgentLoop::BuildUserMessage(const std::string& prompt) {
    return {
        {"role", "user"},
        {"content", prompt}
    };
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
    toolCall["function"] = {
        {"name", response.toolName},
        {"arguments", response.toolArgs.dump()}
    };
    json toolCalls = json::array();
    toolCalls.push_back(toolCall);
    msg["tool_calls"] = toolCalls;

    return msg;
}

// ============================================================================
// Default LLM backend — Ollama /api/chat
// ============================================================================

LLMChatResponse BoundedAgentLoop::OllamaChat(const LLMChatRequest& request,
                                               const std::string& baseUrl) {
    LLMChatResponse response;

    // Build Ollama chat request body
    json body;
    body["model"] = request.model;
    body["messages"] = request.messages;
    body["stream"] = false;
    body["options"] = {
        {"temperature", request.temperature},
        {"num_predict", request.maxTokens}
    };

    // Include tools if available
    if (!request.tools.empty()) {
        body["tools"] = request.tools;
    }

    std::string bodyStr = body.dump();

    // Parse URL
    // Expected format: http://localhost:11434
    std::wstring host = L"localhost";
    INTERNET_PORT port = 11434;

    // Extract host/port from baseUrl
    size_t colonSlash = baseUrl.find("://");
    if (colonSlash != std::string::npos) {
        std::string hostPort = baseUrl.substr(colonSlash + 3);
        size_t colonPos = hostPort.find(':');
        if (colonPos != std::string::npos) {
            host = ToWide(hostPort.substr(0, colonPos));
            try {
                port = static_cast<INTERNET_PORT>(std::stoi(hostPort.substr(colonPos + 1)));
            } catch (...) {}
        } else {
            host = ToWide(hostPort);
        }
    }

    // WinHTTP request
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Agent/1.0",
                                      WINHTTP_ACCESS_TYPE_NO_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        response.error = "WinHttpOpen failed";
        return response;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        response.error = "WinHttpConnect failed";
        return response;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/chat",
                                             nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        response.error = "WinHttpOpenRequest failed";
        return response;
    }

    // Set timeout (5 minutes for model inference)
    DWORD timeout = 300000;
    WinHttpSetTimeouts(hRequest, timeout, timeout, timeout, timeout);

    // Send request
    std::wstring headers = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.size(),
                                    (LPVOID)bodyStr.data(), (DWORD)bodyStr.size(),
                                    (DWORD)bodyStr.size(), 0);
    if (!sent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        response.error = "Failed to send request to Ollama";
        return response;
    }

    BOOL received = WinHttpReceiveResponse(hRequest, nullptr);
    if (!received) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        response.error = "No response from Ollama (is it running?)";
        return response;
    }

    // Read response
    std::string responseBody;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            responseBody.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse response
    try {
        json respJson = json::parse(responseBody);

        if (respJson.contains("error")) {
            response.error = respJson["error"].get<std::string>();
            return response;
        }

        response.success = true;

        // Extract message
        if (respJson.contains("message")) {
            json respMsg = respJson["message"];

            if (respMsg.contains("content") && respMsg["content"].is_string()) {
                response.content = respMsg["content"].get<std::string>();
            }

            // Check for tool calls (Ollama format)
            if (respMsg.contains("tool_calls") && respMsg["tool_calls"].is_array() &&
                !respMsg["tool_calls"].empty()) {

                json tcVal = respMsg["tool_calls"][static_cast<size_t>(0)]; // Handle first tool call
                response.hasToolCall = true;

                if (tcVal.contains("function")) {
                    json funcVal = tcVal["function"];
                    if (funcVal.contains("name")) {
                        response.toolName = funcVal["name"].get<std::string>();
                    }
                    if (funcVal.contains("arguments")) {
                        if (funcVal["arguments"].is_string()) {
                            response.toolArgs = json::parse(funcVal["arguments"].get<std::string>());
                        } else {
                            response.toolArgs = funcVal["arguments"];
                        }
                    }
                }

                if (tcVal.contains("id")) {
                    response.toolCallId = tcVal["id"].get<std::string>();
                } else {
                    response.toolCallId = "call_" + std::to_string(NowMs());
                }
            }
        }

        // Token counts
        if (respJson.contains("prompt_eval_count")) {
            response.promptTokens = respJson["prompt_eval_count"].get<int>();
        }
        if (respJson.contains("eval_count")) {
            response.completionTokens = respJson["eval_count"].get<int>();
        }

    } catch (const std::exception& ex) {
        response.success = false;
        response.error = std::string("Failed to parse Ollama response: ") + ex.what();
    }

    return response;
}
