// =============================================================================
// OrchestratorBridge.cpp — Minimal Wiring Layer for CLI
// =============================================================================
#include "OrchestratorBridge.h"
#include "ToolCallResult.h"
#include "../logging/Logger.h"
#include "../security/InputSanitizer.h"
#include "../core/ConfigurationValidator.h"
#include "../inference/PerformanceMonitor.h"
#include "ErrorRecoveryManager.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>
#include <thread>
#include <stdexcept>
#include <unordered_set>

using RawrXD::Agent::OrchestratorBridge;
using RawrXD::Agent::InferenceResult;
using RawrXD::Agent::ChatMessage;
using RawrXD::Agent::ToolCallResult;
namespace Prediction = RawrXD::Prediction;
using json = nlohmann::json;

namespace {

std::string ToLowerCopy(const std::string& value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return lowered;
}

bool ContainsInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }

    return ToLowerCopy(haystack).find(ToLowerCopy(needle)) != std::string::npos;
}

std::string BuildToolMessageContent(const ToolCallResult& result) {
    if (result.isSuccess()) {
        if (!result.output.empty()) {
            return result.output;
        }
        return "Tool completed successfully.";
    }

    if (!result.error.empty()) {
        return "Error: " + result.error;
    }

    if (!result.output.empty()) {
        return "Error: " + result.output;
    }

    return "Error: Tool execution failed.";
}

RawrXD::Logging::Logger& GetLogger() {
    return RawrXD::Logging::Logger::instance();
}

void EnsureValidationRules() {
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    auto& validator = RawrXD::Core::ConfigurationValidator::instance();
    validator.addRule("ollama", {"host", [](const std::string& value) { return !value.empty(); },
        "Ollama host must not be empty", true});
    validator.addRule("ollama", {"port", [](const std::string& value) {
        try {
            int port = std::stoi(value);
            return port > 0 && port <= 65535;
        } catch (...) {
            return false;
        }
    }, "Ollama port must be within 1-65535", true});
    validator.addRule("ollama", {"chat_model", [](const std::string& value) { return !value.empty(); },
        "Chat model must be selected", false});
    validator.addRule("ollama", {"fim_model", [](const std::string& value) { return !value.empty(); },
        "FIM model must be selected", false});
}

} // namespace

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

OrchestratorBridge& OrchestratorBridge::Instance() {
    static OrchestratorBridge instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Initialization - Simplified for CLI
// ---------------------------------------------------------------------------

bool OrchestratorBridge::Initialize(const std::string& workingDir,
                                     const std::string& ollamaUrl)
{
    EnsureValidationRules();
    auto& logger = GetLogger();
    auto& sanitizer = RawrXD::Security::InputSanitizer::instance();
    RawrXD::Security::SanitizationResult pathSan = sanitizer.sanitizePath(workingDir);
    m_workingDir = pathSan.sanitized;
    if (pathSan.wasModified) {
        logger.warning("OrchestratorBridge", "Working directory sanitized: " + m_workingDir);
    }

    // Configure Ollama — sensible defaults
    m_ollamaConfig.host = "127.0.0.1";
    m_ollamaConfig.port = 11434;
    m_ollamaConfig.chat_model = "";
    m_ollamaConfig.fim_model  = "";
    m_ollamaConfig.timeout_ms = 120000;
    m_ollamaConfig.temperature = 0.1f;
    m_ollamaConfig.num_ctx = 8192;

    // Parse URL if provided
    if (!ollamaUrl.empty()) {
        std::string url = ollamaUrl;
        if (url.find("http://") == 0) url = url.substr(7);
        if (url.find("https://") == 0) url = url.substr(8);
        auto colonPos = url.find(':');
        if (colonPos != std::string::npos) {
            m_ollamaConfig.host = url.substr(0, colonPos);
            try {
                m_ollamaConfig.port = static_cast<uint16_t>(
                    std::stoi(url.substr(colonPos + 1)));
            } catch (...) {}
        } else {
            m_ollamaConfig.host = url;
        }
    }

    RawrXD::Core::ValidationResult validation =
        RawrXD::Core::ConfigurationValidator::instance().validateSection("ollama", {
            {"host", m_ollamaConfig.host},
            {"port", std::to_string(m_ollamaConfig.port)},
            {"chat_model", m_ollamaConfig.chat_model},
            {"fim_model", m_ollamaConfig.fim_model}
        });

    for (const auto& warn : validation.warnings) {
        logger.warning("OrchestratorBridge", warn);
    }
    if (!validation.valid) {
        for (const auto& err : validation.errors) {
            logger.error("OrchestratorBridge", err);
        }
    }

    SetWorkingDirectory(m_workingDir);

    if (!m_ollamaClient) {
        m_ollamaClient = std::make_unique<AgentOllamaClient>(m_ollamaConfig);
    } else {
        ApplyConfig();
    }

    m_initialized = EnsureClientReady();
    return m_initialized;
}

// ---------------------------------------------------------------------------
// Agent Execution - Simplified for CLI
// ---------------------------------------------------------------------------

std::string OrchestratorBridge::RunAgent(const std::string& userPrompt) {
    if (!EnsureClientReady()) {
        return "[ERROR] OrchestratorBridge not initialized";
    }

    auto& logger = GetLogger();
    auto& sanitizer = RawrXD::Security::InputSanitizer::instance();
    auto sanitizedPrompt = sanitizer.sanitizePrompt(userPrompt);
    if (sanitizedPrompt.wasModified) {
        logger.warning("OrchestratorBridge", "Prompt sanitized before dispatch");
    }

    RawrXD::Agent::ToolGuardrails guards = RawrXD::Agent::AgentToolHandlers::GetGuardrails();
    if (guards.allowedRoots.empty() && !m_workingDir.empty()) {
        guards.allowedRoots.push_back(m_workingDir);
        RawrXD::Agent::AgentToolHandlers::SetGuardrails(guards);
    }

    std::vector<ChatMessage> messages;
    messages.push_back({"system", RawrXD::Agent::AgentToolHandlers::GetSystemPrompt(m_workingDir, {}), "", json()});
    messages.push_back({"user", sanitizedPrompt.sanitized, "", json()});

    const json tools = RawrXD::Agent::AgentToolHandlers::GetAllSchemas();
    const int stepLimit = std::max(1, m_maxSteps);
    std::string latestResponse;
    std::unordered_set<std::string> seenToolCalls;
    auto& perf = RawrXD::Inference::PerformanceMonitor::instance();
    auto& recovery = RawrXD::Agentic::ErrorRecoveryManager::instance();
    RawrXD::Agentic::ErrorRecoveryManager::RecoveryConfig recoveryCfg{};
    recoveryCfg.maxRetries = 3;
    recoveryCfg.baseDelay = std::chrono::milliseconds(500);
    recoveryCfg.maxDelay = std::chrono::milliseconds(5000);

    for (int step = 0; step < stepLimit; ++step) {
        perf.startOperation("ollama.chat");
        InferenceResult result;
        try {
            result = recovery.executeWithRecovery([&]() {
                InferenceResult r = m_ollamaClient->ChatSync(messages, tools);
                if (!r.success) {
                    throw std::runtime_error(r.error_message);
                }
                return r;
            }, recoveryCfg);
            perf.endOperation("ollama.chat");
        } catch (const std::exception& ex) {
            perf.recordError("ollama.chat");
            perf.endOperation("ollama.chat");
            logger.error("OrchestratorBridge", std::string("ChatSync failed: ") + ex.what());
            return "[ERROR] " + std::string(ex.what());
        }

        if (!result.response.empty()) {
            latestResponse = result.response;
        }

        if (!result.has_tool_calls || result.tool_calls.empty()) {
            return latestResponse;
        }

        ChatMessage assistantMessage;
        assistantMessage.role = "assistant";
        assistantMessage.content = result.response;
        assistantMessage.tool_calls = json::array();

        for (size_t i = 0; i < result.tool_calls.size(); ++i) {
            const auto& toolCall = result.tool_calls[i];
            const std::string callId = "call_" + std::to_string(step) + "_" + std::to_string(i);

            if (!seenToolCalls.insert(callId).second) {
                logger.warning("OrchestratorBridge", "Duplicate tool call suppressed: " + callId);
                continue;
            }

            json toolCallJson;
            toolCallJson["id"] = callId;
            toolCallJson["type"] = "function";
            toolCallJson["function"] = {
                {"name", toolCall.first},
                {"arguments", toolCall.second}
            };
            assistantMessage.tool_calls.push_back(toolCallJson);
        }

        messages.push_back(std::move(assistantMessage));

        for (size_t i = 0; i < result.tool_calls.size(); ++i) {
            const auto& toolCall = result.tool_calls[i];
            const std::string callId = "call_" + std::to_string(step) + "_" + std::to_string(i);

            ToolCallResult toolResult = RawrXD::Agent::AgentToolHandlers::Instance().Execute(toolCall.first, toolCall.second);

            ChatMessage toolMessage;
            toolMessage.role = "tool";
            toolMessage.content = BuildToolMessageContent(toolResult);
            toolMessage.tool_call_id = callId;
            toolMessage.tool_calls = json();
            messages.push_back(std::move(toolMessage));
        }
    }

    if (latestResponse.empty()) {
        return "[ERROR] Agent stopped after reaching the step limit without a final answer";
    }

    return latestResponse + "\n\n[INFO] Agent step limit reached.";
}

void OrchestratorBridge::RunAgentAsync(const std::string& userPrompt) {
    std::thread([this, userPrompt]() {
        (void)RunAgent(userPrompt);
    }).detach();
}

// ---------------------------------------------------------------------------
// Ghost Text / FIM - Simplified
// ---------------------------------------------------------------------------

Prediction::PredictionResult OrchestratorBridge::RequestGhostText(
    const Prediction::PredictionContext& ctx)
{
    if (!EnsureClientReady()) {
        return Prediction::PredictionResult::Error("OrchestratorBridge not initialized");
    }

    auto& perf = RawrXD::Inference::PerformanceMonitor::instance();
    auto& recovery = RawrXD::Agentic::ErrorRecoveryManager::instance();
    RawrXD::Agentic::ErrorRecoveryManager::RecoveryConfig recoveryCfg{};
    recoveryCfg.maxRetries = 2;
    recoveryCfg.baseDelay = std::chrono::milliseconds(300);
    recoveryCfg.maxDelay = std::chrono::milliseconds(2500);

    perf.startOperation("ollama.fim");
    try {
        InferenceResult result = recovery.executeWithRecovery([&]() {
            InferenceResult r = m_ollamaClient->FIMSync(ctx.prefix, ctx.suffix, ctx.filePath);
            if (!r.success) {
                throw std::runtime_error(r.error_message);
            }
            return r;
        }, recoveryCfg);
        perf.endOperation("ollama.fim");
        return Prediction::PredictionResult::Ok(
            result.response,
            static_cast<int>(result.completion_tokens),
            100);
    } catch (const std::exception& ex) {
        perf.recordError("ollama.fim");
        perf.endOperation("ollama.fim");
        GetLogger().error("OrchestratorBridge", std::string("FIMSync failed: ") + ex.what());
        return Prediction::PredictionResult::Error(ex.what());
    }
}

void OrchestratorBridge::RequestGhostTextStream(
    const Prediction::PredictionContext& ctx,
    Prediction::StreamTokenCallback onToken)
{
    if (!EnsureClientReady()) {
        if (onToken) {
            onToken("", true);
        }
        return;
    }

    m_ollamaClient->FIMStream(
        ctx.prefix,
        ctx.suffix,
        ctx.filePath,
        [onToken](const std::string& token) {
            if (onToken) {
                onToken(token, false);
            }
        },
        [onToken](const std::string&, uint64_t, uint64_t, double) {
            if (onToken) {
                onToken("", true);
            }
        },
        [onToken](const std::string&) {
            if (onToken) {
                onToken("", true);
            }
        });
}

// ---------------------------------------------------------------------------
// Configuration helpers
// ---------------------------------------------------------------------------

bool OrchestratorBridge::EnsureClientReady() {
    if (!m_ollamaClient) {
        m_ollamaClient = std::make_unique<AgentOllamaClient>(m_ollamaConfig);
    }

    ApplyConfig();

    if (!m_ollamaClient->TestConnection()) {
        m_initialized = false;
        return false;
    }

    if (m_ollamaConfig.chat_model.empty() || m_ollamaConfig.fim_model.empty()) {
        RefreshAvailableModels();

        if (m_ollamaConfig.chat_model.empty()) {
            m_ollamaConfig.chat_model = SelectPreferredModel(true);
        }
        if (m_ollamaConfig.fim_model.empty()) {
            m_ollamaConfig.fim_model = SelectPreferredModel(true);
            if (m_ollamaConfig.fim_model.empty()) {
                m_ollamaConfig.fim_model = m_ollamaConfig.chat_model;
            }
        }

        ApplyConfig();
    }

    m_initialized = !m_ollamaConfig.chat_model.empty() || !m_ollamaConfig.fim_model.empty();
    return m_initialized;
}

void OrchestratorBridge::RefreshAvailableModels() {
    if (!m_ollamaClient) {
        return;
    }

    m_availableModels = m_ollamaClient->ListModels();
}

void OrchestratorBridge::ApplyConfig() {
    if (m_ollamaClient) {
        m_ollamaClient->SetConfig(m_ollamaConfig);
    }
}

std::string OrchestratorBridge::SelectPreferredModel(bool preferCoder) const {
    if (m_availableModels.empty()) {
        return "";
    }

    const std::vector<std::string> coderPriority = {
        "qwen2.5-coder", "qwen3-coder", "qwen", "deepseek-coder", "codestral",
        "codellama", "starcoder", "phi-4", "phi4", "phi3", "llama3.3",
        "llama3.2", "llama3.1", "mistral", "gemma"
    };
    const std::vector<std::string> generalPriority = {
        "qwen2.5-coder", "qwen", "llama3.3", "llama3.2", "llama3.1",
        "deepseek", "mistral", "gemma", "phi4", "phi3"
    };

    const auto& priority = preferCoder ? coderPriority : generalPriority;
    for (const auto& preferred : priority) {
        for (const auto& model : m_availableModels) {
            if (ContainsInsensitive(model, preferred)) {
                return model;
            }
        }
    }

    return m_availableModels.front();
}

void OrchestratorBridge::SetModel(const std::string& model) {
    auto sanitized = RawrXD::Security::InputSanitizer::instance().sanitizeModelName(model);
    if (sanitized.wasModified) {
        GetLogger().warning("OrchestratorBridge", "Chat model sanitized from user input");
    }
    m_ollamaConfig.chat_model = sanitized.sanitized;
    if (m_ollamaConfig.fim_model.empty()) {
        m_ollamaConfig.fim_model = sanitized.sanitized;
    }
    ApplyConfig();
    m_initialized = m_initialized || !model.empty();
}

void OrchestratorBridge::SetFIMModel(const std::string& model) {
    auto sanitized = RawrXD::Security::InputSanitizer::instance().sanitizeModelName(model);
    if (sanitized.wasModified) {
        GetLogger().warning("OrchestratorBridge", "FIM model sanitized from user input");
    }
    m_ollamaConfig.fim_model = sanitized.sanitized;
    if (m_ollamaConfig.chat_model.empty()) {
        m_ollamaConfig.chat_model = sanitized.sanitized;
    }
    ApplyConfig();
    m_initialized = m_initialized || !model.empty();
}

void OrchestratorBridge::SetMaxSteps(int steps) {
    if (steps > 0) {
        m_maxSteps = steps;
    }
}

void OrchestratorBridge::SetWorkingDirectory(const std::string& dir) {
    auto sanitized = RawrXD::Security::InputSanitizer::instance().sanitizePath(dir);
    if (sanitized.wasModified) {
        GetLogger().warning("OrchestratorBridge", "Working directory sanitized");
    }
    m_workingDir = sanitized.sanitized;

    RawrXD::Agent::ToolGuardrails guards = RawrXD::Agent::AgentToolHandlers::GetGuardrails();
    guards.allowedRoots.clear();
    if (!m_workingDir.empty()) {
        guards.allowedRoots.push_back(m_workingDir);
    }
    guards.commandTimeoutMs = 30000;
    guards.requireBackupOnWrite = true;
    RawrXD::Agent::AgentToolHandlers::SetGuardrails(guards);
}
