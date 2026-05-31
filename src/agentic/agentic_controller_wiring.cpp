// ============================================================================
// agentic_controller_wiring.cpp
// Implementation of wiring layer for MinimalAgentController
// ============================================================================

#include "agentic_controller_wiring.h"
#include "../agent/model_invoker.hpp"
#include "../agent/model_policy_router.hpp"
#include "../cli/swarm_orchestrator.h"
#include "../cpu_inference_engine.h"
#include "../logging/Logger.h"
#include "../security/InputSanitizer.h"
#include "AgentToolHandlers.h"
#include "agent_controller_minimal.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>


namespace rawrxd
{

// Logging macros — wired to Logger singleton
#define LOG_INFO(msg)  RawrXD::Logging::Logger::instance().info(msg, "AgenticWiring")
#define LOG_ERROR(msg) RawrXD::Logging::Logger::instance().error(msg, "AgenticWiring")
#define LOG_WARNING(msg) RawrXD::Logging::Logger::instance().warning(msg, "AgenticWiring")

// Global inference engine reference (set during initialization)
static RawrXD::InferenceEngine* g_inference_engine = nullptr;
static bool g_agentic_initialized = false;

namespace
{

constexpr uint32_t kDefaultSwarmWaitMs = 1500;
constexpr uint32_t kMaxSwarmWaitMs = 30000;
constexpr const char* kSwarmPendingPrefix = "[SwarmPending]";

struct SwarmTask
{
    std::string id;
    std::string prompt;
    std::string dependency;
    int priority = 0;
    bool requiresCodeContext = true;
};

struct SwarmPlan
{
    std::vector<SwarmTask> tasks;
    std::string mergeStrategy;
};

uint32_t GetSwarmWaitTimeoutMs()
{
    const char* raw = std::getenv("RAWRXD_SWARM_WAIT_MS");
    if (!raw || !*raw)
    {
        return kDefaultSwarmWaitMs;
    }

    char* end = nullptr;
    const unsigned long parsed = std::strtoul(raw, &end, 10);
    if (end == raw || parsed == 0)
    {
        return kDefaultSwarmWaitMs;
    }
    return static_cast<uint32_t>(std::min<unsigned long>(parsed, kMaxSwarmWaitMs));
}

bool IsTruthyEnvVar(const char* name)
{
    if (!name || !*name)
    {
        return false;
    }

    const char* value = std::getenv(name);
    if (!value || !*value)
    {
        return false;
    }

    return !(value[0] == '0' || value[0] == 'n' || value[0] == 'N' || value[0] == 'f' || value[0] == 'F');
}

std::string TrimWhitespace(std::string value)
{
    const size_t first = value.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
    {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\n\r");
    return value.substr(first, last - first + 1);
}

std::vector<uint32_t> BuildSwarmLayerPath(RawrXD::Swarm::SwarmOrchestrator& swarm)
{
    std::vector<RawrXD::Swarm::LayerShard> shards = swarm.getShardList();
    std::sort(shards.begin(), shards.end(),
              [](const auto& left, const auto& right)
              {
                  if (left.layerStart != right.layerStart)
                  {
                      return left.layerStart < right.layerStart;
                  }
                  return left.layerEnd < right.layerEnd;
              });

    std::vector<uint32_t> layerPath;
    for (const auto& shard : shards)
    {
        for (uint32_t layer = shard.layerStart; layer <= shard.layerEnd; ++layer)
        {
            if (layerPath.empty() || layerPath.back() != layer)
            {
                layerPath.push_back(layer);
            }
        }
    }
    return layerPath;
}

bool IsSwarmAvailable()
{
    auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
    if (!swarm.isInitialized())
    {
        return false;
    }
    if (swarm.getNodeCount() == 0 || swarm.getShardCount() == 0)
    {
        return false;
    }

    const auto nodes = swarm.getNodeList();
    return std::any_of(nodes.begin(), nodes.end(),
                       [](const RawrXD::Swarm::SwarmNode& node)
                       {
                           return node.state == RawrXD::Swarm::NodeState::Active ||
                                  node.state == RawrXD::Swarm::NodeState::Overloaded;
                       });
}

std::optional<std::string> TryDistributedSwarmResponse(const std::string& fullPrompt,
                                                       const std::string& requestIdPrefix)
{
    auto& swarm = RawrXD::Swarm::SwarmOrchestrator::instance();
    if (!IsSwarmAvailable())
    {
        LOG_WARNING("[AgentController] Distributed swarm backend requested but the swarm is not ready");
        return std::nullopt;
    }

    std::vector<uint32_t> layerPath = BuildSwarmLayerPath(swarm);
    if (layerPath.empty())
    {
        LOG_WARNING("[AgentController] Distributed swarm backend requested but layer path is empty");
        return std::nullopt;
    }

    std::vector<char> outputBuffer(8192, 0);
    uint64_t bytesWritten = 0;

    RawrXD::Swarm::SwarmInferenceRequest request{};
    request.requestId =
        requestIdPrefix + "-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    request.layerPath = std::move(layerPath);
    request.inputData = const_cast<char*>(fullPrompt.data());
    request.inputSize = static_cast<uint64_t>(fullPrompt.size());
    request.outputData = outputBuffer.data();
    request.outputCapacity = static_cast<uint64_t>(outputBuffer.size());
    request.outputSizeWritten = &bytesWritten;
    request.currentLayer = 0;
    request.onComplete = nullptr;
    request.onCompleteUserData = nullptr;

    const auto submitResult = swarm.submitInference(request);
    if (!submitResult.success)
    {
        LOG_WARNING(std::string("[AgentController] Distributed swarm submit failed: ") + submitResult.detail);
        return std::nullopt;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(GetSwarmWaitTimeoutMs());
    while (bytesWritten == 0 && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    if (bytesWritten == 0)
    {
        swarm.cancelInference(request.requestId);
        LOG_WARNING("[AgentController] Distributed swarm request timed out waiting for a text response");
        return std::nullopt;
    }

    const std::string response(outputBuffer.data(), static_cast<size_t>(bytesWritten));
    const std::string trimmed = TrimWhitespace(response);
    if (trimmed.empty() || trimmed.rfind(kSwarmPendingPrefix, 0) == 0 || trimmed == TrimWhitespace(fullPrompt))
    {
        LOG_WARNING("[AgentController] Distributed swarm returned a non-usable text payload; falling back");
        return std::nullopt;
    }

    LOG_INFO("[AgentController] Routed request through distributed swarm backend");
    return trimmed;
}

SwarmPlan BuildSwarmPlan(const std::string& prompt)
{
    SwarmPlan plan;
    plan.tasks.push_back({
        "t1",
        "analyze request: " + prompt,
        "",
        0,
        true,
    });
    plan.tasks.push_back({
        "t2",
        "generate implementation approach for: " + prompt,
        "t1",
        1,
        true,
    });
    plan.tasks.push_back({
        "t3",
        "produce final code output for: " + prompt,
        "t2",
        2,
        true,
    });
    plan.mergeStrategy = "sequential";
    return plan;
}

std::optional<std::string> ExecuteSwarmPlan(const SwarmPlan& plan)
{
    std::unordered_map<std::string, std::string> results;
    for (const auto& task : plan.tasks)
    {
        std::string taskPrompt = task.prompt;
        if (!task.dependency.empty())
        {
            const auto dependencyIt = results.find(task.dependency);
            if (dependencyIt == results.end())
            {
                LOG_WARNING("[AgentController] Swarm plan dependency missing for task: " + task.id);
                return std::nullopt;
            }
            taskPrompt += "\n\nPrevious step output:\n" + dependencyIt->second;
        }

        const auto response = TryDistributedSwarmResponse(taskPrompt, "agentic-swarm-plan-" + task.id);
        if (!response)
        {
            LOG_WARNING("[AgentController] Swarm plan task failed: " + task.id);
            return std::nullopt;
        }
        results[task.id] = *response;
    }

    if (plan.tasks.empty())
    {
        return std::nullopt;
    }

    const auto finalIt = results.find(plan.tasks.back().id);
    if (finalIt == results.end())
    {
        return std::nullopt;
    }
    return finalIt->second;
}

bool IsRemoteProviderAvailable(const ModelRoute& route)
{
    return route.provider != ProviderType::SwarmDistributed && route.isConfigured();
}

int WriteInteropOutput(const std::string& out, char* out_buf, unsigned int out_buf_size, unsigned int* out_required)
{
    const unsigned int required = static_cast<unsigned int>(out.size() + 1);
    if (out_required)
    {
        *out_required = required;
    }

    if (out_buf && out_buf_size > 0)
    {
        const size_t copyLen = std::min<size_t>(out.size(), static_cast<size_t>(out_buf_size - 1));
        if (copyLen > 0)
        {
            std::memcpy(out_buf, out.data(), copyLen);
        }
        out_buf[copyLen] = '\0';
    }

    return 0;
}

std::string ExecuteToolBackend(const std::string& toolName, const std::map<std::string, std::string>& params)
{
    try
    {
        if (toolName == "file_read")
        {
            const auto it = params.find("path");
            if (it == params.end() || it->second.empty() || it->second.find("..") != std::string::npos)
            {
                return "{\"error\":\"invalid file path\"}";
            }

            std::ifstream file(it->second, std::ios::binary);
            if (!file.is_open())
            {
                return std::string("{\"error\":\"failed to open file: ") + it->second + "\"}";
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            std::replace(content.begin(), content.end(), '"', '\'');
            return std::string("{\"content\":\"") + content.substr(0, 10000) + "\"}";
        }

        if (toolName == "terminal_execute")
        {
            const auto it = params.find("command");
            if (it == params.end() || it->second.empty())
            {
                return "{\"error\":\"missing command\"}";
            }

            static const std::vector<std::string> allowedPrefixes = {"git", "cmake", "ninja", "make", "ctest", "echo",
                                                                     "cat", "ls",    "dir",   "pwd",  "cl"};

            bool allowed = false;
            for (const auto& prefix : allowedPrefixes)
            {
                if (it->second.rfind(prefix, 0) == 0)
                {
                    allowed = true;
                    break;
                }
            }

            if (!allowed)
            {
                return std::string("{\"error\":\"command not allowlisted: ") + it->second + "\"}";
            }

            std::array<char, 4096> readBuffer{};
            std::string result;
#ifdef _WIN32
            const std::string capturedCommand =
                it->second.find("2>") == std::string::npos ? it->second + " 2>&1" : it->second;
            FILE* pipe = _popen(capturedCommand.c_str(), "r");
#else
            const std::string capturedCommand =
                it->second.find("2>") == std::string::npos ? it->second + " 2>&1" : it->second;
            FILE* pipe = popen(capturedCommand.c_str(), "r");
#endif
            if (!pipe)
            {
                return "{\"error\":\"failed to execute command\"}";
            }

            while (fgets(readBuffer.data(), static_cast<int>(readBuffer.size()), pipe) != nullptr)
            {
                result += readBuffer.data();
            }

#ifdef _WIN32
            const int exitCode = _pclose(pipe);
#else
            const int exitCode = pclose(pipe);
#endif
            std::replace(result.begin(), result.end(), '"', '\'');
            return std::string("{\"stdout\":\"") + result + "\",\"exit_code\":" + std::to_string(exitCode) + "}";
        }
    }
    catch (const std::exception& e)
    {
        return std::string("{\"error\":\"") + e.what() + "\"}";
    }

    return std::string("{\"error\":\"unsupported tool: ") + toolName + "\"}";
}

}  // namespace

// Callback function for LLM calls from agent controller
std::string getGlobalLLMResponse(const std::string& system_prompt, const std::string& user_message,
                                 const std::string& model_path)
{
    const std::string full_prompt = system_prompt + "\n\nUser: " + user_message + "\n\nAssistant:";
    const ModelRoute route = ModelPolicyRouter::Select(system_prompt, user_message, model_path);
    const ModelProviderConfig providerConfig = route.toProviderConfig();
    const bool forceLocalSwarm = IsTruthyEnvVar("RAWRXD_FORCE_LOCAL_SWARM");
    const bool shouldUseSwarm =
        providerConfig.enableSwarm || route.provider == ProviderType::SwarmDistributed || forceLocalSwarm;

    if (shouldUseSwarm && IsSwarmAvailable())
    {
        const SwarmPlan plan = BuildSwarmPlan(full_prompt);
        if (const auto swarmResponse = ExecuteSwarmPlan(plan))
        {
            return *swarmResponse;
        }
        if (forceLocalSwarm)
        {
            LOG_ERROR(
                "[AgentController] Strict local swarm mode active and distributed swarm returned no usable output");
            return "Error: Strict local swarm mode requires a usable distributed swarm response";
        }
        LOG_WARNING("[AgentController] Falling back after gated swarm execution returned no usable output");
    }
    else if (shouldUseSwarm)
    {
        if (forceLocalSwarm)
        {
            LOG_ERROR("[AgentController] Strict local swarm mode active but distributed swarm is unavailable");
            return "Error: Strict local swarm mode requires an available distributed swarm backend";
        }
        LOG_WARNING("[AgentController] Swarm was enabled but the distributed swarm backend is not available");
    }

    if (!forceLocalSwarm && IsRemoteProviderAvailable(route))
    {
        try
        {
            ModelInvoker invoker;
            invoker.setProviderConfig(providerConfig);
            const LLMResponse response =
                invoker.queryRaw(system_prompt, user_message, route.maxTokens, route.temperature);
            if (response.success && !response.rawOutput.empty())
            {
                LOG_INFO("[AgentController] Routed request through scheduled provider role");
                return response.rawOutput;
            }
            LOG_WARNING("[AgentController] Scheduled provider returned no usable output: " + response.error);
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("[AgentController] Scheduled provider failed: " + std::string(e.what()));
            if (!g_inference_engine)
            {
                return "Error: Scheduled provider failed and no local inference engine is available";
            }
        }
    }
    else if (!forceLocalSwarm && route.provider == ProviderType::SwarmDistributed)
    {
        LOG_WARNING(
            "[AgentController] Swarm provider selected without a usable swarm response; falling back to CPU backend");
    }

    if (forceLocalSwarm)
    {
        LOG_ERROR("[AgentController] Strict local swarm mode blocked fallback to remote and CPU backends");
        return "Error: Strict local swarm mode blocked fallback after distributed swarm failure";
    }

    if (!g_inference_engine)
    {
        LOG_ERROR("Agentic wiring: no inference engine available");
        return "Error: Inference engine not available";
    }

    try
    {
        LOG_INFO("[AgentController] Processing LLM request for model: " + model_path);

        if (!g_inference_engine->IsModelLoaded())
        {
            return "Error: Inference engine has no loaded model";
        }

        std::string text;
        auto input_tokens = g_inference_engine->Tokenize(full_prompt);
        if (!input_tokens.empty())
        {
            g_inference_engine->GenerateStreaming(
                input_tokens, 1024, [&](const std::string& token_text) { text += token_text; }, []() {});
        }

        if (text.empty())
        {
            return "Error: Empty completion from backend";
        }

        const size_t assistant_pos = text.find("Assistant:");
        if (assistant_pos != std::string::npos)
        {
            text = text.substr(assistant_pos + 10);
        }

        const size_t first = text.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
        {
            return "Error: Empty completion after trim";
        }
        const size_t last = text.find_last_not_of(" \t\n\r");
        return text.substr(first, last - first + 1);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("[AgentController] LLM call failed: " + std::string(e.what()));
        return "Error: " + std::string(e.what());
    }
}

void initializeAgentControllerWiring(RawrXD::InferenceEngine* inference_engine)
{
    if (g_agentic_initialized)
    {
        LOG_WARNING("[AgentController] Wiring already initialized");
        return;
    }

    g_inference_engine = inference_engine;

    // Initialize the agent controller
    auto& controller = MinimalAgentController::instance();
    controller.setInferenceEngine(inference_engine);
    controller.initialize();

    g_agentic_initialized = true;
    LOG_INFO("[AgentController] Wiring initialized with inference engine");
}

MinimalAgenticResponse processAgenticRequest(const MinimalAgenticRequest& request)
{
    if (!g_agentic_initialized)
    {
        return {
            "", false, "Agentic layer not initialized. Call initializeAgentControllerWiring first.", 0, {}, {},
        };
    }

    try
    {
        auto& controller = MinimalAgentController::instance();
        if (!request.workspace_root.empty())
        {
            const auto sanitized = RawrXD::Security::InputSanitizer::instance().sanitizePath(request.workspace_root);
            const std::string& root = sanitized.sanitized;
            if (!root.empty())
            {
                controller.setWorkspaceRoot(root);
                RawrXD::Agent::ToolGuardrails guards = RawrXD::Agent::AgentToolHandlers::GetGuardrails();
                guards.allowedRoots.clear();
                guards.allowedRoots.push_back(root);
                RawrXD::Agent::AgentToolHandlers::SetGuardrails(guards);
            }
        }
        return controller.process(request);
    }
    catch (const std::exception& e)
    {
        return {
            "", false, std::string("Exception processing agentic request: ") + e.what(), 0, {}, {},
        };
    }
}

bool isAgenticLayerAvailable()
{
    return g_agentic_initialized && MinimalAgentController::instance().isAvailable();
}

std::string getLLMResponse(const std::string& system_prompt, const std::string& user_message,
                           const std::string& model_path)
{
    return getGlobalLLMResponse(system_prompt, user_message, model_path);
}

bool executePlanWithTelemetry(const std::string& userIntent, std::string* finalOutput, ExecutePlanTelemetry* telemetry)
{
    PromotedAgentController controller([](const std::string& toolName, const std::map<std::string, std::string>& params)
                                       { return ExecuteToolBackend(toolName, params); },
                                       [](const std::string& prompt)
                                       {
                                           rawrxd::TrustEvent event;
                                           event.requestId = "promoted-plan";
                                           const std::string response = getGlobalLLMResponse(prompt, "", "");
                                           return std::make_pair(response, std::move(event));
                                       });

    const bool success = controller.ExecuteUserIntent(userIntent, finalOutput);
    if (telemetry)
    {
        *telemetry = ExecutePlanTelemetry{};
        for (const auto& event : controller.telemetry().events())
        {
            telemetry->classification = static_cast<int>(event.classification);
            telemetry->inferenceLatencyMs = event.inferenceLatencyMs;
            telemetry->retryLevelAttempted = event.retryLevelAttempted;
            telemetry->inputTokens = static_cast<unsigned int>(event.inputTokens);
            telemetry->outputTokens = static_cast<unsigned int>(event.outputTokens);
            if (event.classification == TrustClassification::DeterministicFallback)
            {
                telemetry->fallbackUsed = 1;
            }
        }
    }

    return success;
}

CompletionResult requestInlineCompletion(const std::string& prefix, const EditorContext& context)
{
    if (!g_agentic_initialized)
    {
        return {"", false, TrustScore::ModelInvalid, "Agentic wiring not initialized."};
    }

    try
    {
        auto& controller = MinimalAgentController::instance();
        if (!controller.isAvailable())
        {
            return {"", false, TrustScore::ModelInvalid, "Minimal controller unavailable."};
        }

        return controller.OnInlineCompletionRequest(prefix, context);
    }
    catch (const std::exception& e)
    {
        return {"", false, TrustScore::ModelInvalid, std::string("Inline completion bridge failed: ") + e.what()};
    }
    catch (...)
    {
        return {"", false, TrustScore::ModelInvalid, "Inline completion bridge failed."};
    }
}

std::string getSessionPlanGraphSummary(const std::string& sessionId, size_t maxNodes)
{
    if (!g_agentic_initialized)
    {
        return "Agentic layer not initialized.";
    }

    try
    {
        auto& controller = MinimalAgentController::instance();
        if (!controller.isAvailable())
        {
            return "Minimal controller unavailable.";
        }

        return controller.DescribeSessionPlanGraph(sessionId, maxNodes);
    }
    catch (const std::exception& e)
    {
        return std::string("Failed to read plan graph: ") + e.what();
    }
    catch (...)
    {
        return "Failed to read plan graph.";
    }
}

void reportInlineCompletionFeedback(const std::string& sessionId, const std::string& planId, bool accepted,
                                    const std::string& detail)
{
    if (!g_agentic_initialized || planId.empty())
    {
        return;
    }

    try
    {
        auto& controller = MinimalAgentController::instance();
        if (!controller.isAvailable())
        {
            return;
        }

        controller.RecordInlineCompletionFeedback(sessionId, planId, accepted, detail);
    }
    catch (...)
    {
        // Feedback routing is best-effort and must not break ghost text.
    }
}

extern "C" __declspec(dllexport) int ExecutePlanWithTelemetry(const char* user_intent, char* out_buf,
                                                              unsigned int out_buf_size, unsigned int* out_required,
                                                              ExecutePlanTelemetry* out_telemetry)
{
    try
    {
        const std::string input = user_intent ? user_intent : "";
        std::string output;
        const bool success = executePlanWithTelemetry(input, &output, out_telemetry);
        WriteInteropOutput(output, out_buf, out_buf_size, out_required);
        return success ? 0 : -2;
    }
    catch (...)
    {
        if (out_required)
        {
            *out_required = 0;
        }
        if (out_buf && out_buf_size > 0)
        {
            out_buf[0] = '\0';
        }
        if (out_telemetry)
        {
            *out_telemetry = ExecutePlanTelemetry{};
        }
        return -1;
    }
}

}  // namespace rawrxd
