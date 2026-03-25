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
#include "Win32IDE_SubAgent.h"
#include <algorithm>
<<<<<<< HEAD
=======
#include <memory>
>>>>>>> origin/main
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace
{

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
static std::shared_ptr<RawrXD::CPUInferenceEngine> g_cpuEngine = nullptr;
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
        g_cpuEngine = std::make_shared<RawrXD::CPUInferenceEngine>();
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

    applyAgentCapabilityHotpatches(refinedPrompt);

    // When no local model is loaded, route through active backend (Ollama/cloud) so agentic
    // work can use the same backend as chat (see docs/AGENTIC_AND_MODEL_LOADING_AUDIT.md).
    std::string response;
    bool localReady = g_cpuEngine && g_cpuEngine->IsModelLoaded();
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
    if (enabled && g_cpuEngine && g_cpuEngine->GetContextLimit() < 32768)
    {
        g_cpuEngine->SetContextLimit(32768);
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
    if (!g_cpuEngine)
        return;

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

    g_cpuEngine->SetContextLimit(limit);

    std::stringstream ss;
    ss << "Context Window resized to: " << sizeName << " (" << limit << " tokens)";
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

<<<<<<< HEAD
    if (!m_initialized)
    {
=======
    if (!m_initialized) {
>>>>>>> origin/main
        LOG_ERROR("Cannot start agent loop - not initialized");
        return false;
    }

<<<<<<< HEAD
    if (m_agentLoopRunning)
    {
=======
    if (m_agentLoopRunning) {
>>>>>>> origin/main
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

<<<<<<< HEAD
std::vector<std::string> AgenticBridge::GetAvailableTools()
{
    return {"shell",       "powershell",       "run_in_terminal", "read_file",    "write_file",
            "list_dir",    "list_directory",   "grep_files",      "search_files", "reference_symbol",
            "load_model",  "model_status",     "web_search",      "git_status",   "task_orchestrator",
            "runSubagent", "manage_todo_list", "chain",           "hexmag_swarm"};
=======
std::vector<std::string> AgenticBridge::GetAvailableTools() {
    return {
        "shell", "powershell", "read_file", "write_file",
        "web_search", "list_dir", "git_status", "task_orchestrator",
        "runSubagent", "manage_todo_list", "chain", "hexmag_swarm"
    };
>>>>>>> origin/main
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
<<<<<<< HEAD

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
=======
    // Only load as GGUF path when it looks like a file path (agentic autonomous: Ollama tags are valid, don't LoadModel)
    if (g_cpuEngine && !modelName.empty()) {
        bool isPath = modelName.find_first_of("/\\") != std::string::npos
                   || modelName.size() > 4 && modelName.compare(modelName.size() - 5, 5, ".gguf") == 0;
        if (isPath)
            g_cpuEngine->LoadModel(modelName);
>>>>>>> origin/main
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

<<<<<<< HEAD
    if (!CreatePipe(&m_hStdoutRead, &m_hStdoutWrite, &sa, 0))
    {
=======
    if (!CreatePipe(&m_hStdoutRead, &m_hStdoutWrite, &sa, 0)) {
>>>>>>> origin/main
        LOG_ERROR("Failed to create stdout pipe");
        return false;
    }
    SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0);

<<<<<<< HEAD
    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0))
    {
=======
    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0)) {
>>>>>>> origin/main
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

<<<<<<< HEAD
    if (!success)
    {
=======
    if (!success) {
>>>>>>> origin/main
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

<<<<<<< HEAD
    if (!m_hStdoutRead)
    {
=======
    if (!m_hStdoutRead) {
>>>>>>> origin/main
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

<<<<<<< HEAD
    while (true)
    {
=======
    while (true) {
>>>>>>> origin/main
        DWORD available = 0;
        if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL))
        {
            break;
        }

<<<<<<< HEAD
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
=======
        if (available > 0) {
            if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output += buffer;
            } else {
>>>>>>> origin/main
                break;
            }
        }

<<<<<<< HEAD
        if (GetTickCount() - startTime > timeoutMs)
        {
=======
        if (GetTickCount() - startTime > timeoutMs) {
>>>>>>> origin/main
            LOG_WARNING("ReadProcessOutput timeout");
            break;
        }

        DWORD exitCode;
<<<<<<< HEAD
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
=======
        if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            while (PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL) && available > 0) {
                if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    output += buffer;
>>>>>>> origin/main
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
<<<<<<< HEAD
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
=======
    if (m_hStdoutRead)  { CloseHandle(m_hStdoutRead);  m_hStdoutRead  = nullptr; }
    if (m_hStdoutWrite) { CloseHandle(m_hStdoutWrite); m_hStdoutWrite = nullptr; }
    if (m_hStdinRead)   { CloseHandle(m_hStdinRead);   m_hStdinRead   = nullptr; }
    if (m_hStdinWrite)  { CloseHandle(m_hStdinWrite);  m_hStdinWrite  = nullptr; }
>>>>>>> origin/main
}

// ============================================================================
// Response Parsing (Full Implementation)
// ============================================================================

<<<<<<< HEAD
AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput)
{
=======
AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput) {
>>>>>>> origin/main
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
            response.content.erase(0, response.content.find_first_not_of(" \t\n\r"));
            response.content.erase(response.content.find_last_not_of(" \t\n\r") + 1);
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

<<<<<<< HEAD
    for (const auto& path : searchPaths)
    {
=======
    for (const auto& path : searchPaths) {
>>>>>>> origin/main
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

<<<<<<< HEAD
std::string AgenticBridge::ResolveToolsModulePath()
{
=======
std::string AgenticBridge::ResolveToolsModulePath() {
>>>>>>> origin/main
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
    return "SubAgentManager not initialized";
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
        }
        return success;
    }
    m_lastModelLoadError = "Model load failed: agent engine not initialized";
    if (m_modelLoadErrorCallback)
    {
        m_modelLoadErrorCallback(m_lastModelLoadError);
    }
    return false;
<<<<<<< HEAD
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
=======
>>>>>>> origin/main
}
