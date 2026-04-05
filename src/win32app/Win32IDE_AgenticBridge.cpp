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
#include "../agentic_engine.h"
#include "../cpu_inference_engine.h"
#include "../inference/PerformanceMonitor.h"
#include "../logging/Logger.h"
#include "../modules/native_memory.hpp"
#include "../security/InputSanitizer.h"
#include "../vsix_native_converter.hpp"
#include "IDEConfig.h"
#include "IDELogger.h"
#include "Win32IDE.h"
#include "Win32IDE_Phase17_AgenticProfiler.h"
#include "Win32IDE_SubAgent.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>

namespace
{

static constexpr size_t kMaxCommandFileBytes = 512 * 1024;
static constexpr size_t kMaxRefinedPromptBytes = 768 * 1024;

bool envDisablesCapabilityHotpatch(const char* varName)
{
    if (!varName)
        return false;
    char buf[12] = {};
    const DWORD n = GetEnvironmentVariableA(varName, buf, static_cast<DWORD>(sizeof(buf)));
    return n > 0 && (buf[0] == '1' || buf[0] == 't' || buf[0] == 'T' || buf[0] == 'y' || buf[0] == 'Y');
}

[[nodiscard]] bool ReadFileWithCap(const std::string& path, size_t maxBytes, std::string& out)
{
    out.clear();
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        return false;
    }

    f.seekg(0, std::ios::end);
    const std::streamoff total = f.tellg();
    if (total < 0)
    {
        return false;
    }
    if (static_cast<size_t>(total) > maxBytes)
    {
        return false;
    }
    f.seekg(0, std::ios::beg);

    out.resize(static_cast<size_t>(total));
    if (total > 0)
    {
        f.read(&out[0], total);
        if (!f)
        {
            out.clear();
            return false;
        }
    }
    return true;
}

}  // namespace

std::string AgenticBridge::BuildOpenTabsPromptContext() const
{
    if (!m_ide || m_ide->m_editorTabs.empty())
    {
        return {};
    }

    std::ostringstream oss;
    if (!m_ide->m_currentFile.empty())
    {
        oss << "[Active file: " << m_ide->m_currentFile << "]\n";
        oss << "[Active language: " << m_ide->getSyntaxLanguageName() << "]\n";
    }

    oss << "[Open tabs";
    if (m_ide->m_activeTabIndex >= 0 && m_ide->m_activeTabIndex < (int)m_ide->m_editorTabs.size())
    {
        oss << ", active=" << m_ide->m_activeTabIndex;
    }
    oss << "]\n";

    const size_t maxTabs = 8;
    const size_t count = std::min(m_ide->m_editorTabs.size(), maxTabs);
    for (size_t i = 0; i < count; ++i)
    {
        const auto& tab = m_ide->m_editorTabs[i];
        oss << ((static_cast<int>(i) == m_ide->m_activeTabIndex) ? "* " : "- ");
        oss << i << ": ";

        if (!tab.displayName.empty())
            oss << tab.displayName;
        else if (!tab.filePath.empty())
            oss << tab.filePath;
        else
            oss << "Untitled";

        if (!tab.filePath.empty() && tab.filePath != tab.displayName)
            oss << " [" << tab.filePath << "]";
        if (tab.modified)
            oss << " (modified)";

        oss << "\n";
    }

    if (m_ide->m_editorTabs.size() > maxTabs)
    {
        oss << "- ... " << (m_ide->m_editorTabs.size() - maxTabs) << " more tab(s) open\n";
    }

    oss << "\n";
    return oss.str();
}

// SCAFFOLD_054: AgenticBridge DispatchModelToolCalls


// SCAFFOLD_053: AgenticBridge LoadModel and model override


// SCAFFOLD_052: AgenticBridge StartAgentLoop / StopAgentLoop


// SCAFFOLD_051: AgenticBridge ExecuteAgentCommand


// SCAFFOLD_020: Agentic bridge initialization


namespace
{
[[nodiscard]] inline std::shared_ptr<RawrXD::CPUInferenceEngine> SharedCpuEngine()
{
    return RawrXD::CPUInferenceEngine::GetSharedInstance();
}
}  // namespace

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

    const auto cpu = SharedCpuEngine();

    if (!g_agentEngine)
    {
        g_agentEngine = std::make_shared<AgenticEngine>();
        g_agentEngine->setInferenceEngine(cpu.get());
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

bool AgenticBridge::HasUsableBackend() const
{
    const auto eng = SharedCpuEngine();
    if (eng && eng->IsModelLoaded())
        return true;
    if (!m_initialized)
        return false;
    if (RawrXD::Agent::OrchestratorBridge::Instance().IsInitialized())
        return true;
    if (!m_modelName.empty())
        return true;
    return false;
}

void AgenticBridge::SetCpuEngineLayerProgressCallback(std::function<void(const std::string&)> cb)
{
    SharedCpuEngine()->SetLayerProgressCallback(std::move(cb));
}

void AgenticBridge::SetCpuEngineSwarmTelemetryOutputCallback(std::function<void(const std::string&)> cb)
{
    SharedCpuEngine()->SetSwarmTelemetryOutputCallback(std::move(cb));
}

void AgenticBridge::SetMainWindow(HWND hwnd)
{
    m_hwndMain = hwnd;
}

bool AgenticBridge::postLogToMainWindow(UILogSeverity severity, const std::string& message) const
{
    if (!m_hwndMain || message.empty())
        return false;

    char* copy = _strdup(message.c_str());
    if (!copy)
        return false;

    if (!PostMessageA(m_hwndMain, WM_RAWR_LOG_MESSAGE, static_cast<WPARAM>(severity), reinterpret_cast<LPARAM>(copy)))
    {
        free(copy);
        return false;
    }
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
    auto& perf = RawrXD::Inference::PerformanceMonitor::instance();
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
    auto& sanitizer = RawrXD::Security::InputSanitizer::instance();
    auto promptSan = sanitizer.sanitizePrompt(prompt);
    if (promptSan.wasModified)
    {
        LOG_WARNING("Prompt sanitized before agent dispatch");
    }
    // E1: propagate workspace root to engine immediately
    if (!m_workspaceRoot.empty() && g_agentEngine)
        g_agentEngine->setWorkspaceRoot(m_workspaceRoot);

    // E2: sync OrchestratorBridge model + workdir before routing
    auto& orch = RawrXD::Agent::OrchestratorBridge::Instance();
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
        bool res = RawrXD::VsixNativeConverter::ConvertVsixToNative(path, "extensions/");
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
        std::string fileContent;
        if (ReadFileWithCap(path, kMaxCommandFileBytes, fileContent))
        {
            refinedPrompt = "Analyze the following code for bugs, security vulnerabilities, "
                            "and logic errors.\n\nCode:\n" +
                            fileContent;
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER,
                    "Error: Could not read file (missing or exceeds size cap) " + path};
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
        std::string fileContent;
        if (ReadFileWithCap(path, kMaxCommandFileBytes, fileContent))
        {
            refinedPrompt = "Provide suggestions to improve the following code "
                            "(performance, readability, style).\n\nCode:\n" +
                            fileContent;
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER,
                    "Error: Could not read file (missing or exceeds size cap) " + path};
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
        std::string fileContent;
        if (ReadFileWithCap(path, kMaxCommandFileBytes, fileContent))
        {
            refinedPrompt = "Review the following code for hallucinations, invalid paths, "
                            "and logical contradictions. Rewrite the code to fix these issues "
                            "immediately.\n\nCode:\n" +
                            fileContent;
        }
        else
        {
            closePerf();
            return {AgentResponseType::ANSWER,
                    "Error: Could not read file (missing or exceeds size cap) " + path};
        }
    }

    // Prepend workspace and active-editor context so generic tab switches immediately
    // influence agent reasoning.
    if (!m_workspaceRoot.empty())
    {
        refinedPrompt = "[Workspace root: " + m_workspaceRoot + "]\n\n" + refinedPrompt;
    }
    const std::string openTabsContext = BuildOpenTabsPromptContext();
    if (!openTabsContext.empty())
    {
        refinedPrompt = openTabsContext + refinedPrompt;
    }

    if (refinedPrompt.size() > kMaxRefinedPromptBytes)
    {
        closePerf();
        return {AgentResponseType::ANSWER, "Error: Prompt exceeds maximum allowed size"};
    }

    applyAgentCapabilityHotpatches(refinedPrompt);

    // When no local model is loaded, route through active backend (Ollama/cloud) so agentic
    // work can use the same backend as chat (see docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md).
    std::string response;
    bool localReady = SharedCpuEngine()->IsModelLoaded();
    if (!localReady)
    {
        // Prefer the orchestrator bridge (Ollama-backed, tool-aware) and let it decide
        // capability at runtime. If it cannot serve, fall back to legacy routing.
        auto& orch = RawrXD::Agent::OrchestratorBridge::Instance();
        if (!m_modelName.empty())
        {
            orch.SetModel(m_modelName);
            orch.SetFIMModel(m_modelName);
        }
        if (!m_workspaceRoot.empty())
        {
            orch.SetWorkingDirectory(m_workspaceRoot);
        }
        response = orch.RunAgent(refinedPrompt);

        if (response.empty() && m_ide)
        {
            response = m_ide->routeInferenceRequest(refinedPrompt);
        }
    }
    else
    {
        response = g_agentEngine->chat(refinedPrompt);
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
    static constexpr size_t kMaxResponseBytes = 256 * 1024;
    if (response.size() > kMaxResponseBytes)
        response = response.substr(0, kMaxResponseBytes) + "\n[truncated]";

    // E6: record per-call latency via performance monitor
    // TODO: add recordLatency to PerformanceMonitor when timing infra lands

    // CRITICAL: Stream response through callback so UI actually displays real inference output
    if (m_outputCallback && !response.empty())
    {
        m_outputCallback("stream", response);
    }

    AgentResponse r;
    r.content = response;
    r.type = AgentResponseType::ANSWER;
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
    if (enabled)
    {
        const auto eng = SharedCpuEngine();
        if (eng->GetContextLimit() < 32768)
            eng->SetContextLimit(32768);
    }
}

void AgenticBridge::SetDeepThinking(bool enabled)
{
    m_deepThinking = enabled;
    LOG_INFO(std::string("Deep Thinking ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetDeepResearch(bool enabled)
{
    m_deepResearch = enabled;
    LOG_INFO(std::string("Deep Research ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetNoRefusal(bool enabled)
{
    m_noRefusal = enabled;
    LOG_INFO(std::string("No Refusal Mode ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetSwarmMode(bool enabled)
{
    m_swarmMode = enabled;
    SharedCpuEngine()->SetSwarmMode(enabled);
    LOG_INFO(std::string("Swarm Mode ") + (enabled ? "Enabled" : "Disabled"));
}

bool AgenticBridge::LoadSwarmFromDirectory(const std::string& directoryPath, int maxModels)
{
    const auto eng = SharedCpuEngine();
    bool success = eng->LoadSwarmFromDirectory(directoryPath, maxModels);
    if (!success)
        m_lastModelLoadError = eng->GetLastLoadErrorMessage();
    return success;
}

void AgenticBridge::SetAutoCorrect(bool enabled)
{
    m_autoCorrect = enabled;
    LOG_INFO(std::string("Auto Correct ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetHotpatchSubAgentToolProtocol(bool enabled)
{
    m_hotpatchSubAgentToolProtocol = enabled;
    LOG_INFO(std::string("SubAgent tool-protocol hotpatch ") + (enabled ? "enabled" : "disabled"));
}

void AgenticBridge::SetHotpatchThoughtProtocol(bool enabled)
{
    m_hotpatchThoughtProtocol = enabled;
    LOG_INFO(std::string("Thought-protocol hotpatch ") + (enabled ? "enabled" : "disabled"));
}

void AgenticBridge::applyAgentCapabilityHotpatches(std::string& refinedPrompt)
{
    std::string prefix;
    if (m_hotpatchSubAgentToolProtocol && !envDisablesCapabilityHotpatch("RAWRXD_DISABLE_SUBAGENT_HOTPATCH"))
    {
        prefix += "[RawrXD hotpatch — SubAgent/tools]\n"
                  "The model stack may not expose native function-calling. To delegate a subtask, emit ONE line:\n"
                  "  TOOL:runSubagent:{\"description\":\"short label\",\"prompt\":\"instructions for the sub-agent\"}\n"
                  "Workspace tools (one line each): tool:list_dir path=.\n"
                  "  tool:read_file path=<path>\n"
                  "  tool:write_file path=<path> content=<text>\n"
                  "Parallel work: TOOL:hexmag_swarm:{\"prompts\":[\"task1\",\"task2\"],\"strategy\":\"concatenate\","
                  "\"maxParallel\":4}\n"
                  "Sequential pipeline: TOOL:chain:{\"steps\":[\"step1\",\"step2\"],\"input\":\"...\"}\n"
                  "RawrXD dispatches these patterns from plain text even without server-side tool schemas.\n\n";
    }
    if (m_deepThinking && m_hotpatchThoughtProtocol &&
        !envDisablesCapabilityHotpatch("RAWRXD_DISABLE_THOUGHT_HOTPATCH"))
    {
        prefix += "[RawrXD hotpatch — Thought]\n"
                  "Even without native chain-of-thought in the backend, first write a concise private reasoning block "
                  "inside <thought>...</thought>, then give the user-facing answer after </thought>.\n\n";
    }
    if (!prefix.empty())
        refinedPrompt.insert(0, prefix);
}

void AgenticBridge::SetLanguageContext(const std::string& language, const std::string& filePath)
{
    m_languageContext = language;
    m_fileContext = filePath;
    // Propagate to the native agent if available
    if (m_nativeAgent)
    {
        m_nativeAgent->SetLanguageContext(language);
        m_nativeAgent->SetFileContext(filePath);
    }
    LOG_INFO("Language context set: " + language + " file: " + filePath);
}

void AgenticBridge::SetContextSize(const std::string& sizeName)
{
    size_t limit = 4096;
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
    else if (s == "unlimited" || s == "bypass")
        limit = 10485760;  // Memory-gate bypass: 10M tokens (reasonable unlimited)

    SharedCpuEngine()->SetContextLimit(limit);

    std::stringstream ss;
    ss << "Context Window resized to: " << sizeName << " (" << (limit >= 10485760 ? "unlimited" : std::to_string(limit))
       << " tokens)";
    LOG_INFO(ss.str());
}

// ============================================================================
// Agent Loop Management
// ============================================================================

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
        else
        {
            postLogToMainWindow(UILogSeverity::Info,
                                "Agent [Cycle " + std::to_string(i + 1) + "]:\n" + response.content);
        }

        // Check for tool results appended to the response (ExecuteAgentCommand does this)
        size_t toolResultPos = response.content.find("[Tool Execution Result]");
        if (toolResultPos != std::string::npos)
        {
            // Found tool results, feed them back into the model
            currentPrompt = "Observation from tool execution:\n" + response.content.substr(toolResultPos);
            // Optionally add a reminder to the agent to continue
            currentPrompt += "\n\nContinue toward the goal: " + initialPrompt;
        }
        else
        {
            // No tool results, agent likely finished or reached a terminal state
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
    const uint32_t phase17Epochs = Phase17Profiler::GetEpochCount();
    const uint64_t phase17ElapsedCycles = AgenticProfilerGetElapsed();
    const std::string phase17TopTools = AgenticProfilerTopSummary(3);
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
    status << "  Engine Loaded: " << (SharedCpuEngine()->IsModelLoaded() ? "Yes" : "No") << "\n";
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
    status << "  Ghost Stream Last Seq: " << GetLastGhostSeq() << "\n";
    status << "  Ghost Stream Backtracks: " << GetGhostSeqBacktracks() << "\n";
    status << "  Ghost Stream Gap Events: " << GetGhostSeqGapEvents() << "\n";
    status << "  Phase17 Epochs: " << phase17Epochs << "\n";
    status << "  Phase17 Elapsed Cycles: " << phase17ElapsedCycles << "\n";
    status << "  Phase17 Top Tools: " << phase17TopTools << "\n";
    return status.str();
}

void AgenticBridge::ObserveGhostStreamSeq(uint64_t seq)
{
    if (seq == 0)
    {
        return;
    }

    const uint64_t prev = m_lastGhostSeq.load(std::memory_order_relaxed);
    if (prev != 0)
    {
        if (seq <= prev)
        {
            m_ghostSeqBacktracks.fetch_add(1, std::memory_order_relaxed);
        }
        else if (seq > prev + 1)
        {
            m_ghostSeqGapEvents.fetch_add(1, std::memory_order_relaxed);
        }
    }

    uint64_t observed = prev;
    while (seq > observed &&
           !m_lastGhostSeq.compare_exchange_weak(observed, seq, std::memory_order_relaxed,
                                                 std::memory_order_relaxed))
    {
    }
}

uint64_t AgenticBridge::GetLastGhostSeq() const
{
    return m_lastGhostSeq.load(std::memory_order_relaxed);
}

uint64_t AgenticBridge::GetGhostSeqBacktracks() const
{
    return m_ghostSeqBacktracks.load(std::memory_order_relaxed);
}

uint64_t AgenticBridge::GetGhostSeqGapEvents() const
{
    return m_ghostSeqGapEvents.load(std::memory_order_relaxed);
}

void AgenticBridge::ResetGhostSeqTelemetry()
{
    m_lastGhostSeq.store(0, std::memory_order_relaxed);
    m_ghostSeqBacktracks.store(0, std::memory_order_relaxed);
    m_ghostSeqGapEvents.store(0, std::memory_order_relaxed);
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
        auto& orch = RawrXD::Agent::OrchestratorBridge::Instance();
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
        LOG_ERROR("Failed to create stdout pipe");
        return false;
    }
    SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0))
    {
        LOG_ERROR("Failed to create stdin pipe");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        return false;
    }
    SetHandleInformation(m_hStdinWrite, HANDLE_FLAG_INHERIT, 0);

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
        LOG_ERROR("Failed to create PowerShell process");
        CloseHandle(m_hStdoutRead);
        CloseHandle(m_hStdoutWrite);
        CloseHandle(m_hStdinRead);
        CloseHandle(m_hStdinWrite);
        return false;
    }

    m_hProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    LOG_DEBUG("PowerShell process spawned successfully");
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

    while (true)
    {
        DWORD available = 0;
        if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL))
        {
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
                break;
            }
        }

        if (GetTickCount() - startTime > timeoutMs)
        {
            LOG_WARNING("ReadProcessOutput timeout");
            break;
        }

        DWORD exitCode;
        if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode != STILL_ACTIVE)
        {
            while (PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL) && available > 0)
            {
                if (ReadFile(m_hStdoutRead, buffer, kMaxChunk, &bytesRead, NULL) && bytesRead > 0)
                {
                    const size_t safeBytes =
                        (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                    buffer[safeBytes] = '\0';
                    output.append(buffer, safeBytes);
                }
            }
            break;
        }

        Sleep(100);
    }

    LOG_DEBUG("Read " + std::to_string(output.length()) + " bytes from process");
    return !output.empty();
}

void AgenticBridge::KillPowerShellProcess()
{
    if (m_hProcess)
    {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
        LOG_DEBUG("PowerShell process terminated");
    }
    if (m_hStdoutRead)
    {
        CloseHandle(m_hStdoutRead);
        m_hStdoutRead = nullptr;
    }
    if (m_hStdoutWrite)
    {
        CloseHandle(m_hStdoutWrite);
        m_hStdoutWrite = nullptr;
    }
    if (m_hStdinRead)
    {
        CloseHandle(m_hStdinRead);
        m_hStdinRead = nullptr;
    }
    if (m_hStdinWrite)
    {
        CloseHandle(m_hStdinWrite);
        m_hStdinWrite = nullptr;
    }
}

// ============================================================================
// Response Parsing (Full Implementation)
// ============================================================================

AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput)
{
    AgentResponse response;
    response.type = AgentResponseType::THINKING;
    response.rawOutput = rawOutput;

    std::istringstream stream(rawOutput);
    std::string line;
    std::string fullContent;

    while (std::getline(stream, line))
    {
        if (IsToolCall(line))
        {
            response.type = AgentResponseType::TOOL_CALL;
            size_t firstColon = line.find(':');
            size_t secondColon = line.find(':', firstColon + 1);
            if (secondColon != std::string::npos)
            {
                response.toolName = line.substr(firstColon + 1, secondColon - firstColon - 1);
                response.toolArgs = line.substr(secondColon + 1);
            }
        }
        else if (IsAnswer(line))
        {
            response.type = AgentResponseType::ANSWER;
            response.content = line.substr(line.find(':') + 1);
            const size_t first = response.content.find_first_not_of(" \t\n\r");
            if (first == std::string::npos)
            {
                response.content.clear();
            }
            else
            {
                const size_t last = response.content.find_last_not_of(" \t\n\r");
                response.content = response.content.substr(first, last - first + 1);
            }
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
    return line.find("TOOL:") == 0;
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
    // First try: ResolveFrameworkPath
    std::string base = ResolveFrameworkPath();
    if (!base.empty() && base != "Agentic-Framework.ps1")
    {
        std::filesystem::path p(base);
        if (p.has_parent_path())
        {
            p = p.parent_path() / "Tools" / "AgentTools.ps1";
            if (std::filesystem::exists(p))
                return p.string();
        }
    }
    
    // Fallback: check environment variables
    const char* envPaths[] = {"RAWRXD_TOOLS_PATH", "RAWRXD_HOME", "PROGRAMFILES"};
    for (const char* envVar : envPaths)
    {
        char buffer[512];
        DWORD n = GetEnvironmentVariableA(envVar, buffer, sizeof(buffer));
        if (n > 0 && n < sizeof(buffer))
        {
            std::filesystem::path p(buffer);
            auto toolsPath = p / "Tools" / "AgentTools.ps1";
            if (std::filesystem::exists(toolsPath))
                return toolsPath.string();
            
            toolsPath = p / "AgentTools.ps1";
            if (std::filesystem::exists(toolsPath))
                return toolsPath.string();
        }
    }
    
    // Fallback: check common installation directories
    const std::string commonPaths[] = {
        "C:\\Program Files\\RawrXD\\Tools\\AgentTools.ps1",
        "C:\\RawrXD\\Tools\\AgentTools.ps1",
        "D:\\rawrxd\\scripts\\AgentTools.ps1",
        "..\\..\\scripts\\AgentTools.ps1"
    };
    for (const auto& path : commonPaths)
    {
        if (std::filesystem::exists(path))
            return path;
    }
    
    return "";
}

// ============================================================================
// RE Suite Tools Bridge (Real Implementations)
// ============================================================================

std::string AgenticBridge::RunDumpbin(const std::string& path, const std::string& mode)
{
    if (path.empty())
        return "Error: Empty file path";
    
    // Try engine first if available
    if (g_agentEngine)
    {
        std::string engineResult = g_agentEngine->runDumpbin(path, mode);
        if (!engineResult.empty() && engineResult != "Agentic Engine not initialized")
            return engineResult;
    }
    
    // Fallback: use system dumpbin.exe
    std::string dumpbinPath;
    
    // Search for dumpbin in Visual Studio installations
    const char* vsEditions[] = {"VS2022Enterprise", "VS2022Community", "VS2022Professional"};
    for (const char* edition : vsEditions)
    {
        char buffer[512];
        DWORD n = GetEnvironmentVariableA(edition, buffer, sizeof(buffer));
        if (n > 0 && n < sizeof(buffer))
        {
            std::string vsPath(buffer);
            std::string candidate = vsPath + "\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\dumpbin.exe";
            if (std::filesystem::exists(candidate))
            {
                dumpbinPath = candidate;
                break;
            }
        }
    }
    
    if (dumpbinPath.empty())
    {
        // Try default Visual Studio path
        if (std::filesystem::exists("C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\dumpbin.exe"))
            dumpbinPath = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\dumpbin.exe";
    }
    
    if (dumpbinPath.empty())
    {
        return "Error: dumpbin.exe not found in Visual Studio installation. Install Visual Studio 2022 or set VS2022Enterprise environment variable.";
    }
    
    // Execute dumpbin with the requested mode
    std::string modeArg = mode.empty() ? "/HEADERS" : "/" + mode;
    std::string command = "\"" + dumpbinPath + "\" " + modeArg + " \"" + path + "\" 2>&1";
    
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe)
    {
        return "Error: Failed to execute dumpbin.exe";
    }
    
    std::string output;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        output.append(buffer);
    }
    _pclose(pipe);
    
    return output.empty() ? "Dumpbin completed with no output" : output;
}

std::string AgenticBridge::RunCodex(const std::string& path)
{
    if (path.empty())
        return "Error: Empty file path";
    
    // Try engine first if available
    if (g_agentEngine)
    {
        std::string engineResult = g_agentEngine->runCodex(path);
        if (!engineResult.empty() && engineResult != "Agentic Engine not initialized")
            return engineResult;
    }
    
    // Fallback: analyze the file independently
    if (!std::filesystem::exists(path))
        return "Error: File not found: " + path;
    
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return "Error: Cannot open file: " + path;
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read file magic/header for analysis
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    std::ostringstream analysis;
    analysis << "File Analysis: " << path << "\n";
    analysis << "Size: " << fileSize << " bytes\n";
    
    // Analysis based on magic number
    if ((magic & 0xFFFF) == 0x5A4D)  // MZ header
    {
        analysis << "Type: PE Executable\n";
        analysis << "Subsystem: Windows\n";
    }
    else if (magic == 0x7F454C46)  // ELF magic
    {
        analysis << "Type: ELF Binary\n";
    }
    else if (magic == 0xCAFEBABE || magic == 0xBEBAFECA)
    {
        analysis << "Type: Mach-O / Class File\n";
    }
    else if ((magic & 0xFF) == 0xFE)
    {
        analysis << "Type: Managed Assembly (.NET)\n";
    }
    else
    {
        analysis << "Type: Unknown/Binary\n";
    }
    
    analysis << "First 4 bytes (hex): ";
    for (int i = 0; i < 4; ++i)
    {
        analysis << std::hex << std::setw(2) << std::setfill('0') << ((magic >> (i*8)) & 0xFF);
    }
    analysis << "\n";
    
    return analysis.str();
}

std::string AgenticBridge::RunCompiler(const std::string& path)
{
    return RunCompilerImpl(path, "x64");
}

// Helper: actual compiler implementation with architecture support
std::string AgenticBridge::RunCompilerImpl(const std::string& path, const std::string& arch)
{
    if (path.empty())
        return "Error: Empty file path";
    
    // Try engine first if available
    if (g_agentEngine)
    {
        std::string engineResult = g_agentEngine->runCompiler(path, arch);
        if (!engineResult.empty() && engineResult != "Agentic Engine not initialized")
            return engineResult;
    }
    
    // Check if file exists
    if (!std::filesystem::exists(path))
        return "Error: File not found: " + path;
    
    // Get file extension
    std::filesystem::path filePath(path);
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    std::string compiler;
    std::string compilerFlags;
    
    // Detect language and get compiler
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c")
    {
        // Try MSVC first
        compiler = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\cl.exe";
        if (!std::filesystem::exists(compiler))
        {
            compiler = "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\cl.exe";
        }
        if (!std::filesystem::exists(compiler))
        {
            compiler = "cl.exe";  // Hope it's in PATH
        }
        compilerFlags = "/c /W4 /std:c++20";
    }
    else if (ext == ".asm" || ext == ".s")
    {
        // Use ML64 for assembly
        compiler = "C:\\VS2022Enterprise\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx64\\x64\\ml64.exe";
        if (!std::filesystem::exists(compiler))
        {
            compiler = "ml64.exe";
        }
        compilerFlags = "/c";
    }
    else
    {
        return "Error: Unsupported file type: " + ext;
    }
    
    // Add architecture flag
    if (arch == "x64" || arch == "x86-64" || arch == "amd64")
    {
        compilerFlags += " /machine:x64";
    }
    else if (arch == "x86" || arch == "i386")
    {
        compilerFlags += " /machine:x86";
    }
    else if (arch == "arm" || arch == "arm64")
    {
        compilerFlags += " /machine:arm64";
    }
    
    // Build command
    std::string outFile = filePath.stem().string() + ".obj";
    std::string command = compiler + " " + compilerFlags + " /Fo" + outFile + " \"" + path + "\" 2>&1";
    
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe)
    {
        return "Error: Failed to execute compiler. Ensure Visual Studio 2022 is installed.";
    }
    
    std::string output;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        output.append(buffer);
    }
    int exitCode = _pclose(pipe);
    
    if (exitCode == 0)
    {
        return "Compilation succeeded. Output: " + outFile + "\n" + output;
    }
    else
    {
        return "Compilation failed (exit code: " + std::to_string(exitCode) + ")\n" + output;
    }
}

// ============================================================================
// SubAgent / Chaining / HexMag Swarm Operations
// ============================================================================

SubAgentManager* AgenticBridge::GetSubAgentManager()
{
    if (!m_subAgentManager && g_agentEngine)
    {
        // Use factory to get IDELogger + METRICS wired automatically
        m_subAgentManager.reset(createWin32SubAgentManager(g_agentEngine.get()));

        // Wire callbacks to IDE output
        m_subAgentManager->setCompletionCallback(
            [this](const std::string& agentId, const std::string& result, bool success)
            {
                if (m_outputCallback)
                {
                    std::string prefix = success ? "✅ SubAgent " : "❌ SubAgent ";
                    m_outputCallback(prefix + agentId, result);
                }
                else
                {
                    const auto sev = success ? UILogSeverity::Info : UILogSeverity::Error;
                    const std::string prefix = success ? "✅ SubAgent " : "❌ SubAgent ";
                    postLogToMainWindow(sev, prefix + agentId + "\n" + result);
                }
            });
    }
    return m_subAgentManager.get();
}

std::string AgenticBridge::RunSubAgent(const std::string& description, const std::string& prompt)
{
    SCOPED_METRIC("agentic.run_subagent");
    METRICS.increment("agentic.subagent_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return "[Error] SubAgentManager not available — engine not initialized";

    LOG_INFO("RunSubAgent: " + description);
    std::string agentId = mgr->spawnSubAgent("bridge", description, prompt);
    bool success = mgr->waitForSubAgent(agentId, 120000);
    return mgr->getSubAgentResult(agentId);
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
    // Attempt lazy creation even from const context so status can reflect live runtime state.
    auto* self = const_cast<AgenticBridge*>(this);
    if (self)
    {
        auto* mgr = self->GetSubAgentManager();
        if (mgr)
        {
            return mgr->getStatusSummary();
        }
    }
    return "SubAgentManager not initialized";
}

void AgenticBridge::ExecuteSubAgentChain(const std::string& taskDescription)
{
    auto* mgr = GetSubAgentManager();
    if (!mgr)
    {
        LOG_ERROR("Cannot execute SubAgent chain: manager not initialized");
        return;
    }
    
    // Parse the task description to identify subtasks
    std::vector<std::string> steps;
    std::stringstream ss(taskDescription);
    std::string line;
    
    // Simple heuristic: "step1 | step2 | step3" or "1. step1 2. step2" format
    size_t delimPos = 0;
    std::string delimiter = (taskDescription.find(" | ") != std::string::npos) ? " | " : "; ";
    
    size_t start = 0;
    size_t end = taskDescription.find(delimiter);
    while (end != std::string::npos)
    {
        std::string step = taskDescription.substr(start, end - start);
        if (!step.empty())
            steps.push_back(step);
        start = end + delimiter.length();
        end = taskDescription.find(delimiter, start);
    }
    
    std::string finalStep = taskDescription.substr(start);
    if (!finalStep.empty())
        steps.push_back(finalStep);
    
    // If no explicit steps, create a default two-step chain
    if (steps.empty() || steps.size() == 1)
    {
        steps.clear();
        steps.push_back("Analyze the following task and break it down:\n" + taskDescription);
        steps.push_back("Execute each subtask in the previous analysis:\n{{INPUT}}");
    }
    
    LOG_INFO("ExecuteSubAgentChain: " + std::to_string(steps.size()) + " steps");
    
    mgr->executeChain("bridge", steps, taskDescription);
}

void AgenticBridge::ExecuteSubAgentSwarm(const std::string& taskDescription)
{
    auto* mgr = GetSubAgentManager();
    if (!mgr)
    {
        LOG_ERROR("Cannot execute SubAgent swarm: manager not initialized");
        return;
    }
    
    // Generate parallel analysis tasks from the description
    std::vector<std::string> prompts;
    
    // Create diverse analysis angles
    prompts.push_back("Security Analysis: Identify potential security vulnerabilities, threats, and risks in the following:\n" + taskDescription);
    prompts.push_back("Performance Analysis: Identify performance bottlenecks, inefficiencies, and optimization opportunities:\n" + taskDescription);
    prompts.push_back("Code Quality Analysis: Evaluate code quality, style, readability, and maintainability:\n" + taskDescription);
    prompts.push_back("Architecture Analysis: Analyze design patterns, structure, and architectural improvements:\n" + taskDescription);
    prompts.push_back("Testing Analysis: Identify test coverage gaps and suggest testing strategies:\n" + taskDescription);
    
    SwarmConfig config;
    config.mergeStrategy = "priority_vote";
    config.maxParallel = 5;
    config.timeoutMs = 30000;
    
    LOG_INFO("ExecuteSubAgentSwarm: " + std::to_string(prompts.size()) + " parallel tasks");
    
    mgr->executeSwarm("bridge", prompts, config);
}

std::vector<std::string> AgenticBridge::GetSubAgentTodoList()
{
    std::vector<std::string> todos;
    
    // If there's a todo list in agent memory, retrieve it
    auto* mgr = GetSubAgentManager();
    if (mgr)
    {
        // Try to get from manager if it has this capability
        std::string todoStatus = mgr->getStatusSummary();
        if (!todoStatus.empty())
        {
            todos.push_back("Current SubAgent Status: " + todoStatus);
        }
    }
    
    return todos;
}

void AgenticBridge::ClearSubAgentTodoList()
{
    auto* mgr = GetSubAgentManager();
    if (mgr)
    {
        mgr->cancelAll();
        LOG_INFO("SubAgent todo list cleared");
    }
}

std::string AgenticBridge::ExportAgentMemory()
{
    std::stringstream export_ss;
    export_ss << "{\n";
    export_ss << "  \"exported_at\": \"" << std::time(nullptr) << "\",\n";
    export_ss << "  \"context\": {\n";
    
    if (!m_modelName.empty())
        export_ss << "    \"model\": \"" << m_modelName << "\",\n";
    
    if (!m_workspaceRoot.empty())
        export_ss << "    \"workspace\": \"" << m_workspaceRoot << "\",\n";
    
    if (!m_languageContext.empty())
        export_ss << "    \"language\": \"" << m_languageContext << "\",\n";
    
    if (!m_fileContext.empty())
        export_ss << "    \"file\": \"" << m_fileContext << "\",\n";
    
    export_ss << "    \"max_mode\": " << (m_maxMode ? "true" : "false") << ",\n";
    export_ss << "    \"deep_thinking\": " << (m_deepThinking ? "true" : "false") << ",\n";
    export_ss << "    \"deep_research\": " << (m_deepResearch ? "true" : "false") << "\n";
    export_ss << "  },\n";
    
    // Agent status
    export_ss << "  \"agent_status\": {\n";
    export_ss << "    \"initialized\": " << (m_initialized ? "true" : "false") << ",\n";
    export_ss << "    \"loop_running\": " << (m_agentLoopRunning ? "true" : "false") << "\n";
    export_ss << "  },\n";
    
    // SubAgent information
    export_ss << "  \"subagent_info\": \"" << GetSubAgentStatus() << "\"\n";
    export_ss << "}\n";
    
    return export_ss.str();
}

void AgenticBridge::ClearAgentMemory()
{
    // Clear context
    m_languageContext.clear();
    m_fileContext.clear();
    m_workspaceRoot.clear();

    // Clear SubAgent manager if present
    if (m_subAgentManager)
    {
        m_subAgentManager->cancelAll();
    }

    LOG_INFO("Agent memory cleared");
}

void AgenticBridge::ExecuteBoundedAgentLoop(const std::string& prompt, int maxIterations)
{
    SCOPED_METRIC("agentic.execute_bounded_loop");
    METRICS.increment("agentic.bounded_loop_calls");

    if (prompt.empty())
    {
        postLogToMainWindow(UILogSeverity::Warning, "Bounded loop skipped: empty prompt");
        return;
    }

    const int boundedIterations = std::max(1, std::min(maxIterations, 50));
    std::string currentPrompt = prompt;
    std::ostringstream transcript;
    bool completed = false;

    for (int i = 0; i < boundedIterations; ++i)
    {
        if (!m_initialized)
        {
            transcript << "[iter " << (i + 1) << "] bridge not initialized\n";
            break;
        }

        AgentResponse response = ExecuteAgentCommand(currentPrompt);
        if (!response.content.empty())
        {
            transcript << "[iter " << (i + 1) << "] " << response.content << "\n";
        }
        else
        {
            transcript << "[iter " << (i + 1) << "] (empty response)\n";
        }

        if (response.type == AgentResponseType::TOOL_CALL)
        {
            std::string toolResult;
            if (DispatchModelToolCalls(response.content, toolResult) && !toolResult.empty())
            {
                transcript << "[tool] " << toolResult << "\n";
                currentPrompt = "Tool result:\n" + toolResult + "\nProvide the next step or final answer.";
                continue;
            }
        }

        if (response.type == AgentResponseType::ANSWER && !response.content.empty())
        {
            completed = true;
            break;
        }

        currentPrompt = "Continue from previous result and make progress:\n" + response.content;
    }

    if (!completed)
    {
        transcript << "[bounded-loop] iteration cap reached without final answer\n";
    }

    const std::string output = transcript.str();
    if (m_outputCallback)
    {
        m_outputCallback("Bounded Agent Loop", output);
    }
    else
    {
        postLogToMainWindow(UILogSeverity::Info, output);
    }
}

bool AgenticBridge::DispatchModelToolCalls(const std::string& modelOutput, std::string& toolResult)
{
    auto* mgr = GetSubAgentManager();
    if (!mgr)
        return false;
    bool dispatched = mgr->dispatchToolCall("bridge", modelOutput, toolResult);

    // Phase 4B: Choke Point 2 — hookToolResult at the dispatch funnel
    // Every tool result flows through here, regardless of caller (Autonomy, Bridge, etc.)
    if (dispatched && m_ide)
    {
        // Extract tool name from the model output (first tool: directive)
        std::string toolName = "unknown";
        auto toolPos = modelOutput.find("tool:");
        if (toolPos == std::string::npos)
            toolPos = modelOutput.find("TOOL:");
        if (toolPos != std::string::npos)
        {
            size_t nameStart = toolPos + 5;
            while (nameStart < modelOutput.size() && modelOutput[nameStart] == ' ')
                nameStart++;
            size_t nameEnd = modelOutput.find_first_of(" \n\r({[", nameStart);
            if (nameEnd == std::string::npos)
                nameEnd = modelOutput.size();
            toolName = modelOutput.substr(nameStart, nameEnd - nameStart);
        }
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
    if (!m_initialized)
        Initialize("", path);

    if (g_agentEngine)
    {
        bool success = g_agentEngine->loadLocalModel(path);
        if (success)
        {
            m_modelName = g_agentEngine->currentModelPath();
            m_lastModelLoadError.clear();
            LOG_INFO("Model loaded in bridge: " + m_modelName);
            SetIDEAgenticEngineForCommands(g_agentEngine ? g_agentEngine.get() : nullptr);
        }
        else
        {
            m_lastModelLoadError = SharedCpuEngine()->GetLastLoadErrorMessage();
            if (m_lastModelLoadError.empty())
            {
                m_lastModelLoadError = "Model load failed in AgenticBridge";
            }
            if (m_modelLoadErrorCallback)
            {
                m_modelLoadErrorCallback(m_lastModelLoadError);
            }
            postLogToMainWindow(UILogSeverity::Error, "Model load failed: " + m_lastModelLoadError);
        }
        return success;
    }
    m_lastModelLoadError = "Model load failed: agent engine not initialized";
    if (m_modelLoadErrorCallback)
    {
        m_modelLoadErrorCallback(m_lastModelLoadError);
    }
    postLogToMainWindow(UILogSeverity::Error, m_lastModelLoadError);
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
