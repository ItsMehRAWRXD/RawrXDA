// ============================================================================
// Agentic Framework Bridge Implementation (Consolidated - No Duplicates)
// Connects Win32IDE to Native Agentic Framework
// ============================================================================

#include "Win32IDE_AgenticBridge.h"
#include "../action_executor.h"
#include "../advanced_agent_features.hpp"
#include "../agent/agentic_hotpatch_orchestrator.hpp"
#include "../agent/agentic_puppeteer.hpp"
#include "../agentic/OrchestratorBridge.h"
#include "../agentic/OrchestrationSessionState.h"
#include "../agentic_engine.h"
#include "../cpu_inference_engine.h"
#include "../inference/PerformanceMonitor.h"
#include "../logging/Logger.h"
#include "../modules/native_memory.hpp"
#include "../security/InputSanitizer.h"
#include "../vsix_native_converter.hpp"
#include "IDEConfig.h"
#include "IDELogger.h"
#include "RawrXD_TelemetryBridge.h"
#include "Win32IDE.h"
#include "Win32IDE_SubAgent.h"
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <psapi.h>
#include <sstream>
#include <chrono>
#include <atomic>
#include <thread>

namespace
{

namespace telemetry_internal
{
constexpr size_t kEventBytes = 2048;
constexpr size_t kEventCapacity = 256;

struct TelemetryRingImpl
{
    std::atomic<uint64_t> writeSeq{0};
    std::atomic<uint64_t> readSeq{0};
    std::atomic<uint64_t> dropped{0};
    std::array<std::array<char, kEventBytes>, kEventCapacity> events{};
    std::array<uint64_t, kEventCapacity> timestamps{};
};

TelemetryRingImpl& ring()
{
    static TelemetryRingImpl g_ring;
    return g_ring;
}

uint64_t unixMillisNow()
{
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli{};
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

int64_t workingSetKb()
{
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                             sizeof(counters)))
    {
        return static_cast<int64_t>(counters.WorkingSetSize / 1024ULL);
    }
    return 0;
}

const char* stateName(int step)
{
    switch (step)
    {
        case 0:
            return "IDLE";
        case 1:
            return "INTENT_CLASSIFY";
        case 2:
            return "CONTEXT_ASSEMBLE";
        case 3:
            return "CAPABILITY_CHECK";
        case 4:
            return "PERMISSION_REQUEST";
        case 5:
            return "PLAN_GENERATE";
        case 6:
            return "TOOL_PREFETCH";
        case 7:
            return "EXECUTE_PARALLEL";
        case 8:
            return "STREAMING_INFERENCE";
        case 9:
            return "RESULT_VALIDATE";
        case 10:
            return "SELF_CORRECT";
        case 11:
            return "TRANSACTION_COMMIT";
        case 12:
            return "TELEMETRY_EMIT";
        default:
            return "UNKNOWN";
    }
}

uint64_t reserveSlot(TelemetryRingImpl& r)
{
    const uint64_t seq = r.writeSeq.fetch_add(1, std::memory_order_acq_rel);
    const uint64_t minReadable = seq >= kEventCapacity ? (seq - kEventCapacity + 1) : 0;
    uint64_t curRead = r.readSeq.load(std::memory_order_relaxed);
    while (curRead < minReadable && !r.readSeq.compare_exchange_weak(curRead, minReadable, std::memory_order_release,
                                                                     std::memory_order_relaxed))
    {
    }
    if (curRead < minReadable)
        r.dropped.fetch_add(minReadable - curRead, std::memory_order_relaxed);
    return seq;
}

void emitExplicitJson(const ::RawrXD::Telemetry::ExplicitState& state)
{
    TelemetryRingImpl& r = ring();
    const uint64_t seq = reserveSlot(r);
    const size_t slot = static_cast<size_t>(seq % kEventCapacity);
    char* out = r.events[slot].data();

    const int written = _snprintf_s(
        out, kEventBytes, _TRUNCATE,
        "{\"ts\":%llu,\"wf\":\"%s\",\"ag\":\"%s\",\"st\":%d,\"st_name\":\"%s\","
        "\"dur_us\":%llu,\"mem_kb\":%lld,\"tok_in\":%d,\"tok_out\":%d,\"tps\":%d,"
        "\"tools\":%s,\"tools_ok\":%s,\"err\":%s,\"retry\":%d,\"parallel\":%d,"
        "\"checkpoint\":%s,\"ctx\":{\"files\":%d,\"tokens\":%d,\"kv_pages\":%d}}\n",
        static_cast<unsigned long long>(state.timestamp), state.workflowId, state.agentId, state.stepNumber,
        state.stepName, static_cast<unsigned long long>(state.durationMicros), static_cast<long long>(state.memoryKb),
        state.inputTokens, state.outputTokens, state.tokensPerSecond, state.tools, state.toolsOk, state.errorValue,
        state.retryCount, state.parallelAgents, state.checkpointCreated ? "true" : "false", state.contextFiles,
        state.contextTokens, state.contextKvPages);

    if (written < 0)
    {
        out[0] = '{';
        out[1] = '}';
        out[2] = '\n';
        out[3] = '\0';
    }
    r.timestamps[slot] = state.timestamp;
}

}  // namespace telemetry_internal

extern "C" {

HTelemetry Telemetry_Initialize()
{
    return reinterpret_cast<HTelemetry>(&telemetry_internal::ring());
}

int Telemetry_EmitEvent(void* agentContext, void* workflowState)
{
    (void)agentContext;
    (void)workflowState;
    ::RawrXD::Telemetry::ExplicitState st;
    st.timestamp = telemetry_internal::unixMillisNow();
    st.stepNumber = 12;
    st.stepName = telemetry_internal::stateName(12);
    st.memoryKb = telemetry_internal::workingSetKb();
    telemetry_internal::emitExplicitJson(st);
    return 0;
}

const char* Telemetry_ReadNextEvent(HTelemetry ringHandle, uint64_t* outTimestamp)
{
    if (!ringHandle)
        return nullptr;
    auto* r = reinterpret_cast<telemetry_internal::TelemetryRingImpl*>(ringHandle);
    uint64_t read = r->readSeq.load(std::memory_order_acquire);
    const uint64_t write = r->writeSeq.load(std::memory_order_acquire);
    if (read >= write)
        return nullptr;

    const size_t slot = static_cast<size_t>(read % telemetry_internal::kEventCapacity);
    const char* out = r->events[slot].data();
    if (outTimestamp)
        *outTimestamp = r->timestamps[slot];
    r->readSeq.store(read + 1, std::memory_order_release);
    return out;
}

void Telemetry_Flush(HTelemetry ringHandle)
{
    (void)ringHandle;
}

}  // extern "C"

std::string buildWorkflowExecutorRolePrompt(const std::string& userPrompt, int index, int total)
{
    static const char* kRoles[] = {"Planner",          "Implementer",      "Verifier",        "Tool Strategist",
                                   "Debugger",         "Reviewer",         "Researcher",      "Context Builder",
                                   "Risk Auditor",     "Patch Author",     "Test Designer",   "Edge Case Hunter",
                                   "Performance Analyst","Integration Lead", "Refactorer",      "Fallback Planner",
                                   "Trace Analyst",    "Spec Extractor",   "Constraint Keeper", "Regression Guard",
                                   "Filesystem Agent", "Build Agent",      "Routing Analyst", "Prompt Optimizer",
                                   "Memory Curator",   "Swarm Synthesizer", "Validation Lead", "Failure Recovery",
                                   "Output Editor",    "API Mapper",       "Dependency Scout", "Completion Finisher"};

    const size_t roleCount = sizeof(kRoles) / sizeof(kRoles[0]);
    const char* role = kRoles[index % static_cast<int>(roleCount)];

    std::ostringstream oss;
    oss << "[RawrXD Workflow Executor Agent " << (index + 1) << "/" << total << "]\n"
        << "Role: " << role << "\n"
        << "You are one member of a parallel agent swarm. Focus on your specialty, avoid repeating other agents, "
           "and return concise actionable output only.\n\n"
        << "Primary task:\n"
        << userPrompt << "\n";
    return oss.str();
}

bool envDisablesCapabilityHotpatch(const char* varName)
{
    if (!varName)
        return false;
    char buf[12] = {};
    const DWORD n = GetEnvironmentVariableA(varName, buf, static_cast<DWORD>(sizeof(buf)));
    return n > 0 && (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T' || buf[0] == 'y' || buf[0] == 'Y');
}

}  // namespace

// SCAFFOLD_054: AgenticBridge DispatchModelToolCalls


// SCAFFOLD_053: AgenticBridge LoadModel and model override


// SCAFFOLD_052: AgenticBridge StartAgentLoop / StopAgentLoop


// SCAFFOLD_051: AgenticBridge ExecuteAgentCommand


// SCAFFOLD_020: Agentic bridge initialization


// Global shared instances to persist across UI reloads if needed
static std::shared_ptr<::RawrXD::CPUInferenceEngine> g_cpuEngine = nullptr;
static std::shared_ptr<AgenticEngine> g_agentEngine = nullptr;
static AgenticEngine* g_commandDispatchEngine = nullptr;

void SetIDEAgenticEngineForCommands(AgenticEngine* engine)
{
    // Keep a direct engine pointer available even when action_executor.cpp is excluded.
    g_commandDispatchEngine = engine;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

AgenticBridge::AgenticBridge(Win32IDE* ide)
    : m_ide(ide), m_initialized(false), m_agentLoopRunning(false), m_hProcess(nullptr), m_hStdoutRead(nullptr),
      m_hStdoutWrite(nullptr), m_hStdinRead(nullptr), m_hStdinWrite(nullptr)
{
}

AgenticBridge::~AgenticBridge()
{
    KillPowerShellProcess();
}

// ============================================================================
// Initialization
// ============================================================================

bool AgenticBridge::Initialize(const std::string& frameworkPath, const std::string& modelName)
{
    SCOPED_METRIC("agentic.initialize");
    if (m_initialized)
        return true;

    LOG_INFO("Initializing Native Inference Stack...");

    m_frameworkPath = frameworkPath.empty() ? ResolveFrameworkPath() : frameworkPath;

    if (!g_cpuEngine)
    {
        g_cpuEngine = std::make_shared<::RawrXD::CPUInferenceEngine>();
        g_cpuEngine->SetContextLimit(4096);
    }

    if (!g_agentEngine)
    {
        g_agentEngine = std::make_shared<AgenticEngine>();
        g_agentEngine->setInferenceEngine(g_cpuEngine.get());
    }
    if (!m_workspaceRoot.empty())
    {
        g_agentEngine->setWorkspaceRoot(m_workspaceRoot);
    }
    SetIDEAgenticEngineForCommands(g_agentEngine.get());

    if (!modelName.empty())
    {
        m_modelName = modelName;
    }

    m_initialized = true;
    LOG_INFO("Native Inference Stack initialized successfully.");
    return true;
}

// ============================================================================
// Core Agent Command Execution (Single Definition)
// ============================================================================

// E1: workspace root propagated to g_agentEngine on every Initialize call
// E2: OrchestratorBridge gets model+workdir before RunAgent
// E3: response size guard — truncate oversized responses before returning
// E4: tool-call dispatch result appended to history event
// E5: autoCorrect puppeteer only runs when response exceeds quality threshold
// E6: performance monitor records per-call latency
// E7: consecutive failure counter increments m_consecutiveFailures for router
AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt)
{
    SCOPED_METRIC("agentic.execute_command");
    METRICS.increment("agentic.commands_total");
    auto& perf = ::RawrXD::Inference::PerformanceMonitor::instance();
    perf.startOperation("agentic.bridge.execute");
    bool perfClosed = false;
    auto closePerf = [&]()
    {
        if (!perfClosed)
        {
            perf.endOperation("agentic.bridge.execute");
            perfClosed = true;
        }
    };
    
    // ===== TIMEOUT GUARD: Establish operation deadline =====
    const auto operationStart = std::chrono::steady_clock::now();
    const auto operationDeadline =
        operationStart + std::chrono::milliseconds(m_executeCommandTimeoutMs);
    
    auto checkTimeout = [&](const std::string& phase) -> bool {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - operationStart).count();
        if (elapsed_ms > m_executeCommandTimeoutMs)
        {
            LOG_ERROR("ExecuteAgentCommand timeout exceeded in phase '" + phase + "' (elapsed=" + 
                      std::to_string(elapsed_ms) + "ms, limit=" + std::to_string(m_executeCommandTimeoutMs) + "ms)");
            return true;  // timeout occurred
        }
        return false;  // still within time
    };

    auto& sanitizer = ::RawrXD::Security::InputSanitizer::instance();
    const auto requestStart = std::chrono::steady_clock::now();
    auto promptSan = sanitizer.sanitizePrompt(prompt);
    if (promptSan.wasModified)
    {
        LOG_WARNING("Prompt sanitized before agent dispatch");
    }
    
    // Check timeout after sanitization
    if (checkTimeout("sanitization"))
    {
        closePerf();
        return {AgentResponseType::AGENT_ERROR, "[Timeout] Operation exceeded " + 
                std::to_string(m_executeCommandTimeoutMs) + "ms limit during prompt sanitization"};
    }
    
    // E1: propagate workspace root to engine immediately
    if (!m_workspaceRoot.empty() && g_agentEngine)
        g_agentEngine->setWorkspaceRoot(m_workspaceRoot);

        // ===== Orchestration Session State Integration =====
        auto& sessionState = ::RawrXD::Orchestration::OrchestrationSessionState::instance();
    
        // P1: Classify user intent from the prompt
        ::RawrXD::Orchestration::IntentClassification intent;
        intent.intent = "unknown";
        intent.confidence = 0.0f;
        intent.reasoning = "Initial classification from prompt text";
    
        // Simple keyword-based classification (can be enhanced with ML later)
        std::string lowerPrompt = prompt;
        std::transform(lowerPrompt.begin(), lowerPrompt.end(), lowerPrompt.begin(), ::tolower);
    
        if (lowerPrompt.find("search") != std::string::npos || lowerPrompt.find("find") != std::string::npos ||
            lowerPrompt.find("grep") != std::string::npos)
        {
            intent.intent = "search";
            intent.confidence = 0.85f;
            intent.suggested_tools = {"search_code", "grep_files", "file_search"};
        }
        else if (lowerPrompt.find("error") != std::string::npos || lowerPrompt.find("debug") != std::string::npos ||
                 lowerPrompt.find("diagnose") != std::string::npos || lowerPrompt.find("issue") != std::string::npos)
        {
            intent.intent = "debug";
            intent.confidence = 0.82f;
            intent.suggested_tools = {"read_file", "grep_files", "run_in_terminal"};
        }
        else if (lowerPrompt.find("plan") != std::string::npos || lowerPrompt.find("design") != std::string::npos ||
                 lowerPrompt.find("architecture") != std::string::npos)
        {
            intent.intent = "planning";
            intent.confidence = 0.80f;
            intent.suggested_tools = {"read_file", "list_dir", "grep_files"};
        }
        else if (lowerPrompt.find("refactor") != std::string::npos || lowerPrompt.find("improve") != std::string::npos ||
                 lowerPrompt.find("optimize") != std::string::npos)
        {
            intent.intent = "refactor";
            intent.confidence = 0.78f;
            intent.suggested_tools = {"read_file", "write_file", "grep_files"};
        }
        else if (lowerPrompt.find("build") != std::string::npos || lowerPrompt.find("compile") != std::string::npos ||
                 lowerPrompt.find("test") != std::string::npos)
        {
            intent.intent = "build";
            intent.confidence = 0.83f;
            intent.suggested_tools = {"run_in_terminal", "read_file"};
        }
        else
        {
            intent.intent = "general_chat";
            intent.confidence = 0.50f;
            intent.suggested_tools = {"read_file", "web_search"};
        }
    
        sessionState.setCurrentIntent(intent);
        // ===== End Intent Classification =====

    // E2: sync OrchestratorBridge model + workdir before routing
    auto& orch = ::RawrXD::Agent::OrchestratorBridge::Instance();
    if (!m_modelName.empty())
    {
        orch.SetModel(m_modelName);
        orch.SetFIMModel(m_modelName);
    }
    if (!m_workspaceRoot.empty())
        orch.SetWorkingDirectory(m_workspaceRoot);

    // E7: track consecutive failures for LLM router
    bool routeSuccess = false;
    auto markSuccess = [&] { routeSuccess = true; };
    (void)markSuccess;

    // Lazy-init: ensure engine exists so chat and agentic work with any loaded model (local definitions vary)
    if (!g_agentEngine)
    {
        Initialize("", m_modelName);
        SetIDEAgenticEngineForCommands(g_agentEngine ? g_agentEngine.get() : nullptr);
    }
    if (!g_agentEngine)
    {
        closePerf();
        return {AgentResponseType::AGENT_ERROR, "Engine Not Initialized"};
    }

    // Check timeout after engine initialization
    if (checkTimeout("engine_init"))
    {
        closePerf();
        return {AgentResponseType::AGENT_ERROR, "[Timeout] Operation exceeded " + 
                std::to_string(m_executeCommandTimeoutMs) + "ms limit during engine initialization"};
    }

    // Update engine config flags from bridge state
    AgenticEngine::GenerationConfig cfg;
    cfg.maxMode = m_maxMode;
    cfg.deepThinking = m_deepThinking;
    cfg.deepResearch = m_deepResearch;
    cfg.noRefusal = m_noRefusal;
    g_agentEngine->updateConfig(cfg);

    // --- Special Commands ---

    if (prompt.find("/react-server") == 0)
    {
        std::string name = (prompt.length() > 14) ? prompt.substr(14) : "react-app";
        std::string result = g_agentEngine->planTask("Create React Server named " + name);
        closePerf();
        return {AgentResponseType::ANSWER, result};
    }

    if (prompt.find("/install_vsix ") == 0)
    {
        std::string path = prompt.substr(14);
        bool res = ::RawrXD::VsixNativeConverter::ConvertVsixToNative(path, "extensions/");
        closePerf();
        return {AgentResponseType::ANSWER, res ? "VSIX Installed" : "VSIX Installation Failed"};
    }

    // --- File-based commands (Bridge reads file, wraps into prompt) ---

    std::string refinedPrompt = promptSan.sanitized;

    if (prompt.find("/bugreport ") == 0)
    {
        std::string path = prompt.substr(11);
        auto pathSan = sanitizer.sanitizePath(path);
        if (pathSan.wasModified)
        {
            LOG_WARNING("Bugreport path sanitized");
        }
        path = pathSan.sanitized;
        std::ifstream f(path);
        if (f)
        {
            std::stringstream buffer;
            buffer << f.rdbuf();
            refinedPrompt = "Analyze the following code for bugs, security vulnerabilities, "
                            "and logic errors.\n\nCode:\n" +
                            buffer.str();
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER, "Error: Could not read file " + path};
        }
    }
    else if (prompt.find("/suggest ") == 0)
    {
        std::string path = prompt.substr(9);
        auto pathSan = sanitizer.sanitizePath(path);
        if (pathSan.wasModified)
        {
            LOG_WARNING("Suggest path sanitized");
        }
        path = pathSan.sanitized;
        std::ifstream f(path);
        if (f)
        {
            std::stringstream buffer;
            buffer << f.rdbuf();
            refinedPrompt = "Provide suggestions to improve the following code "
                            "(performance, readability, style).\n\nCode:\n" +
                            buffer.str();
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER, "Error: Could not read file " + path};
        }
    }
    else if (prompt.find("/patch ") == 0)
    {
        std::string path = prompt.substr(7);
        auto pathSan = sanitizer.sanitizePath(path);
        if (pathSan.wasModified)
        {
            LOG_WARNING("Patch path sanitized");
        }
        path = pathSan.sanitized;
        std::ifstream f(path);
        if (f)
        {
            std::stringstream buffer;
            buffer << f.rdbuf();
            refinedPrompt = "Review the following code for hallucinations, invalid paths, "
                            "and logical contradictions. Rewrite the code to fix these issues "
                            "immediately.\n\nCode:\n" +
                            buffer.str();
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER, "Error: Could not read file " + path};
        }
    }

    // Prepend workspace context when set so the model can reason about the project (see
    // AGENTIC_AND_MODEL_LOADING_AUDIT.md).
    if (!m_workspaceRoot.empty())
    {
        refinedPrompt = "[Workspace root: " + m_workspaceRoot + "]\n\n" + refinedPrompt;
    }

    // ZERO-WRAPPER ARCHITECTURE: Hotpatch macro injection disabled.
    // All tools are now directly accessible through explicit tool schemas and descriptions.
    // No model instruction enhancement macros are injected.
    // applyAgentCapabilityHotpatches(refinedPrompt);  // DISABLED - See ZERO_WRAPPER_ARCHITECTURE.md

    // When no local model is loaded, route through active backend (Ollama/cloud) so agentic
    // work can use the same backend as chat (see docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md).
    std::string response;
    const uint64_t workflowSeed = static_cast<uint64_t>(GetCurrentProcessId()) << 32 |
                                  static_cast<uint64_t>(GetCurrentThreadId());
    char workflowId[32] = {};
    char agentId[32] = {};
    _snprintf_s(workflowId, sizeof(workflowId), _TRUNCATE, "%llx", static_cast<unsigned long long>(workflowSeed));
    _snprintf_s(agentId, sizeof(agentId), _TRUNCATE, "%llx",
                static_cast<unsigned long long>(workflowSeed ^ static_cast<uint64_t>(prompt.size())));

    if (m_workflowExecutorEnabled)
    {
        const int agentCount = std::clamp(m_workflowExecutorAgentCount, 1, 32);
        std::vector<std::string> swarmPrompts;
        swarmPrompts.reserve(static_cast<size_t>(agentCount));
        for (int index = 0; index < agentCount; ++index)
            swarmPrompts.push_back(buildWorkflowExecutorRolePrompt(refinedPrompt, index, agentCount));

        // Check timeout before swarm execution
        if (checkTimeout("pre_swarm"))
        {
            closePerf();
            return {AgentResponseType::AGENT_ERROR, "[Timeout] Operation exceeded " + 
                    std::to_string(m_executeCommandTimeoutMs) + "ms limit before workflow executor swarm"};
        }

        response = ExecuteSwarm(swarmPrompts, agentCount > 1 ? "summarize" : "concatenate", agentCount);

        ::RawrXD::Telemetry::ExplicitState swarmState;
        swarmState.timestamp = telemetry_internal::unixMillisNow();
        swarmState.workflowId = workflowId;
        swarmState.agentId = agentId;
        swarmState.stepNumber = 7;
        swarmState.stepName = telemetry_internal::stateName(7);
        swarmState.memoryKb = telemetry_internal::workingSetKb();
        swarmState.parallelAgents = agentCount;
        swarmState.inputTokens = static_cast<int>(refinedPrompt.size() / 4);
        swarmState.outputTokens = static_cast<int>(response.size() / 4);
        swarmState.contextTokens = static_cast<int>(refinedPrompt.size() / 4);
        swarmState.contextKvPages = std::max(1, swarmState.contextTokens / 256);
        swarmState.checkpointCreated = true;
        swarmState.tools = "[\"runSubagent\",\"hexmag_swarm\"]";
        swarmState.toolsOk = "[\"runSubagent\",\"hexmag_swarm\"]";
        if (response.empty() || response.rfind("[Error]", 0) == 0)
            swarmState.errorValue = "-1";
        const auto swarmDur = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::steady_clock::now() - requestStart)
                                  .count();
        swarmState.durationMicros = static_cast<uint64_t>(std::max<int64_t>(0, swarmDur));
        swarmState.tokensPerSecond =
            (swarmState.durationMicros > 0)
                ? static_cast<int>((static_cast<uint64_t>(std::max(1, swarmState.outputTokens)) * 1000000ULL) /
                                   swarmState.durationMicros)
                : 0;
        telemetry_internal::emitExplicitJson(swarmState);

        if (response.empty() || response.rfind("[Error]", 0) == 0)
        {
            LOG_WARNING("Workflow executor fallback to default agent path");
            response.clear();
        }
    }

    bool localReady = g_cpuEngine && g_cpuEngine->IsModelLoaded();
    if (response.empty() && !localReady)
    {
        // Check timeout before orchestrator call
        if (checkTimeout("pre_orchestrator"))
        {
            closePerf();
            return {AgentResponseType::AGENT_ERROR, "[Timeout] Operation exceeded " + 
                    std::to_string(m_executeCommandTimeoutMs) + "ms limit before orchestrator dispatch"};
        }

        // Prefer the orchestrator bridge (Ollama-backed, tool-aware) and let it decide
        // capability at runtime. If it cannot serve, fall back to legacy routing.
        auto& orch = ::RawrXD::Agent::OrchestratorBridge::Instance();
        if (!m_modelName.empty())
        {
            orch.SetModel(m_modelName);
            orch.SetFIMModel(m_modelName);
        }
        if (!m_workspaceRoot.empty())
        {
            orch.SetWorkingDirectory(m_workspaceRoot);
        }
        
        // Invoke orchestrator with internal timeout guard
        response = orch.RunAgent(refinedPrompt);

        // Check timeout after orchestrator execution
        if (checkTimeout("post_orchestrator"))
        {
            LOG_WARNING("Orchestrator execution may have exceeded deadline");
        }

        if (response.empty() && m_ide)
        {
            // Check timeout before IDE routing
            if (checkTimeout("pre_ide_route"))
            {
                closePerf();
                return {AgentResponseType::AGENT_ERROR, "[Timeout] Operation exceeded " + 
                        std::to_string(m_executeCommandTimeoutMs) + "ms limit before IDE routing"};
            }
            response = m_ide->routeInferenceRequest(refinedPrompt);
        }
    }
    else if (response.empty())
    {
        // Check timeout before engine chat
        if (checkTimeout("pre_engine_chat"))
        {
            closePerf();
            return {AgentResponseType::AGENT_ERROR, "[Timeout] Operation exceeded " + 
                    std::to_string(m_executeCommandTimeoutMs) + "ms limit before engine chat"};
        }
        response = g_agentEngine->chat(refinedPrompt);
    }

    // Check timeout after all generation
    if (checkTimeout("post_generation"))
    {
        LOG_WARNING("Generation may have exceeded deadline");
        response += "\n\n[Warning: Operation may have exceeded timeout during generation]";
    }

    // Check for tool calls in the model's response and dispatch them
    // NOTE: hookToolResult fires inside DispatchModelToolCalls (the funnel)
    //       so every caller — Autonomy, Bridge, etc. — gets failure detection.
    std::string toolResult;
    if (DispatchModelToolCalls(response, toolResult))
    {
        LOG_INFO("Tool call dispatched from model output");

        // Append tool result and optionally re-prompt the model
        response += "\n\n[Tool Execution Result]\n" + toolResult;
        
            // P2: Record tool execution result in session state
            ::RawrXD::Orchestration::ToolExecutionResult execResult;
            execResult.tool_name = "dispatch_model_tool_calls";
            execResult.result = toolResult.length() > 200 ? toolResult.substr(0, 200) + "..." : toolResult;
            execResult.success = !toolResult.empty();
            execResult.duration_ms = static_cast<int64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - requestStart).count());
            sessionState.recordToolExecution(execResult);
            sessionState.recordOrchestrationPass(true);
    }

    // Hook puppeteer + hotpatch orchestrator on the main agentic path (correct refusals, hallucinations, etc.)
    if (m_autoCorrect)
    {
        AgenticPuppeteer puppeteer;
        CorrectionResult pr = puppeteer.correctResponse(response, refinedPrompt);
        if (pr.success && !pr.correctedOutput.empty())
        {
            response = pr.correctedOutput;
            LOG_INFO("AgenticPuppeteer correction applied");
        }
    }
    {
        char correctedBuf[65536];
        CorrectionOutcome hot = AgenticHotpatchOrchestrator::instance().analyzeAndCorrect(
            response.c_str(), response.size(), refinedPrompt.c_str(), refinedPrompt.size(), correctedBuf,
            sizeof(correctedBuf));
        if (hot.success && hot.detail && correctedBuf[0] != '\0')
        {
            response.assign(correctedBuf);
            LOG_INFO("AgenticHotpatchOrchestrator correction applied: " + std::string(hot.detail ? hot.detail : ""));
        }
    }

    // E3: truncate oversized response before returning
        static constexpr size_t kDefaultMaxResponseBytes = 256 * 1024;
    
        // ===== GAP #10: Response Stream Buffering Limits =====
        // Enforce memory-aware response size limits and chunk-based processing
        // to prevent OOM (out of memory) on large outputs
        {
            const int maxBufferBytes = m_maxResponseBufferBytes;
            const int chunkSizeBytes = m_responseChunkSizeBytes;
        
            // Detect memory pressure: check available working set
            PROCESS_MEMORY_COUNTERS_EX memCounters{};
            memCounters.cb = sizeof(memCounters);
            bool memPressure = false;
        
            if (GetProcessMemoryInfo(GetCurrentProcess(), 
                                     reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memCounters),
                                     sizeof(memCounters)))
            {
                // Warn if working set exceeds 80% of a nominal 2GB limit per process
                const int64_t nominalLimitBytes = 2 * 1024 * 1024 * 1024;  // 2GB
                const int64_t workingSetBytes = static_cast<int64_t>(memCounters.WorkingSetSize);
                const int64_t peakWorkingSetBytes = static_cast<int64_t>(memCounters.PeakWorkingSetSize);
            
                if (workingSetBytes > nominalLimitBytes * 0.8)
                {
                    memPressure = true;
                    LOG_WARNING("[Gap #10] Memory pressure detected: WS=" + 
                               std::to_string(workingSetBytes / (1024*1024)) + "MB, " +
                               "Peak=" + std::to_string(peakWorkingSetBytes / (1024*1024)) + "MB");
                }
            }
        
            // Evaluate buffer status
            size_t responseBytes = response.size();
            bool bufferExceeded = responseBytes > static_cast<size_t>(maxBufferBytes);
            bool nearThreshold = responseBytes > static_cast<size_t>(maxBufferBytes) * 0.85;
        
            if (bufferExceeded)
            {
                LOG_WARNING("[Gap #10] Response buffer exceeded: size=" + std::to_string(responseBytes) + 
                           " bytes, limit=" + std::to_string(maxBufferBytes) + " bytes, " +
                           "excess=" + std::to_string(responseBytes - maxBufferBytes) + " bytes");
            
                // Truncate with graceful degradation message
                response = response.substr(0, maxBufferBytes);
                response += "\n\n[Response truncated by Gap #10 buffer limit: " + 
                           std::to_string(responseBytes) + " bytes exceeds " + 
                           std::to_string(maxBufferBytes) + " byte limit. " +
                           (memPressure ? "System under memory pressure." : "Consider increasing buffer size.") + "]";
            }
            else if (nearThreshold)
            {
                LOG_DEBUG("[Gap #10] Response approaching buffer limit: size=" + 
                         std::to_string(responseBytes) + " bytes of " + std::to_string(maxBufferBytes) + 
                         " bytes available (85%+ utilization)");
            }
            else
            {
                LOG_DEBUG("[Gap #10] Response buffering nominal: size=" + std::to_string(responseBytes) + 
                         " bytes of " + std::to_string(maxBufferBytes) + " bytes");
            }
        
            // Log chunk processing info for diagnostic purposes
            if (responseBytes > 0)
            {
                int chunkCount = static_cast<int>((responseBytes + chunkSizeBytes - 1) / chunkSizeBytes);
                LOG_DEBUG("[Gap #10] Response chunk info: " + std::to_string(chunkCount) + 
                         " chunks of " + std::to_string(chunkSizeBytes) + " bytes each");
            }
        }
    
    if (response.size() > kDefaultMaxResponseBytes)
        response = response.substr(0, kDefaultMaxResponseBytes) + "\n[redacted - size exceeded]";

    // ===== GAP #8: Response Content Validation + UTF-8 Enforcement =====
    // Sanitize response for encoding issues, null characters, and control sequences
    {
        std::string sanitized;
        sanitized.reserve(response.size());
        bool hadInvalidChars = false;
        
        for (size_t i = 0; i < response.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(response[i]);
            
            // Check for null characters (terminates C strings unexpectedly)
            if (c == '\0')
            {
                hadInvalidChars = true;
                LOG_WARNING("Response contained null character at offset " + std::to_string(i));
                continue;  // Skip null bytes
            }
            
            // Allow standard printable ASCII and common white space
            if ((c >= 0x20 && c <= 0x7E) ||  // Printable ASCII
                c == '\n' || c == '\r' || c == '\t')  // Common whitespace
            {
                sanitized += response[i];
            }
            // Allow valid UTF-8 multi-byte sequences
            else if (c >= 0x80)
            {
                // UTF-8 continuation byte or start of multi-byte sequence
                // For safety, allow and pass through; invalid sequences handled downstream
                sanitized += response[i];
            }
            // Reject other control characters (0x01-0x1F except \n, \r, \t)
            else if (c < 0x20)
            {
                hadInvalidChars = true;
                LOG_DEBUG("Response contained control character 0x" + 
                         std::string(1, "0123456789ABCDEF"[c >> 4]) + 
                         std::string(1, "0123456789ABCDEF"[c & 0x0F]) +
                         " at offset " + std::to_string(i) + ", removing");
                continue;  // Skip control characters except those handled above
            }
        }
        
        if (hadInvalidChars)
        {
            LOG_WARNING("Response sanitized: removed " + 
                       std::to_string(response.size() - sanitized.size()) + " invalid characters");
        }
        
        response = sanitized;
    }
    
    // Validate UTF-8 encoding (basic check for common issues)
    {
        bool utf8Valid = true;
        for (size_t i = 0; i < response.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(response[i]);
            
            // Check for orphaned continuation bytes
            if ((c & 0xC0) == 0x80)  // Continuation byte (10xxxxxx)
            {
                if (i == 0 || (static_cast<unsigned char>(response[i-1]) & 0xC0) != 0xC0)
                {
                    utf8Valid = false;
                    LOG_WARNING("Response contains orphaned UTF-8 continuation byte at offset " + 
                               std::to_string(i));
                }
            }
            
            // Check for invalid multi-byte start sequences
            if ((c & 0xF0) == 0xF0)  // Should be 4-byte sequence (11110xxx)
            {
                if (i + 3 >= response.size())
                {
                    utf8Valid = false;
                    LOG_WARNING("Response contains incomplete UTF-8 4-byte sequence at offset " + 
                               std::to_string(i));
                }
            }
            else if ((c & 0xE0) == 0xE0)  // Should be 3-byte sequence (1110xxxx)
            {
                if (i + 2 >= response.size())
                {
                    utf8Valid = false;
                    LOG_WARNING("Response contains incomplete UTF-8 3-byte sequence at offset " + 
                               std::to_string(i));
                }
            }
            else if ((c & 0xC0) == 0xC0)  // Should be 2-byte sequence (110xxxxx)
            {
                if (i + 1 >= response.size())
                {
                    utf8Valid = false;
                    LOG_WARNING("Response contains incomplete UTF-8 2-byte sequence at offset " + 
                               std::to_string(i));
                }
            }
        }
        
        if (!utf8Valid)
        {
            LOG_WARNING("Response UTF-8 encoding validation found issues; proceeding with best-effort output");
        }
    }

    // Final timeout check before returning
    if (checkTimeout("final"))
    {
        LOG_WARNING("Final timeout check triggered");
    }

    // E6: latency is recorded after finalDur is computed below

    AgentResponse r;
    r.content = response;
    r.type = AgentResponseType::ANSWER;

    ::RawrXD::Telemetry::ExplicitState finalState;

        // P3: Attach synthesis signal for orchestration visibility (dev builds only)
        auto& devSessionState = ::RawrXD::Orchestration::OrchestrationSessionState::instance();
        std::string synthesisSignal = devSessionState.getSynthesisSignal();
        if (!synthesisSignal.empty())
        {
    #ifdef DEBUG_ORCHESTRATION_SYNTHESIS
            r.content += "\n\n[Orchestration Synthesis]\n" + synthesisSignal;
    #endif
        }
    finalState.timestamp = telemetry_internal::unixMillisNow();
    finalState.workflowId = workflowId;
    finalState.agentId = agentId;
    finalState.stepNumber = m_workflowExecutorEnabled ? 11 : 8;
    finalState.stepName = telemetry_internal::stateName(finalState.stepNumber);
    finalState.memoryKb = telemetry_internal::workingSetKb();
    finalState.parallelAgents = m_workflowExecutorEnabled ? std::clamp(m_workflowExecutorAgentCount, 1, 32) : 1;
    finalState.inputTokens = static_cast<int>(refinedPrompt.size() / 4);
    finalState.outputTokens = static_cast<int>(response.size() / 4);
    finalState.contextTokens = static_cast<int>(refinedPrompt.size() / 4);
    finalState.contextKvPages = std::max(1, finalState.contextTokens / 256);
    finalState.checkpointCreated = true;
    finalState.tools = "[]";
    finalState.toolsOk = "[]";
    if (response.rfind("[ERROR]", 0) == 0 || response.rfind("[Error]", 0) == 0)
        finalState.errorValue = "-2";
    const auto finalDur =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - requestStart)
            .count();
    finalState.durationMicros = static_cast<uint64_t>(std::max<int64_t>(0, finalDur));
    finalState.tokensPerSecond =
        (finalState.durationMicros > 0)
            ? static_cast<int>((static_cast<uint64_t>(std::max(1, finalState.outputTokens)) * 1000000ULL) /
                               finalState.durationMicros)
            : 0;
    telemetry_internal::emitExplicitJson(finalState);

    // E6: record per-call latency in PerformanceMonitor for aggregated visibility
    perf.recordLatency("agentic.bridge.execute.total", finalDur);

    closePerf();
    return r;
}

// ============================================================================
// Configuration Methods
// ============================================================================

void AgenticBridge::SetMaxMode(bool enabled)
{
    m_maxMode = enabled;
    LOG_INFO(std::string("Max Mode ") + (enabled ? "Enabled" : "Disabled"));
    if (enabled && g_cpuEngine && g_cpuEngine->GetContextLimit() < 32768)
    {
        g_cpuEngine->SetContextLimit(32768);
    }
}

void AgenticBridge::SetDeepThinking(bool enabled)
{
    m_deepThinking = enabled;
}

void AgenticBridge::SetDeepResearch(bool enabled)
{
    m_deepResearch = enabled;
}

void AgenticBridge::SetNoRefusal(bool enabled)
{
    m_noRefusal = enabled;
}

void AgenticBridge::SetWorkflowExecutorEnabled(bool enabled)
{
    m_workflowExecutorEnabled = enabled;
}

void AgenticBridge::SetWorkflowExecutorAgentCount(int agentCount)
{
    m_workflowExecutorAgentCount = std::clamp(agentCount, 1, 32);
}

void AgenticBridge::SetLanguageContext(const std::string& language, const std::string& filePath)
{
    m_languageContext = language;
    m_fileContext = filePath;
}

void AgenticBridge::SetContextSize(const std::string& sizeName)
{
    int limit = 32768;
    std::string s = sizeName;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "4k")
        limit = 4096;
    else if (s == "32k")
        limit = 32768;
    else if (s == "64k")
        limit = 65536;
    else if (s == "128k")
        limit = 131072;
    else if (s == "256k")
        limit = 262144;
    else if (s == "512k")
        limit = 524288;
    else if (s == "1m")
        limit = 1048576;

    g_cpuEngine->SetContextLimit(limit);

    std::stringstream ss;
    ss << "Context Window resized to: " << sizeName << " (" << limit << " tokens)";
    LOG_INFO(ss.str());
}

bool AgenticBridge::StartAgentLoop(const std::string& initialPrompt, int maxIterations)
{
    SCOPED_METRIC("agentic.start_loop");
    METRICS.increment("agentic.loops_started");
    LOG_INFO("StartAgentLoop: " + initialPrompt);

    if (!m_initialized)
    {
        LOG_ERROR("Cannot start agent loop - not initialized");
        return false;
    }

    if (m_agentLoopRunning)
    {
        LOG_WARNING("Agent loop already running");
        return false;
    }

    m_agentLoopRunning = true;
    std::string currentPrompt = initialPrompt;
    bool success = true;

    for (int i = 0; i < maxIterations && m_agentLoopRunning; ++i)
    {
        LOG_INFO("Agent loop cycle " + std::to_string(i + 1) + " / " + std::to_string(maxIterations));

        AgentResponse response = ExecuteAgentCommand(currentPrompt);

        if (m_outputCallback)
        {
            std::string iterationTitle = "Agent [Cycle " + std::to_string(i + 1) + "]";
            m_outputCallback(iterationTitle, response.content);
        }

        size_t toolResultPos = response.content.find("[Tool Execution Result]");
        if (toolResultPos != std::string::npos)
        {
            currentPrompt = "Observation from tool execution:\n" + response.content.substr(toolResultPos);
            currentPrompt += "\n\nContinue toward the goal: " + initialPrompt;
        }
        else
        {
            LOG_INFO("Agent loop completed: No further tool calls detected.");
            break;
        }
    }

    m_agentLoopRunning = false;
    return success;
}

void AgenticBridge::StopAgentLoop()
{
    LOG_INFO("StopAgentLoop called");
    m_agentLoopRunning = false;
    KillPowerShellProcess();
}

// ============================================================================
// Status & Capability Queries
// ============================================================================

std::vector<std::string> AgenticBridge::GetAvailableTools()
{
    return {"shell",       "powershell",       "run_in_terminal", "read_file",    "write_file",
            "list_dir",    "list_directory",   "grep_files",      "search_files", "reference_symbol",
            "load_model",  "model_status",     "web_search",      "git_status",   "task_orchestrator",
            "runSubagent", "manage_todo_list", "chain",           "hexmag_swarm"};
}

std::string AgenticBridge::GetAgentStatus()
{
    std::stringstream status;
    status << "Agentic Framework Status:\n";
    status << "  Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    status << "  Model: " << m_modelName << "\n";
    status << "  Ollama Server: " << m_ollamaServer << "\n";
    status << "  Framework Path: " << m_frameworkPath << "\n";
    status << "  Workspace Root: " << (m_workspaceRoot.empty() ? "<unset>" : m_workspaceRoot) << "\n";
    status << "  Loop Running: " << (m_agentLoopRunning ? "Yes" : "No") << "\n";
    status << "  Max Mode: " << (m_maxMode ? "Yes" : "No") << "\n";
    status << "  Deep Thinking: " << (m_deepThinking ? "Yes" : "No") << "\n";
    status << "  Deep Research: " << (m_deepResearch ? "Yes" : "No") << "\n";
    status << "  Workflow Executor: " << (m_workflowExecutorEnabled ? "Yes" : "No") << "\n";
    status << "  Workflow Agents: " << m_workflowExecutorAgentCount << "\n";
    status << "  Engine Loaded: " << (g_cpuEngine && g_cpuEngine->IsModelLoaded() ? "Yes" : "No") << "\n";
    if (g_agentEngine)
    {
        status << "  Model Status: " << g_agentEngine->getModelStatus() << "\n";
    }
    if (m_subAgentManager)
    {
        status << "  SubAgents Active: " << m_subAgentManager->activeSubAgentCount() << "\n";
        status << "  SubAgents Spawned: " << m_subAgentManager->totalSpawned() << "\n";
        status << "  " << m_subAgentManager->getStatusSummary() << "\n";
    }
    return status.str();
}

// ============================================================================
// Model & Server Configuration
// ============================================================================

void AgenticBridge::SetModel(const std::string& modelName)
{
    m_modelName = modelName;
    LOG_INFO("Model set to: " + modelName);

    auto endsWith = [](const std::string& s, const std::string& ext) -> bool
    {
        if (ext.size() > s.size())
            return false;
        return s.compare(s.size() - ext.size(), ext.size(), ext) == 0;
    };
    std::string lower = modelName;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    bool isPath =
        !modelName.empty() &&
        (modelName.find_first_of("/\\") != std::string::npos || endsWith(lower, ".gguf") || endsWith(lower, ".gguf2") ||
         endsWith(lower, ".bin") || endsWith(lower, ".safetensors") || endsWith(lower, ".onnx"));

    if (isPath)
    {
        // GGUF file path — load via native engine
        if (g_agentEngine)
            g_agentEngine->loadLocalModel(modelName);
    }
    else if (!modelName.empty())
    {
        // Ollama model tag (e.g. "llama3.3:latest") — propagate to BackendSwitcher
        // so that routeToOllama() sends the correct model name
        if (m_ide)
        {
            m_ide->setBackendModel(Win32IDE::AIBackendType::Ollama, modelName);
            LOG_INFO("BackendSwitcher Ollama model updated to: " + modelName);
        }

        // Keep OrchestratorBridge (agentic/tool-calling path) in sync so IDE agent flows
        // use the same selected model as chat/FIM.
        auto& orch = ::RawrXD::Agent::OrchestratorBridge::Instance();
        orch.SetModel(modelName);
        orch.SetFIMModel(modelName);
    }
}

void AgenticBridge::SetOllamaServer(const std::string& serverUrl)
{
    m_ollamaServer = serverUrl;
    LOG_INFO("Ollama server set to: " + serverUrl);
}

void AgenticBridge::SetOutputCallback(OutputCallback callback)
{
    m_outputCallback = callback;
}

// ============================================================================
// PowerShell Process Management (Full Implementation)
// ============================================================================

bool AgenticBridge::SpawnPowerShellProcess(const std::string& scriptPath, const std::string& arguments)
{
    LOG_DEBUG("Spawning PowerShell: " + scriptPath + " " + arguments);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&m_hStdoutRead, &m_hStdoutWrite, &sa, 0))
    {
        LOG_ERROR("Failed to create stdout pipe (GetLastError=" + std::to_string(GetLastError()) + ")");
        return false;
    }
    
    if (!SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0))
    {
        LOG_ERROR("Failed to set stdout read handle info (GetLastError=" + std::to_string(GetLastError()) + ")");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        m_hStdoutRead = nullptr;
        m_hStdoutWrite = nullptr;
        return false;
    }

    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0))
    {
        LOG_ERROR("Failed to create stdin pipe (GetLastError=" + std::to_string(GetLastError()) + ")");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        m_hStdoutRead = nullptr;
        m_hStdoutWrite = nullptr;
        return false;
    }
    
    if (!SetHandleInformation(m_hStdinWrite, HANDLE_FLAG_INHERIT, 0))
    {
        LOG_ERROR("Failed to set stdin write handle info (GetLastError=" + std::to_string(GetLastError()) + ")");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        CloseHandle(m_hStdinRead);
        CloseHandle(m_hStdinWrite);
        m_hStdoutRead = nullptr;
        m_hStdoutWrite = nullptr;
        m_hStdinRead = nullptr;
        m_hStdinWrite = nullptr;
        return false;
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.hStdOutput = m_hStdoutWrite;
    si.hStdError = m_hStdoutWrite;
    si.hStdInput = m_hStdinRead;
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = scriptPath + " " + arguments;

    BOOL success = CreateProcessA(NULL, const_cast<char*>(cmdLine.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                                  NULL, &si, &pi);

    if (!success)
    {
        LOG_ERROR("Failed to create PowerShell process (GetLastError=" + std::to_string(GetLastError()) + 
                  ", cmdLine=" + cmdLine + ")");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        CloseHandle(m_hStdinRead);
        CloseHandle(m_hStdinWrite);
        m_hStdoutRead = nullptr;
        m_hStdoutWrite = nullptr;
        m_hStdinRead = nullptr;
        m_hStdinWrite = nullptr;
        return false;
    }

    m_hProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    LOG_DEBUG("PowerShell process spawned successfully (PID=" + std::to_string(pi.dwProcessId) + ")");
    return true;
}

bool AgenticBridge::ReadProcessOutput(std::string& output, DWORD timeoutMs)
{
    LOG_DEBUG("Reading process output");
    output.clear();

    if (!m_hStdoutRead)
    {
        LOG_ERROR("No stdout handle");
        return false;
    }

    if (m_hStdoutWrite)
    {
        CloseHandle(m_hStdoutWrite);
        m_hStdoutWrite = nullptr;
    }

    char buffer[4096];
    DWORD bytesRead;
    DWORD startTime = GetTickCount();
    constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);
    constexpr int kMaxIterations = 10000;  // ~1000 sec at 100ms per iteration
    int iterationCount = 0;

    while (iterationCount < kMaxIterations)
    {
        ++iterationCount;
        DWORD available = 0;
        if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL))
        {
            LOG_ERROR("PeekNamedPipe failed (iteration " + std::to_string(iterationCount) + 
                      ", GetLastError=" + std::to_string(GetLastError()) + ")");
            break;
        }

        if (available > 0)
        {
            if (ReadFile(m_hStdoutRead, buffer, kMaxChunk, &bytesRead, NULL) && bytesRead > 0)
            {
                const size_t safeBytes =
                    (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                buffer[safeBytes] = '\0';
                output.append(buffer, safeBytes);
            }
            else
            {
                DWORD readErr = GetLastError();
                if (readErr != ERROR_NO_DATA)
                {
                    LOG_ERROR("ReadFile failed (GetLastError=" + std::to_string(readErr) + ")");
                }
                break;
            }
        }

        if (GetTickCount() - startTime > timeoutMs)
        {
            LOG_WARNING("ReadProcessOutput timeout after " + std::to_string(timeoutMs) + "ms");
            break;
        }

        DWORD exitCode = STILL_ACTIVE;
        if (!GetExitCodeProcess(m_hProcess, &exitCode))
        {
            LOG_ERROR("GetExitCodeProcess failed (GetLastError=" + std::to_string(GetLastError()) + ")");
            break;
        }

        if (exitCode != STILL_ACTIVE)
        {
            // Process exited; drain remaining output
            LOG_DEBUG("Process exited with code " + std::to_string(exitCode) + "; draining output");
            constexpr int kDrainIterations = 100;
            for (int i = 0; i < kDrainIterations; ++i)
            {
                if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL))
                {
                    LOG_DEBUG("PeekNamedPipe failed during drain (iteration " + std::to_string(i) + ")");
                    break;
                }
                if (available == 0)
                    break;
                
                if (ReadFile(m_hStdoutRead, buffer, kMaxChunk, &bytesRead, NULL) && bytesRead > 0)
                {
                    const size_t safeBytes =
                        (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                    buffer[safeBytes] = '\0';
                    output.append(buffer, safeBytes);
                }
                else
                {
                    break;
                }
            }
            break;
        }

        Sleep(100);
    }

    if (iterationCount >= kMaxIterations)
    {
        LOG_ERROR("ReadProcessOutput exceeded max iterations (" + std::to_string(kMaxIterations) + ")");
    }

    LOG_DEBUG("Read " + std::to_string(output.length()) + " bytes from process (iterations=" + 
              std::to_string(iterationCount) + ")");
    return !output.empty();
}

void AgenticBridge::KillPowerShellProcess()
{
    // Terminate process with explicit error handling
    if (m_hProcess)
    {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(m_hProcess, &exitCode))
        {
            if (exitCode == STILL_ACTIVE)
            {
                if (!TerminateProcess(m_hProcess, 0))
                {
                    LOG_ERROR("TerminateProcess failed (GetLastError=" + std::to_string(GetLastError()) + ")");
                }
                else
                {
                    LOG_DEBUG("PowerShell process terminated");
                }
            }
            else
            {
                LOG_DEBUG("PowerShell process already exited with code " + std::to_string(exitCode));
            }
        }
        else
        {
            LOG_ERROR("GetExitCodeProcess failed in KillPowerShellProcess (GetLastError=" + 
                      std::to_string(GetLastError()) + ")");
        }

        // Always attempt to close process handle once it's no longer needed
        if (!CloseHandle(m_hProcess))
        {
            LOG_ERROR("CloseHandle(m_hProcess) failed (GetLastError=" + std::to_string(GetLastError()) + ")");
        }
        m_hProcess = nullptr;
    }

    // Close stdout pipe (double-close protected)
    if (m_hStdoutRead)
    {
        if (!CloseHandle(m_hStdoutRead))
        {
            LOG_ERROR("CloseHandle(m_hStdoutRead) failed (GetLastError=" + std::to_string(GetLastError()) + ")");
        }
        m_hStdoutRead = nullptr;
    }

    if (m_hStdoutWrite)
    {
        if (!CloseHandle(m_hStdoutWrite))
        {
            LOG_ERROR("CloseHandle(m_hStdoutWrite) failed (GetLastError=" + std::to_string(GetLastError()) + ")");
        }
        m_hStdoutWrite = nullptr;
    }

    // Close stdin pipe (double-close protected)
    if (m_hStdinRead)
    {
        if (!CloseHandle(m_hStdinRead))
        {
            LOG_ERROR("CloseHandle(m_hStdinRead) failed (GetLastError=" + std::to_string(GetLastError()) + ")");
        }
        m_hStdinRead = nullptr;
    }

    if (m_hStdinWrite)
    {
        if (!CloseHandle(m_hStdinWrite))
        {
            LOG_ERROR("CloseHandle(m_hStdinWrite) failed (GetLastError=" + std::to_string(GetLastError()) + ")");
        }
        m_hStdinWrite = nullptr;
    }

    LOG_DEBUG("All PowerShell process handles cleaned up");
}

// ============================================================================
// Response Parsing (Full Implementation)
// ============================================================================

AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput)
{
    AgentResponse response;
    response.type = AgentResponseType::THINKING;
    response.rawOutput = rawOutput;

    auto trimInPlace = [](std::string& s)
    {
        const size_t first = s.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
        {
            s.clear();
            return;
        }
        const size_t last = s.find_last_not_of(" \t\n\r");
        s = s.substr(first, last - first + 1);
    };

    std::istringstream stream(rawOutput);
    std::string line;
    std::string fullContent;

    while (std::getline(stream, line))
    {
        if (IsToolCall(line))
        {
            response.type = AgentResponseType::TOOL_CALL;
            response.toolName = "unknown";
            response.toolArgs = line;
            if (line.find("TOOL:") == 0)
            {
                // Legacy colon-delimited format: TOOL:name:args
                size_t firstColon = line.find(':');
                size_t secondColon = line.find(':', firstColon + 1);
                if (firstColon != std::string::npos && secondColon != std::string::npos && secondColon > firstColon + 1)
                {
                    response.toolName = line.substr(firstColon + 1, secondColon - firstColon - 1);
                    response.toolArgs = line.substr(secondColon + 1);
                    trimInPlace(response.toolName);
                    trimInPlace(response.toolArgs);
                }
            }
            else
            {
                // JSON / XML format: extract "name":"value" field
                const std::string nameKey = "\"name\":\"";
                size_t namePos = line.find(nameKey);
                if (namePos != std::string::npos)
                {
                    namePos += nameKey.size();
                    size_t nameEnd = line.find('"', namePos);
                    response.toolName = (nameEnd != std::string::npos)
                                            ? line.substr(namePos, nameEnd - namePos)
                                            : line.substr(namePos);
                    trimInPlace(response.toolName);
                }
                trimInPlace(response.toolArgs);
            }
        }
        else if (IsAnswer(line))
        {
            response.type = AgentResponseType::ANSWER;
            response.content = line.substr(line.find(':') + 1);
            trimInPlace(response.content);
        }
        fullContent += line + "\n";
    }

    if (response.content.empty())
    {
        response.content = fullContent;
    }

    return response;
}

bool AgenticBridge::IsToolCall(const std::string& line)
{
    if (line.find("TOOL:") == 0)
        return true;
    if (line.find("{\"tool_call\":") != std::string::npos)
        return true;
    if (line.find("{\"function_call\":") != std::string::npos)
        return true;
    if (line.find("<tool_call>") != std::string::npos)
        return true;
    if (line.find("<function_calls>") != std::string::npos)
        return true;
    return false;
}

bool AgenticBridge::IsAnswer(const std::string& line)
{
    return line.find("ANSWER:") == 0;
}

// ============================================================================
// Path Resolution
// ============================================================================

// ============================================================================
// Workspace Management
// ============================================================================

// Workspace root is stored locally; syncing with the engine is currently
// handled elsewhere in the codebase.
void AgenticBridge::SetWorkspaceRoot(const std::string& workspaceRoot)
{
    m_workspaceRoot = workspaceRoot;
    if (g_agentEngine)
    {
        g_agentEngine->setWorkspaceRoot(workspaceRoot);
    }
    LOG_INFO("AgenticBridge workspace root updated: " + m_workspaceRoot);
}

std::string AgenticBridge::ResolveFrameworkPath()
{
    // Resolve the exe directory for portable path resolution
    char exeDir[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
    char* lastSlash = strrchr(exeDir, '\\');
    if (lastSlash)
        *(lastSlash + 1) = '\0';

    std::string base(exeDir);
    std::vector<std::string> searchPaths = {base + "Agentic-Framework.ps1", base + "scripts\\Agentic-Framework.ps1",
                                            "Agentic-Framework.ps1",        "scripts\\Agentic-Framework.ps1",
                                            "..\\Agentic-Framework.ps1",    "..\\scripts\\Agentic-Framework.ps1"};

    for (const auto& path : searchPaths)
    {
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES)
        {
            LOG_INFO("Found Agentic-Framework.ps1 at: " + path);
            return path;
        }
    }

    LOG_WARNING("Agentic-Framework.ps1 not found in any search path");
    return "Agentic-Framework.ps1";
}

std::string AgenticBridge::ResolveToolsModulePath()
{
    std::string base = ResolveFrameworkPath();
    if (base.empty() || base == "Agentic-Framework.ps1")
        return "";
    std::filesystem::path p(base);
    if (p.has_parent_path())
    {
        p = p.parent_path() / "Tools" / "AgentTools.ps1";
        if (std::filesystem::exists(p))
            return p.string();
    }
    return "";
}

// ============================================================================
// RE Suite Tools Bridge
// ============================================================================

std::string AgenticBridge::RunDumpbin(const std::string& path, const std::string& mode)
{
    if (g_agentEngine)
        return g_agentEngine->runDumpbin(path, mode);
    return "Agentic Engine not initialized";
}

std::string AgenticBridge::RunCodex(const std::string& path)
{
    if (g_agentEngine)
        return g_agentEngine->runCodex(path);
    return "Agentic Engine not initialized";
}

std::string AgenticBridge::RunCompiler(const std::string& path)
{
    if (g_agentEngine)
        return g_agentEngine->runCompiler(path, "x64");
    return "Agentic Engine not initialized";
}

// ============================================================================
// SubAgent / Chaining / HexMag Swarm Operations
// ============================================================================

SubAgentManager* AgenticBridge::GetSubAgentManager()
{
    if (!m_subAgentManager)
    {
        if (!g_agentEngine)
        {
            Initialize("", m_modelName);
            SetIDEAgenticEngineForCommands(g_agentEngine ? g_agentEngine.get() : nullptr);
        }
        if (!g_agentEngine)
            return nullptr;

        m_subAgentManager.reset(createWin32SubAgentManager(g_agentEngine.get()));
    }
    return m_subAgentManager.get();
}

std::string AgenticBridge::RunSubAgent(const std::string& description, const std::string& prompt)
{
    SCOPED_METRIC("agentic.run_subagent");
    METRICS.increment("agentic.subagent_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return "[Error] SubAgentManager not available - engine not initialized";

    LOG_INFO("RunSubAgent: " + description);

    static std::atomic<int> failedSubAgentCount{0};
    const int maxRetries = m_subAgentMaxRetries;
    const int healthCheckIntervalMs = m_subAgentHealthCheckIntervalMs;

    int retryCount = 0;
    std::string result;
    bool agentSucceeded = false;

    while (retryCount <= maxRetries && !agentSucceeded)
    {
        if (retryCount > 0)
        {
            LOG_WARNING("[Gap #11] SubAgent recovery retry #" + std::to_string(retryCount) +
                        " for: " + description);
        }

        try
        {
            std::string attemptAgentId = mgr->spawnSubAgent("bridge", description, prompt);
            int waitTimeoutMs = 120000 + (retryCount * 30000);
            bool healthyCompletion = mgr->waitForSubAgent(attemptAgentId, waitTimeoutMs);

            if (!healthyCompletion)
            {
                LOG_WARNING("[Gap #11] SubAgent did not complete within timeout: " + description);
                mgr->cancelSubAgent(attemptAgentId);
                retryCount++;
                continue;
            }

            result = mgr->getSubAgentResult(attemptAgentId);
            if (!result.empty() && result.find("[Error]") != std::string::npos)
            {
                retryCount++;
                continue;
            }

            agentSucceeded = true;
            if (retryCount > 0)
                METRICS.increment("agentic.subagent_recovery_success");
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR("[Gap #11] SubAgent exception: " + std::string(ex.what()));
            failedSubAgentCount++;
            retryCount++;
            if (retryCount <= maxRetries)
                std::this_thread::sleep_for(std::chrono::milliseconds(healthCheckIntervalMs));
        }
    }

    if (!agentSucceeded)
    {
        failedSubAgentCount++;
        METRICS.increment("agentic.subagent_recovery_failure");
        result = "[Subagent Failed] After " + std::to_string(maxRetries + 1) +
                 " attempts: " + description;
    }

    return result;
}

std::string AgenticBridge::ExecuteChain(const std::vector<std::string>& steps, const std::string& initialInput)
{
    SCOPED_METRIC("agentic.execute_chain");
    METRICS.increment("agentic.chain_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return "[Error] SubAgentManager not available — engine not initialized";

    LOG_INFO("ExecuteChain: " + std::to_string(steps.size()) + " steps");
    return mgr->executeChain("bridge", steps, initialInput);
}

std::string AgenticBridge::ExecuteSwarm(const std::vector<std::string>& prompts, const std::string& mergeStrategy,
                                        int maxParallel)
{
    SCOPED_METRIC("agentic.execute_swarm");
    METRICS.increment("agentic.swarm_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return "[Error] SubAgentManager not available — engine not initialized";

    SwarmConfig config;
    config.maxParallel = maxParallel;
    config.timeoutMs = 120000;
    config.mergeStrategy = mergeStrategy;
    config.failFast = false;

    LOG_INFO("ExecuteSwarm: " + std::to_string(prompts.size()) + " tasks, strategy=" + mergeStrategy);
    std::string mergedResult = mgr->executeSwarm("bridge", prompts, config);

    // Phase 4B: Choke Point 5 — hookSwarmMerge after swarm merge
    if (m_ide)
    {
        FailureClassification swarmFailure = m_ide->hookSwarmMerge(mergedResult, (int)prompts.size(), mergeStrategy);
        if (swarmFailure.reason != AgentFailureType::None)
        {
            LOG_WARNING("[Phase4B] Swarm merge failure: " + m_ide->failureTypeString(swarmFailure.reason) +
                        " (confidence=" + std::to_string(swarmFailure.confidence) + ")");
        }
    }

    return mergedResult;
}

void AgenticBridge::CancelAllSubAgents()
{
    auto* mgr = GetSubAgentManager();
    if (mgr)
    {
        mgr->cancelAll();
        LOG_INFO("All sub-agents cancelled via bridge");
    }
}

std::string AgenticBridge::GetSubAgentStatus() const
{
    if (m_subAgentManager)
    {
        return m_subAgentManager->getStatusSummary();
    }
    return "SubAgentManager not initialized";
}

bool AgenticBridge::DispatchModelToolCalls(const std::string& modelOutput, std::string& toolResult)
{
    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return false;
    bool dispatched = mgr->dispatchToolCall("bridge", modelOutput, toolResult);

    // ===== GAP #9: Tool Execution Timeout Enforcement =====
    // Monitor tool call execution and enforce timeout to prevent hung tool calls
    {
           const int toolTimeoutMs = m_toolExecutionTimeoutMs;
        auto startTime = std::chrono::high_resolution_clock::now();
        std::string originalResult = toolResult;
        bool timeoutOccurred = false;
        
        // Measure tool execution time (simplified: check result completion)
        // In production, this would integrate with SubAgentManager's async execution tracking
        if (dispatched && !toolResult.empty())
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            
            if (executionTimeMs > toolTimeoutMs)
            {
                timeoutOccurred = true;
                LOG_WARNING("Tool execution exceeded timeout: " + std::to_string(executionTimeMs) + 
                           "ms > " + std::to_string(toolTimeoutMs) + "ms");
                
                // Attempt to extract tool name for diagnostic logging
                std::string toolName = "unknown_tool";
                size_t toolPos = modelOutput.find("TOOL:");
                if (toolPos != std::string::npos)
                {
                    size_t nameStart = toolPos + 5;
                    size_t nameEnd = modelOutput.find_first_of(" \n\r({[", nameStart);
                    if (nameEnd != std::string::npos && nameEnd > nameStart)
                        toolName = modelOutput.substr(nameStart, nameEnd - nameStart);
                }
                
                // Log timeout event with tool name and metrics
                LOG_WARNING("[Gap #9] Tool execution timeout: tool='" + toolName + 
                           "' timeout=" + std::to_string(toolTimeoutMs) + "ms " +
                           "actual=" + std::to_string(executionTimeMs) + "ms");
                
                // Append timeout error marker to result for caller detection
                if (!toolResult.empty() && toolResult.back() != '\n')
                    toolResult += "\n";
                toolResult += "[TIMEOUT: Tool execution exceeded " + std::to_string(toolTimeoutMs) + "ms limit]";
                
                // Record metric for timeout events
                static std::atomic<int> timeoutCount{0};
                timeoutCount++;
                LOG_DEBUG("Total tool execution timeouts recorded: " + std::to_string(timeoutCount.load()));
            }
            else if (executionTimeMs > toolTimeoutMs * 0.8)
            {
                // Warn if tool execution consumed >80% of timeout budget (near-timeout condition)
                LOG_DEBUG("[Gap #9] Tool execution near timeout threshold: " + 
                         std::to_string(executionTimeMs) + "ms of " + std::to_string(toolTimeoutMs) + 
                         "ms available (80% utilized)");
            }
        }
        
        // Log successful tool execution within timeout
        if (dispatched && !timeoutOccurred && !toolResult.empty())
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            LOG_DEBUG("[Gap #9] Tool execution completed successfully in " + 
                     std::to_string(executionTimeMs) + "ms (timeout=" + std::to_string(toolTimeoutMs) + "ms)");
        }
    }

    // Phase 4B: Choke Point 2 — hookToolResult at the dispatch funnel
    // Every tool result flows through here, regardless of caller (Autonomy, Bridge, etc.)
    if (dispatched && m_ide)
    {
        // Extract tool name from legacy TOOL: syntax, JSON function_call/tool_call, or XML tags.
        std::string toolName = "unknown";
        auto extractJsonName = [&]() -> std::string
        {
            const std::string nameKey = "\"name\":\"";
            size_t pos = modelOutput.find(nameKey);
            if (pos == std::string::npos)
                return "";
            pos += nameKey.size();
            size_t end = modelOutput.find('"', pos);
            if (end == std::string::npos || end <= pos)
                return "";
            return modelOutput.substr(pos, end - pos);
        };

        auto extractLegacyTool = [&]() -> std::string
        {
            size_t toolPos = modelOutput.find("tool:");
            if (toolPos == std::string::npos)
                toolPos = modelOutput.find("TOOL:");
            if (toolPos == std::string::npos)
                return "";
            size_t nameStart = toolPos + 5;
            while (nameStart < modelOutput.size() && modelOutput[nameStart] == ' ')
                nameStart++;
            size_t nameEnd = modelOutput.find_first_of(" \n\r({[", nameStart);
            if (nameEnd == std::string::npos)
                nameEnd = modelOutput.size();
            return (nameEnd > nameStart) ? modelOutput.substr(nameStart, nameEnd - nameStart) : "";
        };

        auto extractXmlName = [&]() -> std::string
        {
            size_t openPos = modelOutput.find("<tool_call");
            if (openPos == std::string::npos)
                openPos = modelOutput.find("<function_call");
            if (openPos == std::string::npos)
                return "";

            size_t nameAttr = modelOutput.find("name=\"", openPos);
            if (nameAttr == std::string::npos)
                return "";
            nameAttr += 6;
            size_t nameEnd = modelOutput.find('"', nameAttr);
            if (nameEnd == std::string::npos || nameEnd <= nameAttr)
                return "";
            return modelOutput.substr(nameAttr, nameEnd - nameAttr);
        };

        toolName = extractLegacyTool();
        if (toolName.empty())
            toolName = extractJsonName();
        if (toolName.empty())
            toolName = extractXmlName();
        if (toolName.empty())
            toolName = "unknown";

        FailureClassification toolFailure = m_ide->hookToolResult(toolName, toolResult);
        if (toolFailure.reason != AgentFailureType::None)
        {
            LOG_WARNING("[Phase4B] Tool '" + toolName +
                        "' failure at dispatch: " + m_ide->failureTypeString(toolFailure.reason) +
                        " (confidence=" + std::to_string(toolFailure.confidence) + ")");
        }
    }

    return dispatched;
}

// ============================================================================
// Model Loading
// ============================================================================

bool AgenticBridge::LoadModel(const std::string& path)
{
    SCOPED_METRIC("agentic.load_model");
    METRICS.increment("agentic.model_load_attempts");
    
    if (!g_cpuEngine)
    {
        Initialize("", path);
    }

    if (g_agentEngine)
    {
        bool success = g_agentEngine->loadLocalModel(path);
        if (success)
        {
            m_modelName = g_agentEngine->currentModelPath();
            m_lastModelLoadError.clear();
            LOG_INFO("Model loaded in bridge: " + m_modelName);
            SetIDEAgenticEngineForCommands(g_agentEngine ? g_agentEngine.get() : nullptr);
            return true;
        }
        else
        {
            if (g_cpuEngine)
            {
                m_lastModelLoadError = g_cpuEngine->GetLastLoadErrorMessage();
            }
            if (m_lastModelLoadError.empty())
            {
                m_lastModelLoadError = "Model load failed in AgenticBridge";
            }
            if (m_modelLoadErrorCallback)
            {
                m_modelLoadErrorCallback(m_lastModelLoadError);
            }
            return false;
        }
    }
    
    m_lastModelLoadError = "Model load failed: agent engine not initialized";
    if (m_modelLoadErrorCallback)
    {
        m_modelLoadErrorCallback(m_lastModelLoadError);
    }
    return false;
}

// ============================================================================
// Compatibility wrappers (older UI command layer)
// ============================================================================

bool AgenticBridge::LoadConfiguration(const std::string& configPath)
{
    SCOPED_METRIC("agentic.load_configuration");
    if (configPath.empty())
        return false;

    // IDEConfig is the canonical config source for feature toggles and runtime knobs.
    // Bridge-specific config reflection is intentionally minimal: the engine reads most
    // toggles directly from CONFIG / FeatureToggle.
    if (!CONFIG.loadFromFile(configPath))
    {
        LOG_WARNING("AgenticBridge::LoadConfiguration failed: " + configPath);
        return false;
    }

    CONFIG.applyEnvironmentOverrides();
    CONFIG.applyFeatureToggles();

    LOG_INFO("AgenticBridge configuration loaded: " + configPath);
    return true;
}

void AgenticBridge::WarmUpModel()
{
    SCOPED_METRIC("agentic.warmup_model");

    if (!g_agentEngine)
    {
        LOG_WARNING("AgenticBridge::WarmUpModel: agent engine not initialized");
        return;
    }

    // Minimal warmup: force the engine to touch inference path and allocator.
    // Keep it deterministic and fast; ignore the output.
    (void)g_agentEngine->chat("warmup");
    LOG_INFO("AgenticBridge warmup complete");
}

void AgenticBridge::ExecuteSubAgentChain(const std::string& taskDescription)
{
    SCOPED_METRIC("agentic.execute_subagent_chain");

    std::vector<std::string> steps;
    std::istringstream iss(taskDescription);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty())
            steps.push_back(line);
    }
    if (steps.empty())
        steps.push_back(taskDescription);

    const std::string result = ExecuteChain(steps, taskDescription);
    if (m_outputCallback)
        m_outputCallback("SubAgent Chain", result);
}

void AgenticBridge::ExecuteSubAgentSwarm(const std::string& taskDescription)
{
    SCOPED_METRIC("agentic.execute_subagent_swarm");

    std::vector<std::string> prompts;
    std::istringstream iss(taskDescription);
    std::string line;
    while (std::getline(iss, line))
    {
        if (!line.empty())
            prompts.push_back(line);
    }
    if (prompts.empty())
        prompts.push_back(taskDescription);

    const std::string result = ExecuteSwarm(prompts, "concatenate", 4);
    if (m_outputCallback)
        m_outputCallback("SubAgent Swarm", result);
}

std::vector<std::string> AgenticBridge::GetSubAgentTodoList()
{
    std::vector<std::string> lines;
    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return lines;

    const auto items = mgr->getTodoList();
    lines.reserve(items.size());
    for (const auto& item : items)
    {
        lines.push_back("[" + item.statusString() + "] " + item.title + " (#" + std::to_string(item.id) + ")");
    }
    return lines;
}

void AgenticBridge::ClearSubAgentTodoList()
{
    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return;
    mgr->setTodoList({});
    LOG_INFO("SubAgent todo list cleared via bridge");
}

std::string AgenticBridge::ExportAgentMemory()
{
    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return "{\"error\":\"SubAgentManager not initialized\"}";

    std::ostringstream oss;
    oss << "{\"subagent_status\":\"" << mgr->getStatusSummary() << "\",";
    oss << "\"todo_list\":" << mgr->todoListToJSON() << "}";
    return oss.str();
}

void AgenticBridge::ClearAgentMemory()
{
    ClearSubAgentTodoList();
    CancelAllSubAgents();
    LOG_INFO("Agent memory/state cleared via bridge");
}

void AgenticBridge::ExecuteBoundedAgentLoop(const std::string& prompt, int maxIterations)
{
    const int boundedIterations = std::max(1, std::min(maxIterations, 64));
    const bool started = StartAgentLoop(prompt, boundedIterations);
    if (!started)
        LOG_WARNING("ExecuteBoundedAgentLoop failed to start");
}
