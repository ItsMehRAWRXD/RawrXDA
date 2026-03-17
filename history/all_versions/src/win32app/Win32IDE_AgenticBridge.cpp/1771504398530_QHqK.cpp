// ============================================================================
// Agentic Framework Bridge Implementation (Consolidated - No Duplicates)
// Connects Win32IDE to Native Agentic Framework
// ============================================================================

#include "Win32IDE_AgenticBridge.h"
#include "Win32IDE_SubAgent.h"
#include "IDELogger.h"
#include "IDEConfig.h"
#include "Win32IDE.h"
#include "../advanced_agent_features.hpp"
#include "../agentic_engine.h"
#include "../cpu_inference_engine.h"
#include "../modules/native_memory.hpp"
#include "../vsix_native_converter.hpp"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <memory>
#include <mutex>

// Thread-safe singleton for engine instances (Finding #8 fix)
// Replaces raw file-scope globals with mutex-protected lazy init.
namespace {
struct EngineHolder {
    std::mutex mtx;
    std::shared_ptr<RawrXD::CPUInferenceEngine> cpuEngine;
    std::shared_ptr<AgenticEngine> agentEngine;

    static EngineHolder& instance() {
        static EngineHolder holder;
        return holder;
    }

    std::shared_ptr<RawrXD::CPUInferenceEngine> getCPUEngine() {
        std::lock_guard<std::mutex> lock(mtx);
        if (!cpuEngine) {
            cpuEngine = std::make_shared<RawrXD::CPUInferenceEngine>();
            cpuEngine->SetContextLimit(4096);
        }
        return cpuEngine;
    }

    std::shared_ptr<AgenticEngine> getAgentEngine(RawrXD::CPUInferenceEngine* cpu) {
        std::lock_guard<std::mutex> lock(mtx);
        if (!agentEngine) {
            agentEngine = std::make_shared<AgenticEngine>();
            agentEngine->setInferenceEngine(cpu);
        }
        return agentEngine;
    }
};
} // anonymous namespace
// ============================================================================
// Constructor / Destructor
// ============================================================================

AgenticBridge::AgenticBridge(Win32IDE* ide)
    : m_ide(ide), m_initialized(false), m_agentLoopRunning(false),
      m_hProcess(nullptr), m_hStdoutRead(nullptr), m_hStdoutWrite(nullptr),
      m_hStdinRead(nullptr), m_hStdinWrite(nullptr)
{
}

AgenticBridge::~AgenticBridge() {
    KillPowerShellProcess();
}

// ============================================================================
// Initialization
// ============================================================================

bool AgenticBridge::Initialize(const std::string& frameworkPath, const std::string& modelName) {
    SCOPED_METRIC("agentic.initialize");
    if (m_initialized) return true;

    LOG_INFO("Initializing Native Inference Stack...");

    m_frameworkPath = frameworkPath.empty() ? ResolveFrameworkPath() : frameworkPath;

    auto cpuEngine = EngineHolder::instance().getCPUEngine();
    auto agentEngine = EngineHolder::instance().getAgentEngine(cpuEngine.get());

    if (!modelName.empty()) {
        m_modelName = modelName;
    }

    m_initialized = true;
    LOG_INFO("Native Inference Stack initialized successfully.");
    return true;
}

// ============================================================================
// Core Agent Command Execution (Single Definition)
// ============================================================================

AgentResponse AgenticBridge::ExecuteAgentCommand(const std::string& prompt) {
    SCOPED_METRIC("agentic.execute_command");
    METRICS.increment("agentic.commands_total");
    if (!EngineHolder::instance().agentEngine) return {AgentResponseType::AGENT_ERROR, "Engine Not Initialized"};

    // Update engine config flags from bridge state
    AgenticEngine::GenerationConfig cfg;
    cfg.maxMode = m_maxMode;
    cfg.deepThinking = m_deepThinking;
    cfg.deepResearch = m_deepResearch;
    cfg.noRefusal = m_noRefusal;
    EngineHolder::instance().agentEngine->updateConfig(cfg);

    // --- Special Commands ---

    if (prompt.find("/react-server") == 0) {
        std::string name = (prompt.length() > 14) ? prompt.substr(14) : "react-app";
        std::string result = EngineHolder::instance().agentEngine->planTask("Create React Server named " + name);
        return {AgentResponseType::ANSWER, result};
    }

    if (prompt.find("/install_vsix ") == 0) {
        std::string path = prompt.substr(14);
        bool res = RawrXD::VsixNativeConverter::ConvertVsixToNative(path, "extensions/");
        return {AgentResponseType::ANSWER, res ? "VSIX Installed" : "VSIX Installation Failed"};
    }

    // --- File-based commands (Bridge reads file, wraps into prompt) ---

    std::string refinedPrompt = prompt;

    if (prompt.find("/bugreport ") == 0) {
        std::string path = prompt.substr(11);
        std::ifstream f(path);
        if (f) {
            std::stringstream buffer; buffer << f.rdbuf();
            refinedPrompt = "Analyze the following code for bugs, security vulnerabilities, "
                            "and logic errors.\n\nCode:\n" + buffer.str();
        } else {
            return {AgentResponseType::ANSWER, "Error: Could not read file " + path};
        }
    }
    else if (prompt.find("/suggest ") == 0) {
        std::string path = prompt.substr(9);
        std::ifstream f(path);
        if (f) {
            std::stringstream buffer; buffer << f.rdbuf();
            refinedPrompt = "Provide suggestions to improve the following code "
                            "(performance, readability, style).\n\nCode:\n" + buffer.str();
        } else {
            return {AgentResponseType::ANSWER, "Error: Could not read file " + path};
        }
    }
    else if (prompt.find("/patch ") == 0) {
        std::string path = prompt.substr(7);
        std::ifstream f(path);
        if (f) {
            std::stringstream buffer; buffer << f.rdbuf();
            refinedPrompt = "Review the following code for hallucinations, invalid paths, "
                            "and logical contradictions. Rewrite the code to fix these issues "
                            "immediately.\n\nCode:\n" + buffer.str();
        } else {
            return {AgentResponseType::ANSWER, "Error: Could not read file " + path};
        }
    }

    // Fall through to general chat
    std::string response = EngineHolder::instance().agentEngine->chat(refinedPrompt);

    // Check for tool calls in the model's response and dispatch them
    // NOTE: hookToolResult fires inside DispatchModelToolCalls (the funnel)
    //       so every caller — Autonomy, Bridge, etc. — gets failure detection.
    std::string toolResult;
    if (DispatchModelToolCalls(response, toolResult)) {
        LOG_INFO("Tool call dispatched from model output");

        // Append tool result and optionally re-prompt the model
        response += "\n\n[Tool Execution Result]\n" + toolResult;
    }

    AgentResponse r;
    r.content = response;
    r.type = AgentResponseType::ANSWER;
    return r;
}

// ============================================================================
// Configuration Methods
// ============================================================================

void AgenticBridge::SetMaxMode(bool enabled) {
    m_maxMode = enabled;
    LOG_INFO(std::string("Max Mode ") + (enabled ? "Enabled" : "Disabled"));
    if (enabled && EngineHolder::instance().cpuEngine && EngineHolder::instance().cpuEngine->GetContextLimit() < 32768) {
        EngineHolder::instance().cpuEngine->SetContextLimit(32768);
    }
}

void AgenticBridge::SetDeepThinking(bool enabled) {
    m_deepThinking = enabled;
    LOG_INFO(std::string("Deep Thinking ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetDeepResearch(bool enabled) {
    m_deepResearch = enabled;
    LOG_INFO(std::string("Deep Research ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetNoRefusal(bool enabled) {
    m_noRefusal = enabled;
    LOG_INFO(std::string("No Refusal Mode ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetAutoCorrect(bool enabled) {
    m_autoCorrect = enabled;
    LOG_INFO(std::string("Auto Correct ") + (enabled ? "Enabled" : "Disabled"));
}

void AgenticBridge::SetLanguageContext(const std::string& language, const std::string& filePath) {
    m_languageContext = language;
    m_fileContext = filePath;
    // Propagate to the native agent if available
    if (m_nativeAgent) {
        m_nativeAgent->SetLanguageContext(language);
        m_nativeAgent->SetFileContext(filePath);
    }
    LOG_INFO("Language context set: " + language + " file: " + filePath);
}

void AgenticBridge::SetContextSize(const std::string& sizeName) {
    if (!EngineHolder::instance().cpuEngine) return;

    size_t limit = 4096;
    std::string s = sizeName;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "4k")        limit = 4096;
    else if (s == "32k")  limit = 32768;
    else if (s == "64k")  limit = 65536;
    else if (s == "128k") limit = 131072;
    else if (s == "256k") limit = 262144;
    else if (s == "512k") limit = 524288;
    else if (s == "1m")   limit = 1048576;

    EngineHolder::instance().cpuEngine->SetContextLimit(limit);

    std::stringstream ss;
    ss << "Context Window resized to: " << sizeName << " (" << limit << " tokens)";
    LOG_INFO(ss.str());
}

// ============================================================================
// Agent Loop Management
// ============================================================================

bool AgenticBridge::StartAgentLoop(const std::string& initialPrompt, int maxIterations) {
    SCOPED_METRIC("agentic.start_loop");
    METRICS.increment("agentic.loops_started");
    LOG_INFO("StartAgentLoop: " + initialPrompt);

    if (!m_initialized) {
        LOG_ERROR("Cannot start agent loop - not initialized");
        return false;
    }

    if (m_agentLoopRunning) {
        LOG_WARNING("Agent loop already running");
        return false;
    }

    m_agentLoopRunning = true;

    AgentResponse response = ExecuteAgentCommand(initialPrompt);

    if (m_outputCallback) {
        m_outputCallback("Agent Response", response.content);
        if (!response.rawOutput.empty()) {
            m_outputCallback("Agent Debug", response.rawOutput);
        }
    }

    m_agentLoopRunning = false;
    return true;
}

void AgenticBridge::StopAgentLoop() {
    LOG_INFO("StopAgentLoop called");
    m_agentLoopRunning = false;
    KillPowerShellProcess();
}

// ============================================================================
// Status & Capability Queries
// ============================================================================

std::vector<std::string> AgenticBridge::GetAvailableTools() {
    return {
        "shell", "powershell", "read_file", "write_file",
        "list_dir", "open_file", "terminal_send",
        "web_search", "git_status", "task_orchestrator",
        "runSubagent", "manage_todo_list", "chain", "hexmag_swarm"
    };
}

std::string AgenticBridge::GetAgentStatus() {
    std::stringstream status;
    status << "Agentic Framework Status:\n";
    status << "  Initialized: " << (m_initialized ? "Yes" : "No") << "\n";
    status << "  Model: " << m_modelName << "\n";
    status << "  Ollama Server: " << m_ollamaServer << "\n";
    status << "  Framework Path: " << m_frameworkPath << "\n";
    status << "  Loop Running: " << (m_agentLoopRunning ? "Yes" : "No") << "\n";
    status << "  Max Mode: " << (m_maxMode ? "Yes" : "No") << "\n";
    status << "  Deep Thinking: " << (m_deepThinking ? "Yes" : "No") << "\n";
    status << "  Deep Research: " << (m_deepResearch ? "Yes" : "No") << "\n";
    status << "  Engine Loaded: " << (EngineHolder::instance().cpuEngine && EngineHolder::instance().cpuEngine->IsModelLoaded() ? "Yes" : "No") << "\n";
    if (m_subAgentManager) {
        status << "  SubAgents Active: " << m_subAgentManager->activeSubAgentCount() << "\n";
        status << "  SubAgents Spawned: " << m_subAgentManager->totalSpawned() << "\n";
        status << "  " << m_subAgentManager->getStatusSummary() << "\n";
    }
    return status.str();
}

// ============================================================================
// Model & Server Configuration
// ============================================================================

void AgenticBridge::SetModel(const std::string& modelName) {
    m_modelName = modelName;
    LOG_INFO("Model set to: " + modelName);
    if (EngineHolder::instance().cpuEngine) EngineHolder::instance().cpuEngine->LoadModel(modelName);
}

void AgenticBridge::SetOllamaServer(const std::string& serverUrl) {
    m_ollamaServer = serverUrl;
    LOG_INFO("Ollama server set to: " + serverUrl);
}

void AgenticBridge::SetOutputCallback(OutputCallback callback) {
    m_outputCallback = callback;
}

// ============================================================================
// PowerShell Process Management (Full Implementation)
// ============================================================================

bool AgenticBridge::SpawnPowerShellProcess(const std::string& scriptPath, const std::string& arguments) {
    LOG_DEBUG("Spawning PowerShell: " + scriptPath + " " + arguments);

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&m_hStdoutRead, &m_hStdoutWrite, &sa, 0)) {
        LOG_ERROR("Failed to create stdout pipe");
        return false;
    }
    SetHandleInformation(m_hStdoutRead, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&m_hStdinRead, &m_hStdinWrite, &sa, 0)) {
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

    BOOL success = CreateProcessA(
        NULL,
        const_cast<char*>(cmdLine.c_str()),
        NULL, NULL, TRUE,
        CREATE_NO_WINDOW,
        NULL, NULL,
        &si, &pi
    );

    if (!success) {
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

bool AgenticBridge::ReadProcessOutput(std::string& output, DWORD timeoutMs) {
    LOG_DEBUG("Reading process output");
    output.clear();

    if (!m_hStdoutRead) {
        LOG_ERROR("No stdout handle");
        return false;
    }

    if (m_hStdoutWrite) {
        CloseHandle(m_hStdoutWrite);
        m_hStdoutWrite = nullptr;
    }

    char buffer[4096];
    DWORD bytesRead;
    DWORD startTime = GetTickCount();

    while (true) {
        DWORD available = 0;
        if (!PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL)) {
            break;
        }

        if (available > 0) {
            if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                output += buffer;
            } else {
                break;
            }
        }

        if (GetTickCount() - startTime > timeoutMs) {
            LOG_WARNING("ReadProcessOutput timeout");
            break;
        }

        DWORD exitCode;
        if (GetExitCodeProcess(m_hProcess, &exitCode) && exitCode != STILL_ACTIVE) {
            while (PeekNamedPipe(m_hStdoutRead, NULL, 0, NULL, &available, NULL) && available > 0) {
                if (ReadFile(m_hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                    buffer[bytesRead] = '\0';
                    output += buffer;
                }
            }
            break;
        }

        Sleep(100);
    }

    LOG_DEBUG("Read " + std::to_string(output.length()) + " bytes from process");
    return !output.empty();
}

void AgenticBridge::KillPowerShellProcess() {
    if (m_hProcess) {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;
        LOG_DEBUG("PowerShell process terminated");
    }
    if (m_hStdoutRead)  { CloseHandle(m_hStdoutRead);  m_hStdoutRead  = nullptr; }
    if (m_hStdoutWrite) { CloseHandle(m_hStdoutWrite); m_hStdoutWrite = nullptr; }
    if (m_hStdinRead)   { CloseHandle(m_hStdinRead);   m_hStdinRead   = nullptr; }
    if (m_hStdinWrite)  { CloseHandle(m_hStdinWrite);  m_hStdinWrite  = nullptr; }
}

// ============================================================================
// Response Parsing (Full Implementation)
// ============================================================================

AgentResponse AgenticBridge::ParseAgentResponse(const std::string& rawOutput) {
    AgentResponse response;
    response.type = AgentResponseType::THINKING;
    response.rawOutput = rawOutput;

    std::istringstream stream(rawOutput);
    std::string line;
    std::string fullContent;

    while (std::getline(stream, line)) {
        if (IsToolCall(line)) {
            response.type = AgentResponseType::TOOL_CALL;
            size_t firstColon = line.find(':');
            size_t secondColon = line.find(':', firstColon + 1);
            if (secondColon != std::string::npos) {
                response.toolName = line.substr(firstColon + 1, secondColon - firstColon - 1);
                response.toolArgs = line.substr(secondColon + 1);
            }
        } else if (IsAnswer(line)) {
            response.type = AgentResponseType::ANSWER;
            response.content = line.substr(line.find(':') + 1);
            response.content.erase(0, response.content.find_first_not_of(" \t\n\r"));
            response.content.erase(response.content.find_last_not_of(" \t\n\r") + 1);
        }
        fullContent += line + "\n";
    }

    if (response.content.empty()) {
        response.content = fullContent;
    }

    return response;
}

bool AgenticBridge::IsToolCall(const std::string& line) {
    return line.find("TOOL:") == 0;
}

bool AgenticBridge::IsAnswer(const std::string& line) {
    return line.find("ANSWER:") == 0;
}

// ============================================================================
// Path Resolution
// ============================================================================

std::string AgenticBridge::ResolveFrameworkPath() {
    // Resolve the exe directory for portable path resolution
    char exeDir[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
    char* lastSlash = strrchr(exeDir, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';

    std::string base(exeDir);
    std::vector<std::string> searchPaths = {
        base + "Agentic-Framework.ps1",
        base + "scripts\\Agentic-Framework.ps1",
        "Agentic-Framework.ps1",
        "scripts\\Agentic-Framework.ps1",
        "..\\Agentic-Framework.ps1",
        "..\\scripts\\Agentic-Framework.ps1"
    };

    for (const auto& path : searchPaths) {
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) {
            LOG_INFO("Found Agentic-Framework.ps1 at: " + path);
            return path;
        }
    }

    LOG_WARNING("Agentic-Framework.ps1 not found in any search path");
    return "Agentic-Framework.ps1";
}

std::string AgenticBridge::ResolveToolsModulePath() {
    std::vector<std::string> searchPaths = {
        "tools",
        "modules\\tools",
        "agentic\\tools",
        "scripts\\tools",
        "..\\tools",
        "..\\modules\\tools"
    };

    for (const auto& path : searchPaths) {
        DWORD attr = GetFileAttributesA(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            LOG_INFO("Found tools module at: " + path);
            return path;
        }
    }

    LOG_WARNING("Tools module directory not found in any search path");
    return "tools";
}

// ============================================================================
// RE Suite Tools Bridge
// ============================================================================

std::string AgenticBridge::RunDumpbin(const std::string& path, const std::string& mode) {
    if (EngineHolder::instance().agentEngine) return EngineHolder::instance().agentEngine->runDumpbin(path, mode);
    return "Agentic Engine not initialized";
}

std::string AgenticBridge::RunCodex(const std::string& path) {
    if (EngineHolder::instance().agentEngine) return EngineHolder::instance().agentEngine->runCodex(path);
    return "Agentic Engine not initialized";
}

std::string AgenticBridge::RunCompiler(const std::string& path) {
    if (EngineHolder::instance().agentEngine) return EngineHolder::instance().agentEngine->runCompiler(path, "x64");
    return "Agentic Engine not initialized";
}

// ============================================================================
// SubAgent / Chaining / HexMag Swarm Operations
// ============================================================================

SubAgentManager* AgenticBridge::GetSubAgentManager() {
    if (!m_subAgentManager && EngineHolder::instance().agentEngine) {
        // Use factory to get IDELogger + METRICS wired automatically
        m_subAgentManager.reset(createWin32SubAgentManager(EngineHolder::instance().agentEngine.get()));

        // Wire callbacks to IDE output
        m_subAgentManager->setCompletionCallback(
            [this](const std::string& agentId, const std::string& result, bool success) {
                if (m_outputCallback) {
                    std::string prefix = success ? "✅ SubAgent " : "❌ SubAgent ";
                    m_outputCallback(prefix + agentId, result);
                }
            });

        // Wire IDE tool callbacks so agent can open files and send to terminal
        if (m_ide) {
            m_subAgentManager->setOpenFileCallback(
                [this](const std::string& path) { m_ide->openFile(path); });
            m_subAgentManager->setSendToTerminalCallback(
                [this](const std::string& cmd) { m_ide->sendToAllTerminals(cmd); });
            std::string workspace = m_ide->getExplorerRootPath();
            if (workspace.empty()) workspace = ".";
            m_subAgentManager->setWorkspaceRoot(workspace);
        }
    }
    return m_subAgentManager.get();
}

std::string AgenticBridge::RunSubAgent(const std::string& description, const std::string& prompt) {
    SCOPED_METRIC("agentic.run_subagent");
    METRICS.increment("agentic.subagent_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr) return "[Error] SubAgentManager not available — engine not initialized";

    LOG_INFO("RunSubAgent: " + description);
    std::string agentId = mgr->spawnSubAgent("bridge", description, prompt);
    bool success = mgr->waitForSubAgent(agentId, 120000);
    return mgr->getSubAgentResult(agentId);
}

std::string AgenticBridge::ExecuteChain(const std::vector<std::string>& steps,
                                         const std::string& initialInput) {
    SCOPED_METRIC("agentic.execute_chain");
    METRICS.increment("agentic.chain_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr) return "[Error] SubAgentManager not available — engine not initialized";

    LOG_INFO("ExecuteChain: " + std::to_string(steps.size()) + " steps");
    return mgr->executeChain("bridge", steps, initialInput);
}

std::string AgenticBridge::ExecuteSwarm(const std::vector<std::string>& prompts,
                                         const std::string& mergeStrategy,
                                         int maxParallel) {
    SCOPED_METRIC("agentic.execute_swarm");
    METRICS.increment("agentic.swarm_calls");

    auto* mgr = GetSubAgentManager();
    if (!mgr) return "[Error] SubAgentManager not available — engine not initialized";

    SwarmConfig config;
    config.maxParallel = maxParallel;
    config.timeoutMs = 120000;
    config.mergeStrategy = mergeStrategy;
    config.failFast = false;

    LOG_INFO("ExecuteSwarm: " + std::to_string(prompts.size()) + " tasks, strategy=" + mergeStrategy);
    std::string mergedResult = mgr->executeSwarm("bridge", prompts, config);

    // Phase 4B: Choke Point 5 — hookSwarmMerge after swarm merge
    if (m_ide) {
        FailureClassification swarmFailure = m_ide->hookSwarmMerge(
            mergedResult, (int)prompts.size(), mergeStrategy);
        if (swarmFailure.reason != AgentFailureType::None) {
            LOG_WARNING("[Phase4B] Swarm merge failure: " +
                m_ide->failureTypeString(swarmFailure.reason) +
                " (confidence=" + std::to_string(swarmFailure.confidence) + ")");
        }
    }

    return mergedResult;
}

void AgenticBridge::CancelAllSubAgents() {
    auto* mgr = GetSubAgentManager();
    if (mgr) {
        mgr->cancelAll();
        LOG_INFO("All sub-agents cancelled via bridge");
    }
}

std::string AgenticBridge::GetSubAgentStatus() const {
    if (m_subAgentManager) {
        return m_subAgentManager->getStatusSummary();
    }
    return "SubAgentManager not initialized";
}

bool AgenticBridge::DispatchModelToolCalls(const std::string& modelOutput, std::string& toolResult) {
    auto* mgr = GetSubAgentManager();
    if (!mgr) return false;
    bool dispatched = mgr->dispatchToolCall("bridge", modelOutput, toolResult);

    // Phase 4B: Choke Point 2 — hookToolResult at the dispatch funnel
    // Every tool result flows through here, regardless of caller (Autonomy, Bridge, etc.)
    if (dispatched && m_ide) {
        // Extract tool name from the model output (first tool: directive)
        std::string toolName = "unknown";
        auto toolPos = modelOutput.find("tool:");
        if (toolPos == std::string::npos) toolPos = modelOutput.find("TOOL:");
        if (toolPos != std::string::npos) {
            size_t nameStart = toolPos + 5;
            while (nameStart < modelOutput.size() && modelOutput[nameStart] == ' ') nameStart++;
            size_t nameEnd = modelOutput.find_first_of(" \n\r({[", nameStart);
            if (nameEnd == std::string::npos) nameEnd = modelOutput.size();
            toolName = modelOutput.substr(nameStart, nameEnd - nameStart);
        }
        FailureClassification toolFailure = m_ide->hookToolResult(toolName, toolResult);
        if (toolFailure.reason != AgentFailureType::None) {
            LOG_WARNING("[Phase4B] Tool '" + toolName + "' failure at dispatch: " +
                m_ide->failureTypeString(toolFailure.reason) +
                " (confidence=" + std::to_string(toolFailure.confidence) + ")");
        }
    }

    return dispatched;
}

// ============================================================================
// Model Loading
// ============================================================================

bool AgenticBridge::LoadModel(const std::string& path) {
    SCOPED_METRIC("agentic.load_model");
    METRICS.increment("agentic.model_load_attempts");
    if (!EngineHolder::instance().cpuEngine) {
        Initialize("", path);
    }

    if (EngineHolder::instance().cpuEngine) {
        bool success = EngineHolder::instance().cpuEngine->LoadModel(path);
        if (success) {
            m_modelName = path;
            LOG_INFO("Model loaded in bridge: " + path);
        }
        return success;
    }
    return false;
}
