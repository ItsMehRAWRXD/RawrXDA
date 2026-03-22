// Agent menu implementation for Win32IDE
// Implements all agentic framework menu commands and integrations

#include "../agentic/AgentOllamaClient.h"
#include "../agentic/agentic_orchestrator_integration.hpp"
#include "../core/enterprise_license.h"
#include "IDELogger.h"
#include "ModelConnection.h"
#include "RawrXD_AgentCoordinator.h"
#include "RawrXD_AutonomousAgenticPipeline.h"
#include "Win32IDE.h"
#include "Win32IDE_AgenticBridge.h"
#include "Win32SwarmBridge.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

// Local IDM constants used in switch-case dispatch (defined via #define in Commands.cpp/Win32IDE.cpp)
// IDM_AGENT_AUTONOMOUS_COMMUNICATOR: free slot in 4163–4199 range
#ifndef IDM_AGENT_AUTONOMOUS_COMMUNICATOR
#define IDM_AGENT_AUTONOMOUS_COMMUNICATOR 4163
#endif
// IDM_TELEMETRY_UNIFIED_CORE: free slot in 4163–4199 range
#ifndef IDM_TELEMETRY_UNIFIED_CORE
#define IDM_TELEMETRY_UNIFIED_CORE 4164
#endif

// Forward declarations for free-function handlers defined in their own .cpp files
void HandleAutonomousCommunicator(void* idePtr);
void HandleUnifiedTelemetry(void* idePtr);

// ============================================================================
// SUBAGENT CHAIN / SWARM / TODO HANDLERS (Phase 19B)
// ============================================================================
#if 0
// Implemented elsewhere:
// - SubAgent + agent memory UI: Win32IDE_SubAgent.cpp
// - Autonomy UI: Win32IDE.cpp
// - Bounded agent loop UI: Win32IDE_AgentPanel.cpp
void Win32IDE::onSubAgentChain() {
    LOG_INFO("onSubAgentChain called");
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_agenticBridge) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "SubAgent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Get task description from user
    char taskDesc[1024] = {0};
    if (DialogBoxParamA(m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
        [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
            switch (msg) {
                case WM_INITDIALOG:
                    SetWindowTextA(GetDlgItem(hwnd, 101), "Enter task for SubAgent Chain:");
                    return TRUE;
                case WM_COMMAND:
                    if (LOWORD(wp) == IDOK) {
                        GetDlgItemTextA(hwnd, 102, (char*)lp, 1024);
                        EndDialog(hwnd, IDOK);
                        return TRUE;
                    } else if (LOWORD(wp) == IDCANCEL) {
                        EndDialog(hwnd, IDCANCEL);
                        return TRUE;
                    }
                    break;
            }
            return FALSE;
        }, (LPARAM)taskDesc) != IDOK) {
        return;
    }
    
    if (strlen(taskDesc) == 0) {
        strcpy_s(taskDesc, "Execute modular task sequence");
    }
    
    appendToOutput("🔗 SubAgent Chain initiated: " + std::string(taskDesc) + "\n", "Output", OutputSeverity::Info);
    
    // Execute chain in background
    std::thread([this, taskStr = std::string(taskDesc)]() {
        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
        if (_guard.cancelled) return;
        m_agenticBridge->ExecuteSubAgentChain(taskStr);
    }).detach();
}

void Win32IDE::onSubAgentSwarm() {
    LOG_INFO("onSubAgentSwarm called");
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_agenticBridge) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "SubAgent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Get swarm task from user
    char taskDesc[1024] = {0};
    if (DialogBoxParamA(m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
        [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
            switch (msg) {
                case WM_INITDIALOG:
                    SetWindowTextA(GetDlgItem(hwnd, 101), "Enter task for SubAgent Swarm:");
                    return TRUE;
                case WM_COMMAND:
                    if (LOWORD(wp) == IDOK) {
                        GetDlgItemTextA(hwnd, 102, (char*)lp, 1024);
                        EndDialog(hwnd, IDOK);
                        return TRUE;
                    } else if (LOWORD(wp) == IDCANCEL) {
                        EndDialog(hwnd, IDCANCEL);
                        return TRUE;
                    }
                    break;
            }
            return FALSE;
        }, (LPARAM)taskDesc) != IDOK) {
        return;
    }
    
    if (strlen(taskDesc) == 0) {
        strcpy_s(taskDesc, "Execute parallel task swarm");
    }
    
    appendToOutput("🐝 SubAgent Swarm initiated: " + std::string(taskDesc) + "\n", "Output", OutputSeverity::Info);
    
    // Execute swarm in background
    std::thread([this, taskStr = std::string(taskDesc)]() {
        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
        if (_guard.cancelled) return;
        m_agenticBridge->ExecuteSubAgentSwarm(taskStr);
    }).detach();
}

void Win32IDE::onSubAgentTodoList() {
    LOG_INFO("onSubAgentTodoList called");
    if (!m_agenticBridge) {
        appendToOutput("Agentic Bridge not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }
    
    std::vector<std::string> todoItems = m_agenticBridge->GetSubAgentTodoList();
    
    std::stringstream todoOutput;
    todoOutput << "=== SubAgent Todo List ===\n\n";
    if (todoItems.empty()) {
        todoOutput << "(empty)\n";
    } else {
        for (size_t i = 0; i < todoItems.size(); ++i) {
            todoOutput << (i + 1) << ". " << todoItems[i] << "\n";
        }
    }
    
    appendToOutput(todoOutput.str(), "Output", OutputSeverity::Info);
}

void Win32IDE::onSubAgentTodoClear() {
    LOG_INFO("onSubAgentTodoClear called");
    if (!m_agenticBridge) {
        appendToOutput("Agentic Bridge not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }
    
    if (MessageBoxA(m_hwndMain, "Clear all SubAgent todo items?", "Confirm", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        m_agenticBridge->ClearSubAgentTodoList();
        appendToOutput("🗑️ SubAgent Todo List cleared\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::onSubAgentStatus() {
    LOG_INFO("onSubAgentStatus called");
    if (!m_agenticBridge) {
        appendToOutput("Agentic Bridge not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }
    
    std::string status = m_agenticBridge->GetSubAgentStatus();
    appendToOutput("=== SubAgent Status ===\n" + status + "\n", "Output", OutputSeverity::Info);
}

// ============================================================================
// AGENT MEMORY HANDLERS (Phase 19B)
// ============================================================================
void Win32IDE::onAgentMemoryView() {
    LOG_INFO("onAgentMemoryView called");
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_agenticBridge) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::string memory = m_agenticBridge->ExportAgentMemory();
    std::stringstream memOutput;
    memOutput << "=== Agent Memory Context ===\n\n" << memory << "\n";
    
    appendToOutput(memOutput.str(), "Output", OutputSeverity::Info);
}

void Win32IDE::onAgentMemoryClear() {
    LOG_INFO("onAgentMemoryClear called");
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_agenticBridge) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    if (MessageBoxA(m_hwndMain, "Clear all agent memory? This cannot be undone.", "Confirm Clear", MB_YESNO | MB_ICONWARNING) == IDYES) {
        m_agenticBridge->ClearAgentMemory();
        appendToOutput("🗑️ Agent Memory cleared\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::onAgentMemoryExport() {
    LOG_INFO("onAgentMemoryExport called");
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_agenticBridge) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Prefer the IDE's tracked directory; fall back to deriving from current file.
    std::string exportPath = m_currentDirectory;
    if (exportPath.empty()) {
        exportPath = m_currentFile;
        if (!exportPath.empty()) {
            size_t lastSlash = exportPath.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                exportPath = exportPath.substr(0, lastSlash);
            } else {
                exportPath.clear();
            }
        }
    }
    if (exportPath.empty()) exportPath = ".";
    exportPath += "\\agent_memory_export.json";
    
    std::string memory = m_agenticBridge->ExportAgentMemory();
    
    std::ofstream outFile(exportPath);
    if (outFile.is_open()) {
        outFile << memory;
        outFile.close();
        appendToOutput("✅ Agent Memory exported to: " + exportPath + "\n", "Output", OutputSeverity::Info);
    } else {
        appendToOutput("❌ Failed to export agent memory\n", "Errors", OutputSeverity::Error);
    }
}

// ============================================================================
// AUTONOMY HANDLERS
// ============================================================================
void Win32IDE::onAutonomyToggle() {
    LOG_INFO("onAutonomyToggle called");
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_autonomyManager) {
        initializeAutonomy();
    }
    
    bool isRunning = m_autonomyManager && m_autonomyManager->IsRunning();
    if (m_autonomyManager) {
        if (isRunning) {
            m_autonomyManager->Stop();
            appendToOutput("⏸️ Autonomy toggled OFF\n", "Output", OutputSeverity::Info);
        } else {
            m_autonomyManager->Start();
            appendToOutput("▶️ Autonomy toggled ON\n", "Output", OutputSeverity::Info);
        }
    }
}

void Win32IDE::onAutonomyStart() {
    LOG_INFO("onAutonomyStart called");
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_autonomyManager) {
        initializeAutonomy();
    }
    
    if (m_autonomyManager && !m_autonomyManager->IsRunning()) {
        m_autonomyManager->Start();
        appendToOutput("▶️ Autonomy started\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::onAutonomyStop() {
    LOG_INFO("onAutonomyStop called");
    if (m_autonomyManager && m_autonomyManager->IsRunning()) {
        m_autonomyManager->Stop();
        appendToOutput("⏹️ Autonomy stopped\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::onAutonomySetGoal() {
    LOG_INFO("onAutonomySetGoal called");
    if (!m_autonomyManager) {
        appendToOutput("Autonomy Manager not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }
    
    char goalText[512] = {0};
    if (DialogBoxParamA(m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
        [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
            switch (msg) {
                case WM_INITDIALOG:
                    SetWindowTextA(GetDlgItem(hwnd, 101), "Enter autonomy goal:");
                    return TRUE;
                case WM_COMMAND:
                    if (LOWORD(wp) == IDOK) {
                        GetDlgItemTextA(hwnd, 102, (char*)lp, 512);
                        EndDialog(hwnd, IDOK);
                        return TRUE;
                    } else if (LOWORD(wp) == IDCANCEL) {
                        EndDialog(hwnd, IDCANCEL);
                        return TRUE;
                    }
                    break;
            }
            return FALSE;
        }, (LPARAM)goalText) == IDOK && strlen(goalText) > 0) {
        m_autonomyManager->SetGoal(std::string(goalText));
        appendToOutput("🎯 Autonomy goal set: " + std::string(goalText) + "\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::onAutonomyViewStatus() {
    LOG_INFO("onAutonomyViewStatus called");
    if (!m_autonomyManager) {
        appendToOutput("Autonomy Manager not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }
    
    std::string status = m_autonomyManager->GetStatus();
    appendToOutput("=== Autonomy Status ===\n" + status + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAutonomyViewMemory() {
    LOG_INFO("onAutonomyViewMemory called");
    if (!m_autonomyManager) {
        appendToOutput("Autonomy Manager not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }
    
    std::string memory = m_autonomyManager->ExportMemory();
    appendToOutput("=== Autonomy Memory ===\n" + memory + "\n", "Output", OutputSeverity::Info);
}

// ----------------------------------------------------------------------------
// Autonomous Agentic Pipeline (Task 1: Wire + Build)
// Init: create coordinator, wire buildChatPrompt / routeWithIntelligence / onInferenceToken / appendStreamingToken.
// Trigger: Autonomy menu -> Pipeline: Run once | Start autonomous loop | Stop autonomous loop.
// ----------------------------------------------------------------------------
void Win32IDE::ensureAutonomousPipelineInitialized() {
    if (m_autonomousPipeline)
        return;
    m_autonomousPipeline = std::make_unique<RawrXD::AutonomousAgenticPipelineCoordinator>();
    m_autonomousPipeline->setBuildPrompt([this](const std::string& m) { return buildChatPrompt(m); });
    m_autonomousPipeline->setRouteLLM([this](const std::string& p) { return routeWithIntelligence(p); });
    m_autonomousPipeline->setOnToken([this](const std::string& t, bool) { onInferenceToken(t); });
    m_autonomousPipeline->setAppendRenderer([this](const std::string& s) { appendStreamingToken(s); });
    // Task 2: wire external AgentCoordinator so autonomous loop can pull tasks
    if (!m_agentCoordinatorForPipeline) {
        m_agentCoordinatorForPipeline = CreateAgentCoordinator();
        if (m_agentCoordinatorForPipeline && AgentCoordinator_Initialize((AgentCoordinatorHandle)m_agentCoordinatorForPipeline)) {
            m_autonomousPipeline->setExternalAgentCoordinator(m_agentCoordinatorForPipeline);
            m_autonomousPipeline->setDequeueTaskFn([this](std::wstring* outDesc, int* outPriority) -> bool {
                if (!m_agentCoordinatorForPipeline || !outDesc || !outPriority) return false;
                wchar_t buf[4096];
                if (!AgentCoordinator_TryDequeueTask(static_cast<AgentCoordinatorHandle>(m_agentCoordinatorForPipeline), buf, (int)4096, outPriority))
                    return false;
                *outDesc = buf;
                return true;
            });
            LOG_INFO("Autonomous Pipeline wired to AgentCoordinator (dequeue tasks)");
        }
    }
    LOG_INFO("Autonomous Agentic Pipeline initialized and wired");
}

// onPipelineRun / onPipelineAutonomyStart / onPipelineAutonomyStop defined after #endif below (outside #if 0)

void Win32IDE::onBoundedAgentLoop() {
    LOG_INFO("onBoundedAgentLoop called");
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    if (!m_agenticBridge) {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Get task and iteration limit from user
    char taskDesc[1024] = {0};
    if (DialogBoxParamA(m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
        [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR {
            switch (msg) {
                case WM_INITDIALOG:
                    SetWindowTextA(GetDlgItem(hwnd, 101), "Enter task for Bounded Agent Loop (task:max_iterations):");
                    return TRUE;
                case WM_COMMAND:
                    if (LOWORD(wp) == IDOK) {
                        GetDlgItemTextA(hwnd, 102, (char*)lp, 1024);
                        EndDialog(hwnd, IDOK);
                        return TRUE;
                    } else if (LOWORD(wp) == IDCANCEL) {
                        EndDialog(hwnd, IDCANCEL);
                        return TRUE;
                    }
                    break;
            }
            return FALSE;
        }, (LPARAM)taskDesc) != IDOK) {
        return;
    }
    
    std::string input(taskDesc);
    std::string task = input;
    int maxIterations = 5; // default
    
    size_t colonPos = input.find(':');
    if (colonPos != std::string::npos) {
        task = input.substr(0, colonPos);
        std::string iterStr = input.substr(colonPos + 1);
        try {
            maxIterations = std::stoi(iterStr);
            if (maxIterations <= 0) maxIterations = 5;
        } catch (...) {
            maxIterations = 5;
        }
    }
    
    if (task.empty()) {
        task = "Execute bounded task";
    }
    
    appendToOutput("⚙️ Bounded Agent Loop with max iterations (" + std::to_string(maxIterations) + "): " + task + "\n", "Output", OutputSeverity::Info);
    
    // Execute with bounded retries in background
    std::thread([this, taskStr = task, maxIter = maxIterations]() {
        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
        if (_guard.cancelled) return;
        m_agenticBridge->ExecuteBoundedAgentLoop(taskStr, maxIter);
    }).detach();
}

#endif  // 0

// ensureAutonomousPipelineInitialized + pipeline handlers (defined here so they are compiled)
// E1: workspace-aware prompt injection  E2: loop telemetry  E3: shutdown guard
// E4: backend health pre-check          E5: memory snapshot E6: context-window sync
// E7: subagent status on loop end
void Win32IDE::ensureAutonomousPipelineInitialized()
{
    if (m_autonomousPipeline)
        return;

    // E4: probe active backend before wiring the pipeline
    if (m_backendManagerInitialized)
        probeBackendHealth(m_activeBackend);

    m_autonomousPipeline = std::make_unique<RawrXD::AutonomousAgenticPipelineCoordinator>();

    // E6: context window size — not yet exposed on pipeline coordinator
    // TODO: add setContextWindow() to AutonomousAgenticPipelineCoordinator

    // E1 + E5: workspace root + recent memory snapshot injected into every prompt
    m_autonomousPipeline->setBuildPrompt(
        [this](const std::string& m)
        {
            std::string enriched = m;
            // E1: prepend workspace root
            const std::string ws = !m_projectRoot.empty() ? m_projectRoot : m_explorerRootPath;
            if (!ws.empty())
                enriched = "[Workspace: " + ws + "]\n" + enriched;
            // E5: prepend last 3 agent memory entries
            if (m_agentHistoryEnabled)
            {
                std::lock_guard<std::mutex> lk(m_eventBufferMutex);
                int shown = 0;
                std::string mem;
                for (auto it = m_eventBuffer.rbegin(); it != m_eventBuffer.rend() && shown < 3; ++it, ++shown)
                    if (!it->prompt.empty())
                        mem = "[ctx] " + truncateForLog(it->prompt, 80) + "\n" + mem;
                if (!mem.empty())
                    enriched = mem + enriched;
            }
            return buildChatPrompt(enriched);
        });

    // E3: wrap LLM route with shutdown guard
    m_autonomousPipeline->setRouteLLM(
        [this](const std::string& p) -> std::string
        {
            if (isShuttingDown())
                return "[pipeline] shutdown requested";
            return routeWithIntelligence(p);
        });
    m_autonomousPipeline->setOnToken([this](const std::string& t, bool) { onInferenceToken(t); });
    m_autonomousPipeline->setAppendRenderer([this](const std::string& s) { appendStreamingToken(s); });
    if (!m_agentCoordinatorForPipeline)
    {
        m_agentCoordinatorForPipeline = CreateAgentCoordinator();
        if (m_agentCoordinatorForPipeline &&
            AgentCoordinator_Initialize((AgentCoordinatorHandle)m_agentCoordinatorForPipeline))
        {
            m_autonomousPipeline->setExternalAgentCoordinator(m_agentCoordinatorForPipeline);
            m_autonomousPipeline->setDequeueTaskFn(
                [this](std::wstring* outDesc, int* outPriority) -> bool
                {
                    if (!m_agentCoordinatorForPipeline || !outDesc || !outPriority)
                        return false;
                    wchar_t buf[4096];
                    if (!AgentCoordinator_TryDequeueTask(
                            static_cast<AgentCoordinatorHandle>(m_agentCoordinatorForPipeline), buf, (int)4096,
                            outPriority))
                        return false;
                    *outDesc = buf;
                    return true;
                });
            LOG_INFO("Autonomous Pipeline wired to AgentCoordinator (dequeue tasks)");
        }
    }
    LOG_INFO("Autonomous Agentic Pipeline initialized and wired");
}

void Win32IDE::onPipelineRun()
{
    ensureAutonomousPipelineInitialized();
    if (!m_autonomousPipeline)
        return;
    // E2: record telemetry
    recordSimpleEvent(AgentEventType::AgentStarted, "pipeline:run");
    std::string msg = "Run pipeline once from IDE.";
    auto result = m_autonomousPipeline->runPipeline(msg);
    if (result.success)
    {
        recordSimpleEvent(AgentEventType::AgentCompleted, "pipeline:run");
        appendToOutput("Pipeline run completed.\n", "Output", OutputSeverity::Info);
    }
    else
    {
        recordSimpleEvent(AgentEventType::AgentFailed, "pipeline:run:" + result.error.message);
        appendToOutput("Pipeline failed: " + result.error.message + "\n", "Output", OutputSeverity::Error);
    }
    // E7: show subagent status after run
    if (m_agenticBridge)
        appendToOutput(m_agenticBridge->GetSubAgentStatus() + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onPipelineAutonomyStart()
{
    ensureAutonomousPipelineInitialized();
    if (!m_autonomousPipeline)
        return;
    m_autonomousPipeline->startAutonomousLoop();
    appendToOutput("Pipeline autonomous loop started.\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onPipelineAutonomyStop()
{
    if (!m_autonomousPipeline)
        return;
    m_autonomousPipeline->stopAutonomousLoop();
    appendToOutput("Pipeline autonomous loop stopped.\n", "Output", OutputSeverity::Info);
}

// Initialize the Agentic Bridge — Full Agentic IDE is the single entry point (src/full_agentic_ide/)
void Win32IDE::initializeAgenticBridge()
{
    LOG_INFO("Initializing Full Agentic IDE (single orchestrator)");

    if (!m_fullAgenticIDE)
    {
        try
        {
            m_fullAgenticIDE = std::make_unique<full_agentic_ide::FullAgenticIDE>(this);

            // Set output callback to send agent responses to Copilot Chat
            m_fullAgenticIDE->setOutputCallback(
                [this](const std::string& title, const std::string& content)
                {
                    appendToOutput(title + ":\n" + content + "\n", "Output", OutputSeverity::Info);

                    // Also send to Copilot Chat if available
                    if (m_hwndCopilotChatOutput)
                    {
                        std::string formatted = "🤖 " + title + "\n" + content + "\n\n";
                        SendMessageA(m_hwndCopilotChatOutput, EM_SETSEL, -1, -1);
                        SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)formatted.c_str());
                    }
                });

            // Initialize with default framework path; no required default model
            full_agentic_ide::FullAgenticIDEConfig config;
            config.frameworkPath = "";
            config.defaultModel = "";
            char* envPath = getenv("AGENTIC_FRAMEWORK_PATH");
            if (envPath)
                config.frameworkPath = envPath;

            if (m_fullAgenticIDE->initialize(config))
            {
                m_agenticBridge = m_fullAgenticIDE->getBridge();
                if (!m_agenticBridge)
                {
                    LOG_ERROR("Full Agentic IDE init: getBridge() returned null");
                    return;
                }

                // Load agent configuration from file if exists
                std::string configPath = m_currentDirectory;
                if (configPath.empty())
                {
                    configPath = m_currentFile;
                    if (!configPath.empty())
                    {
                        size_t lastSlash = configPath.find_last_of("\\/");
                        if (lastSlash != std::string::npos)
                            configPath = configPath.substr(0, lastSlash);
                        else
                            configPath.clear();
                    }
                }
                if (configPath.empty())
                    configPath = ".";
                configPath += "\\agent_config.json";
                if (std::ifstream(configPath).good())
                {
                    m_agenticBridge->LoadConfiguration(configPath);
                    LOG_INFO("Loaded agent configuration from: " + configPath);
                }
                else
                {
                    LOG_INFO("No agent configuration file found, using defaults");
                }

                std::string frameworkPath = config.frameworkPath;
                LOG_INFO("Agentic Bridge initialized successfully");
                appendToOutput("✅ Agentic Framework initialized\n", "Output", OutputSeverity::Info);

                // Initialize Autonomy Manager
                initializeAutonomy();

                // Initialize Native Engine if not already done
                if (!m_nativeEngine)
                {
                    try
                    {
                        m_nativeEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
                        m_nativeEngineLoaded = false;
                        LOG_INFO("Native CPU Inference Engine created");
                        appendToOutput("✅ Native CPU Inference Engine created\n", "Output", OutputSeverity::Info);
                    }
                    catch (const std::exception& e)
                    {
                        LOG_WARNING(std::string("Failed to create Native CPU Inference Engine: ") + e.what());
                        appendToOutput("⚠️ Failed to create Native CPU Inference Engine\n", "Output",
                                       OutputSeverity::Warning);
                    }
                }

                // Default AI modes: all extended modes ON (chat panel matches bridge; user may toggle off).
                m_agenticBridge->SetMaxMode(true);
                m_agenticBridge->SetDeepThinking(true);
                m_agenticBridge->SetDeepResearch(true);
                m_agenticBridge->SetNoRefusal(true);
                m_agenticBridge->SetContextSize("32k");  // Default context size

                // Sync workspace root so agent has project context (see AGENTIC_AND_MODEL_LOADING_AUDIT.md)
                if (!m_projectRoot.empty())
                    m_agenticBridge->SetWorkspaceRoot(m_projectRoot);
                else if (!m_explorerRootPath.empty())
                    m_agenticBridge->SetWorkspaceRoot(m_explorerRootPath);

                // Propagate to Native Engine if available
                if (m_nativeEngine)
                {
                    m_nativeEngine->SetMaxMode(false);
                    m_nativeEngine->SetDeepThinking(true);
                    m_nativeEngine->SetDeepResearch(false);
                    m_nativeEngine->SetContextSize(32768);
                }

                // Load default memory plugins
                std::string pluginDir = frameworkPath.empty() ? "." : frameworkPath;
                pluginDir += "\\plugins";
                if (std::filesystem::exists(pluginDir))
                {
                    for (const auto& entry : std::filesystem::directory_iterator(pluginDir))
                    {
                        if (entry.path().extension() == ".dll")
                        {
                            loadMemoryPlugin(entry.path().string());
                        }
                    }
                }

                // Set up additional callbacks for enhanced functionality
                m_agenticBridge->SetErrorCallback(
                    [this](const std::string& error)
                    { appendToOutput("❌ Agent Error: " + error + "\n", "Errors", OutputSeverity::Error); });

                m_agenticBridge->SetProgressCallback(
                    [this](const std::string& progress)
                    { appendToOutput("🔄 " + progress + "\n", "Output", OutputSeverity::Info); });

                // Initialize multi-agent system if enabled
                if (m_multiAgentEnabled)
                {
                    m_agenticBridge->EnableMultiAgent(true);
                    appendToOutput("✅ Multi-Agent system enabled\n", "Output", OutputSeverity::Info);
                }

                // Set initial language context
                m_agenticBridge->SetLanguageContext(getSyntaxLanguageName(), m_currentFile);

                // Warm up the model with a simple query to reduce first-response latency
                std::thread(
                    [this]()
                    {
                        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
                        if (_guard.cancelled)
                            return;
                        m_agenticBridge->WarmUpModel();
                    })
                    .detach();

                // --- Surgical Slot 20 Activation ---
                // 1. Register the bridge with the IAT (maps slot 20 to our C++ implementation)
                if (RawrXD::Bridge::RegisterSwarmBridgeWithIAT())
                {
                    // 2. Perform actual initialization of the swarm system
                    RawrXD::Bridge::SwarmInitConfig swarmConfig;
                    memset(&swarmConfig, 0, sizeof(swarmConfig));
                    swarmConfig.structSize = sizeof(RawrXD::Bridge::SwarmInitConfig);
                    swarmConfig.maxSubAgents = 16;
                    swarmConfig.taskTimeoutMs = 120000;  // 2 minutes
                    swarmConfig.enableGPUWorkStealing = 1;
                    sprintf_s(swarmConfig.coordinatorModel, "gemma3:1b");  // Default coordinator

                    if (RawrXD::Bridge::InitializeSwarmSystem(&swarmConfig) == S_OK)
                    {
                        LOG_INFO("Agentic Swarm System (Slot 20) initialized via bridge");
                        appendToOutput("✅ Agentic Swarm System initialized\n", "Output", OutputSeverity::Info);
                    }
                    else
                    {
                        LOG_ERROR("Failed to initialize Agentic Swarm System via bridge");
                    }
                }
                else
                {
                    LOG_ERROR("Failed to register Swarm Bridge with IAT (Slot 20 hole remains)");
                }

                LOG_INFO("Agentic Bridge fully initialized with enhancements");

                wireAgenticOrchestratorIntegration();

                syncAgentModeUiFromBridge();
            }
            else
            {
                LOG_ERROR("Failed to initialize Agentic Bridge");
                appendToOutput("❌ Failed to initialize Agentic Framework\n", "Errors", OutputSeverity::Error);
                MessageBoxA(m_hwndMain,
                            "Failed to initialize Agentic Framework.\nMake sure Agentic-Framework.ps1 is in the "
                            "Powershield folder.\nCheck logs for detailed error information.",
                            "Agent Error", MB_OK | MB_ICONERROR);
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Exception during Agentic Bridge initialization: " + std::string(e.what()));
            appendToOutput("❌ Exception during initialization: " + std::string(e.what()) + "\n", "Errors",
                           OutputSeverity::Error);
            MessageBoxA(m_hwndMain, ("Initialization failed with exception:\n" + std::string(e.what())).c_str(),
                        "Agent Error", MB_OK | MB_ICONERROR);
        }
        catch (...)
        {
            LOG_ERROR("Unknown exception during Agentic Bridge initialization");
            appendToOutput("❌ Unknown error during initialization\n", "Errors", OutputSeverity::Error);
            MessageBoxA(m_hwndMain, "Unknown error during Agentic Framework initialization", "Agent Error",
                        MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        LOG_INFO("Agentic Bridge already initialized");
    }
}

// Start Agent Loop - multi-turn agentic conversation
void Win32IDE::onAgentStartLoop()
{
    LOG_INFO("onAgentStartLoop called");
    // Start Agent Loop fallback when invoked from command palette: show dialog if chat input unavailable (gotPrompt).

    if (!m_agenticBridge)
    {
        initializeAgenticBridge();
    }

    if (!m_agenticBridge)
    {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Show input dialog for user prompt
    char prompt[1024] = {0};
    if (DialogBoxParamA(
            m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
            [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR
            {
                switch (msg)
                {
                    case WM_INITDIALOG:
                    {
                        SetWindowTextA(GetDlgItem(hwnd, 101), "Enter your task for the agent:");
                        return TRUE;
                    }
                    case WM_COMMAND:
                        if (LOWORD(wp) == IDOK)
                        {
                            GetDlgItemTextA(hwnd, 102, (char*)lp, 1024);
                            EndDialog(hwnd, IDOK);
                            return TRUE;
                        }
                        else if (LOWORD(wp) == IDCANCEL)
                        {
                            EndDialog(hwnd, IDCANCEL);
                            return TRUE;
                        }
                        break;
                }
                return FALSE;
            },
            (LPARAM)prompt) != IDOK)
    {
        return;
    }

    // Fallback to simple input box if dialog fails
    if (strlen(prompt) == 0)
    {
        strcpy_s(prompt, "Analyze the current file and suggest improvements");
    }

    std::string promptStr(prompt);

    // Enrich the user prompt with language context
    promptStr = buildLanguageAwarePrompt(promptStr);

    // Start agent loop in background thread
    appendToOutput("🚀 Starting Agent Loop: " + promptStr + "\n", "Output", OutputSeverity::Info);

    std::thread(
        [this, promptStr]()
        {
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled)
                return;
            if (m_agenticBridge->StartAgentLoop(promptStr, 10))
            {
                LOG_INFO("Agent loop completed successfully");
            }
            else
            {
                LOG_ERROR("Agent loop failed");
            }
        })
        .detach();
}

// Execute single agent command
void Win32IDE::onAgentExecuteCommand()
{
    LOG_INFO("onAgentExecuteCommand called");
    // Fallback when invoked from command palette: if chat input missing, we still surface a dialog-based Execute
    // Command.

    if (!m_agenticBridge)
    {
        initializeAgenticBridge();
    }

    if (!m_agenticBridge)
    {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Get command from Copilot Chat input
    if (m_hwndCopilotChatInput)
    {
        char input[2048] = {0};
        GetWindowTextA(m_hwndCopilotChatInput, input, sizeof(input));

        if (strlen(input) == 0)
        {
            MessageBoxA(m_hwndMain, "Enter a command in the Copilot Chat input box", "Agent",
                        MB_OK | MB_ICONINFORMATION);
            return;
        }

        std::string command(input);
        appendToOutput("⚡ Executing Agent Command: " + command + "\n", "Output", OutputSeverity::Info);

        // Execute in background
        std::thread(
            [this, command]()
            {
                DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
                if (_guard.cancelled)
                    return;
                AgentResponse response = m_agenticBridge->ExecuteAgentCommand(command);

                // Phase 4B: Choke Point 3 — hookAgentCommand after direct command execution
                FailureClassification cmdFailure = hookAgentCommand(response.content, command);
                if (cmdFailure.reason != AgentFailureType::None)
                {
                    // Failure detected — attempt bounded retry
                    AgentResponse retryResponse = executeWithBoundedRetry(command);
                    if (!retryResponse.content.empty() && retryResponse.type != AgentResponseType::AGENT_ERROR)
                    {
                        response = retryResponse;
                    }
                }

                std::string output = "Agent Response:\n";
                output += "Type: " + std::to_string((int)response.type) + "\n";
                output += "Content: " + response.content + "\n";

                if (!response.toolName.empty())
                {
                    output += "Tool: " + response.toolName + "\n";
                    output += "Args: " + response.toolArgs + "\n";
                }

                appendToOutput(output, "Output", OutputSeverity::Info);
            })
            .detach();

        // Clear input
        SetWindowTextA(m_hwndCopilotChatInput, "");
    }
    else
    {
        // Fallback when invoked from command palette without chat panel: create inline input dialog
        // CreateWindowExA fallback for Execute Command
        char gotPrompt[2048] = {0};
        HWND hwndFallback = CreateWindowExA(0, "STATIC", "Enter command:", WS_CHILD, 0, 0, 0, 0, m_hwndMain, nullptr,
                                            m_hInstance, nullptr);
        DestroyWindow(hwndFallback);  // transient marker

        if (DialogBoxParamA(
                m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
                [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR
                {
                    switch (msg)
                    {
                        case WM_INITDIALOG:
                            SetWindowTextA(GetDlgItem(hwnd, 101), "Execute Command — enter prompt:");
                            return TRUE;
                        case WM_COMMAND:
                            if (LOWORD(wp) == IDOK)
                            {
                                GetDlgItemTextA(hwnd, 102, (char*)lp, 2048);
                                EndDialog(hwnd, IDOK);
                                return TRUE;
                            }
                            else if (LOWORD(wp) == IDCANCEL)
                            {
                                EndDialog(hwnd, IDCANCEL);
                                return TRUE;
                            }
                            break;
                    }
                    return FALSE;
                },
                (LPARAM)gotPrompt) == IDOK &&
            strlen(gotPrompt) > 0)
        {
            std::string command(gotPrompt);
            appendToOutput("⚡ Executing Agent Command (palette): " + command + "\n", "Output", OutputSeverity::Info);

            std::thread(
                [this, command]()
                {
                    DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
                    if (_guard.cancelled)
                        return;
                    AgentResponse response = m_agenticBridge->ExecuteAgentCommand(command);
                    std::string output = "Agent Response:\n" + response.content + "\n";
                    appendToOutput(output, "Output", OutputSeverity::Info);
                })
                .detach();
        }
    }
}

// Configure AI model
void Win32IDE::onAgentConfigureModel()
{
    LOG_INFO("onAgentConfigureModel called");

    // Initialize agentic bridge if needed
    if (!m_agenticBridge)
    {
        initializeAgenticBridge();
    }

    if (!m_agenticBridge)
    {
        MessageBoxA(m_hwndMain,
                    "Agentic Framework not initialized.\nPlease use Agent > Start Loop first to initialize.",
                    "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }

    // Retrieve available models from Ollama with enhanced error handling
    std::vector<std::string> availableModels;
    std::string connectionStatus = "Probing Ollama connection...";
    bool ollamaAvailable = false;

    try
    {
        RawrXD::Agent::OllamaConfig probeCfg;
        probeCfg.timeout_ms = 5000;  // Increased timeout for reliability
        RawrXD::Agent::AgentOllamaClient probeClient(probeCfg);

        if (probeClient.TestConnection())
        {
            availableModels = probeClient.ListModels();
            ollamaAvailable = true;
            connectionStatus = "✅ Connected to Ollama";
            LOG_INFO("Successfully queried Ollama for available models: " + std::to_string(availableModels.size()) +
                     " found");
        }
        else
        {
            connectionStatus = "⚠️ Ollama connection failed";
            LOG_WARNING("Ollama connection test failed");
        }
    }
    catch (const std::exception& e)
    {
        connectionStatus = std::string("❌ Ollama error: ") + e.what();
        LOG_ERROR("Ollama probe exception: " + std::string(e.what()));
        availableModels.clear();
    }
    catch (...)
    {
        connectionStatus = "❌ Ollama probe failed (unknown error)";
        LOG_ERROR("Ollama probe failed with unknown exception");
        availableModels.clear();
    }

    // Fallback to ModelConnection if Ollama probe failed
    if (!ollamaAvailable)
    {
        try
        {
            ModelConnection connection;
            availableModels = connection.getAvailableModels();
            if (!availableModels.empty())
            {
                ollamaAvailable = true;
                connectionStatus = "✅ Connected via ModelConnection";
                LOG_INFO("Successfully queried ModelConnection for available models: " +
                         std::to_string(availableModels.size()) + " found");
            }
        }
        catch (const std::exception& e)
        {
            connectionStatus = std::string("❌ ModelConnection error: ") + e.what();
            LOG_ERROR("ModelConnection probe exception: " + std::string(e.what()));
            availableModels.clear();
        }
        catch (...)
        {
            connectionStatus = "❌ ModelConnection probe failed (unknown error)";
            LOG_ERROR("ModelConnection probe failed with unknown exception");
            availableModels.clear();
        }
    }

    if (availableModels.empty())
    {
        std::string detailedMsg = std::string(connectionStatus) +
                                  "\n\nNo models available. Please ensure:\n"
                                  "1. Ollama is installed and running (ollama serve)\n"
                                  "2. At least one model is pulled (ollama pull <model>)\n"
                                  "3. Ollama is accessible at http://localhost:11434\n\n"
                                  "Common models: llama2, mistral, neural-chat, deepseek-coder";

        MessageBoxA(m_hwndMain, detailedMsg.c_str(), "Agent Model Configuration", MB_OK | MB_ICONWARNING);
        appendToOutput("⚠️ Model configuration failed: " + detailedMsg + "\n", "Output", OutputSeverity::Warning);
        return;
    }

    // Get current model and ensure it's in the list
    const std::string currentModel = m_agenticBridge->GetCurrentModel();
    bool currentModelFound =
        std::find(availableModels.begin(), availableModels.end(), currentModel) != availableModels.end();

    if (!currentModelFound && !currentModel.empty())
    {
        // Insert current model at the beginning if it's not in the available list
        availableModels.insert(availableModels.begin(), currentModel + " (current, not available)");
    }

    // Sort models alphabetically for better UX (keep current at top if found)
    if (currentModelFound)
    {
        std::sort(availableModels.begin() + 1, availableModels.end());
    }
    else
    {
        std::sort(availableModels.begin(), availableModels.end());
    }

    // Create modal dialog window with enhanced styling
    RECT parentRect{};
    GetWindowRect(m_hwndMain, &parentRect);

    const int dlgWidth = 500;
    const int dlgHeight = 280;
    const int dlgX = parentRect.left + ((parentRect.right - parentRect.left) - dlgWidth) / 2;
    const int dlgY = parentRect.top + ((parentRect.bottom - parentRect.top) - dlgHeight) / 2;

    EnableWindow(m_hwndMain, FALSE);

    HWND hwndDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "STATIC", "🤖 Configure AI Model",
                                   WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, dlgX, dlgY, dlgWidth, dlgHeight,
                                   m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hwndDlg)
    {
        EnableWindow(m_hwndMain, TRUE);
        MessageBoxA(m_hwndMain, "Failed to create model selection dialog", "Agent Error", MB_OK | MB_ICONERROR);
        LOG_ERROR("Failed to create model selection dialog window");
        return;
    }

    // Connection status label
    CreateWindowExA(0, "STATIC", connectionStatus.c_str(), WS_CHILD | WS_VISIBLE, 10, 10, dlgWidth - 20, 18, hwndDlg,
                    nullptr, m_hInstance, nullptr);

    // Model selection label
    CreateWindowExA(0, "STATIC", "Select an available model:", WS_CHILD | WS_VISIBLE, 10, 35, dlgWidth - 20, 16,
                    hwndDlg, nullptr, m_hInstance, nullptr);

    // Combo box for model selection
    HWND hwndCombo =
        CreateWindowExA(WS_EX_CLIENTEDGE, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 10, 53,
                        dlgWidth - 20, 160, hwndDlg, (HMENU)1001, m_hInstance, nullptr);

    int selectedIndex = 0;
    for (size_t i = 0; i < availableModels.size(); ++i)
    {
        const auto& model = availableModels[i];
        SendMessageA(hwndCombo, CB_ADDSTRING, 0, (LPARAM)model.c_str());

        // Pre-select current model or first in list
        if (model == currentModel)
        {
            selectedIndex = static_cast<int>(i);
        }
        else if (i == 0 && selectedIndex == 0)
        {
            selectedIndex = 0;  // Keep first index as fallback
        }
    }
    SendMessageA(hwndCombo, CB_SETCURSEL, selectedIndex, 0);

    // Current model info label
    std::string currentModelLabel = "Current model: " + (currentModel.empty() ? "(none)" : currentModel);
    CreateWindowExA(0, "STATIC", currentModelLabel.c_str(), WS_CHILD | WS_VISIBLE, 10, dlgHeight - 85, dlgWidth - 20,
                    16, hwndDlg, nullptr, m_hInstance, nullptr);

    // Buttons: Use Model and Cancel
    HWND hwndOk = CreateWindowExA(0, "BUTTON", "Use Model", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, dlgWidth - 200,
                                  dlgHeight - 50, 90, 30, hwndDlg, (HMENU)IDOK, m_hInstance, nullptr);
    HWND hwndCancel = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE, dlgWidth - 100, dlgHeight - 50, 80,
                                      30, hwndDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);

    bool accepted = false;
    std::string selectedModel = currentModel;

    // Modal message loop
    MSG msg{};
    while (IsWindow(hwndDlg) && GetMessage(&msg, nullptr, 0, 0))
    {
        if (msg.message == WM_COMMAND)
        {
            const WORD cmdId = LOWORD(msg.wParam);
            const WORD cmdCode = HIWORD(msg.wParam);

            if (cmdId == IDOK && cmdCode == BN_CLICKED)
            {
                int sel = (int)SendMessageA(hwndCombo, CB_GETCURSEL, 0, 0);
                if (sel != CB_ERR)
                {
                    char buffer[256] = {0};
                    SendMessageA(hwndCombo, CB_GETLBTEXT, sel, (LPARAM)buffer);
                    selectedModel = buffer;

                    // Remove "(current, not available)" suffix if present
                    size_t notAvailPos = selectedModel.find(" (current, not available)");
                    if (notAvailPos != std::string::npos)
                    {
                        selectedModel = selectedModel.substr(0, notAvailPos);
                    }

                    accepted = true;
                    LOG_INFO("User selected model: " + selectedModel);
                }
                DestroyWindow(hwndDlg);
                break;
            }
            else if (cmdId == IDCANCEL && cmdCode == BN_CLICKED)
            {
                LOG_INFO("Model configuration cancelled");
                DestroyWindow(hwndDlg);
                break;
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);

    // Apply selected model with validation
    if (accepted && !selectedModel.empty())
    {
        // Validate model selection
        if (selectedModel == currentModel)
        {
            appendToOutput("ℹ️ Model already configured: " + selectedModel + "\n", "Output", OutputSeverity::Info);
            return;
        }

        // Set model on agentic bridge
        try
        {
            m_agenticBridge->SetModel(selectedModel);

            // Also set on native engine if available
            if (m_nativeEngine)
            {
                // Native engine model setting if applicable
                LOG_INFO("Setting model on native engine: " + selectedModel);
            }

            appendToOutput("✅ Agent model configured: " + selectedModel + "\n", "Output", OutputSeverity::Info);
            MessageBoxA(m_hwndMain,
                        ("Model successfully updated to:\n" + selectedModel +
                         "\n\nThe new model will be used for all future agent operations.")
                            .c_str(),
                        "Agent Model Configuration", MB_OK | MB_ICONINFORMATION);

            LOG_INFO("Model successfully set to: " + selectedModel);
        }
        catch (const std::exception& e)
        {
            std::string errorMsg = std::string("Failed to set model: ") + e.what();
            appendToOutput("❌ " + errorMsg + "\n", "Errors", OutputSeverity::Error);
            MessageBoxA(m_hwndMain, errorMsg.c_str(), "Model Configuration Error", MB_OK | MB_ICONERROR);
            LOG_ERROR("Exception setting model: " + errorMsg);
        }
        catch (...)
        {
            std::string errorMsg = "Failed to set model (unknown error)";
            appendToOutput("❌ " + errorMsg + "\n", "Errors", OutputSeverity::Error);
            MessageBoxA(m_hwndMain, errorMsg.c_str(), "Model Configuration Error", MB_OK | MB_ICONERROR);
            LOG_ERROR("Unknown exception while setting model");
        }
    }
    else
    {
        LOG_INFO("Model configuration completed without selection");
    }
}

// View available agent tools
void Win32IDE::onAgentViewTools()
{
    LOG_INFO("onAgentViewTools called");

    if (!m_agenticBridge)
    {
        initializeAgenticBridge();
    }

    if (!m_agenticBridge)
    {
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized", "Agent Error", MB_OK | MB_ICONERROR);
        return;
    }

    std::vector<std::string> tools = m_agenticBridge->GetAvailableTools();

    std::stringstream toolsList;
    toolsList << "Available Agent Tools:\n\n";

    for (const auto& tool : tools)
    {
        toolsList << "• " << tool << "\n";
    }

    toolsList << "\nThese tools can be invoked by the agent to perform tasks.\n";
    toolsList << "Example: TOOL:shell:{\"cmd\":\"Get-Process\"}";

    MessageBoxA(m_hwndMain, toolsList.str().c_str(), "Agent Tools", MB_OK | MB_ICONINFORMATION);

    // Also log to output
    appendToOutput(toolsList.str() + "\n", "Output", OutputSeverity::Info);
}

// View agent status
void Win32IDE::onAgentViewStatus()
{
    LOG_INFO("onAgentViewStatus called");

    if (!m_agenticBridge)
    {
        appendToOutput("Agentic Bridge not initialized\n", "Output", OutputSeverity::Warning);
        MessageBoxA(m_hwndMain, "Agentic Framework not initialized.\nUse Agent > Start Loop to initialize.",
                    "Agent Status", MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::string status = m_agenticBridge->GetAgentStatus();
    appendToOutput("=== Agent Status ===\n" + status + "\n", "Output", OutputSeverity::Info);
    MessageBoxA(m_hwndMain, status.c_str(), "Agent Status", MB_OK | MB_ICONINFORMATION);
}

// Stop agent loop
void Win32IDE::onAgentStop()
{
    LOG_INFO("onAgentStop called");

    if (!m_agenticBridge)
    {
        return;
    }

    if (m_agenticBridge->IsAgentLoopRunning())
    {
        m_agenticBridge->StopAgentLoop();
        appendToOutput("🛑 Agent loop stopped\n", "Output", OutputSeverity::Warning);
        MessageBoxA(m_hwndMain, "Agent loop stopped", "Agent", MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        MessageBoxA(m_hwndMain, "No agent loop is currently running", "Agent", MB_OK | MB_ICONINFORMATION);
    }
}

// ============================================================================
// KEYWORD: handleAgentCommand IMPLEMENTATION
// Routes all AI/Agent commands from the 4100-4300 range
// ============================================================================
void Win32IDE::handleAgentCommand(int commandId)
{
    // Ensure agent bridge is ready (lazy init)
    if (!m_agenticBridge)
        initializeAgenticBridge();

    // Push current language context to the agent on every command dispatch
    if (m_agenticBridge)
    {
        m_agenticBridge->SetLanguageContext(getSyntaxLanguageName(), m_currentFile);
    }

    switch (commandId)
    {
        // --- Agent Execution ---
        case IDM_AGENT_START_LOOP:
            onAgentStartLoop();
            break;
        case IDM_AGENT_BOUNDED_LOOP:
            onBoundedAgentLoop();
            break;
        case IDM_AGENT_EXECUTE_CMD:
            onAgentExecuteCommand();
            break;
        case IDM_AGENT_CONFIGURE_MODEL:
            onAgentConfigureModel();
            break;
        case IDM_AGENT_VIEW_TOOLS:
            onAgentViewTools();
            break;
        case IDM_AGENT_VIEW_STATUS:
            onAgentViewStatus();
            break;
        case IDM_AGENT_STOP:
            onAgentStop();
            break;
        case IDM_AGENT_AUTONOMOUS_COMMUNICATOR:
            HandleAutonomousCommunicator(this);
            break;

        // --- Autonomy ---
        case IDM_AUTONOMY_TOGGLE:
            onAutonomyToggle();
            break;
        case IDM_AUTONOMY_START:
            onAutonomyStart();
            break;
        case IDM_AUTONOMY_STOP:
            onAutonomyStop();
            break;
        case IDM_AUTONOMY_SET_GOAL:
            onAutonomySetGoal();
            break;
        case IDM_AUTONOMY_STATUS:
            onAutonomyViewStatus();
            break;
        case IDM_AUTONOMY_MEMORY:
            onAutonomyViewMemory();
            break;
        case IDM_PIPELINE_RUN:
            onPipelineRun();
            break;
        case IDM_PIPELINE_AUTONOMY_START:
            onPipelineAutonomyStart();
            break;
        case IDM_PIPELINE_AUTONOMY_STOP:
            onPipelineAutonomyStop();
            break;
        case IDM_TELEMETRY_UNIFIED_CORE:
            HandleUnifiedTelemetry(this);
            break;

        // --- Agent Memory (Phase 19B) ---
        case IDM_AGENT_MEMORY:
            onAgentMemoryView();
            break;
        case IDM_AGENT_MEMORY_VIEW:
            onAgentMemoryView();
            break;
        case IDM_AGENT_MEMORY_CLEAR:
            onAgentMemoryClear();
            break;
        case IDM_AGENT_MEMORY_EXPORT:
            onAgentMemoryExport();
            break;

        // --- SubAgent Chain / Swarm / Todo (Phase 19B) ---
        case IDM_SUBAGENT_CHAIN:
            onSubAgentChain();
            break;
        case IDM_SUBAGENT_SWARM:
            onSubAgentSwarm();
            break;
        case IDM_SUBAGENT_TODO_LIST:
            onSubAgentTodoList();
            break;
        case IDM_SUBAGENT_TODO_CLEAR:
            onSubAgentTodoClear();
            break;
        case IDM_SUBAGENT_STATUS:
            onSubAgentStatus();
            break;

        // --- Agentic Planning Orchestrator (Full Approval Gates) ---
        case IDM_PLANNING_START:
            onPlanningStart();
            break;
        case IDM_PLANNING_SHOW_QUEUE:
            onPlanningShowQueue();
            break;
        case IDM_PLANNING_APPROVE_STEP:
            onPlanningApproveStep();
            break;
        case IDM_PLANNING_REJECT_STEP:
            onPlanningRejectStep();
            break;
        case IDM_PLANNING_EXECUTE_STEP:
            onPlanningExecuteStep();
            break;
        case IDM_PLANNING_EXECUTE_ALL:
            onPlanningExecuteAll();
            break;
        case IDM_PLANNING_ROLLBACK:
            onPlanningRollback();
            break;
        case IDM_PLANNING_SET_POLICY:
            onPlanningSetPolicy();
            break;
        case IDM_PLANNING_VIEW_STATUS:
            onPlanningViewStatus();
            break;
        case IDM_PLANNING_DIAGNOSTICS:
            onPlanningDiagnostics();
            break;

        // --- AI Options (Max Mode / Reasoning) ---
        case IDM_AI_MODE_MAX:
        {
            bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_MAX, MF_BYCOMMAND) & MF_CHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_MODE_MAX, current ? MF_UNCHECKED : MF_CHECKED);
            if (m_agenticBridge)
                m_agenticBridge->SetMaxMode(!current);
            appendToOutput(std::string("Max Mode ") + (!current ? "ENABLED" : "DISABLED") + "\n", "Output",
                           OutputSeverity::Info);
            break;
        }
        case IDM_AI_MODE_DEEP_THINK:
        {
            bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_DEEP_THINK, MF_BYCOMMAND) & MF_CHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_THINK, current ? MF_UNCHECKED : MF_CHECKED);
            if (m_agenticBridge)
                m_agenticBridge->SetDeepThinking(!current);
            appendToOutput(std::string("Deep Thinking (CoT) ") + (!current ? "ENABLED" : "DISABLED") + "\n", "Output",
                           OutputSeverity::Info);
            break;
        }
        case IDM_AI_MODE_DEEP_RESEARCH:
        {
            bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, MF_BYCOMMAND) & MF_CHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, current ? MF_UNCHECKED : MF_CHECKED);
            if (m_agenticBridge)
                m_agenticBridge->SetDeepResearch(!current);
            appendToOutput(std::string("Deep Research ") + (!current ? "ENABLED" : "DISABLED") + "\n", "Output",
                           OutputSeverity::Info);
            break;
        }
        case IDM_AI_MODE_NO_REFUSAL:
        {
            bool current = (GetMenuState(m_hMenu, IDM_AI_MODE_NO_REFUSAL, MF_BYCOMMAND) & MF_CHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_MODE_NO_REFUSAL, current ? MF_UNCHECKED : MF_CHECKED);
            if (m_agenticBridge)
                m_agenticBridge->SetNoRefusal(!current);
            appendToOutput(std::string("No Refusal Mode ") + (!current ? "ENABLED" : "DISABLED") + "\n", "Output",
                           OutputSeverity::Info);
            break;
        }

        // --- Context Window Size ---
        case IDM_AI_CONTEXT_4K:
        case IDM_AI_CONTEXT_32K:
        case IDM_AI_CONTEXT_64K:
        case IDM_AI_CONTEXT_128K:
        case IDM_AI_CONTEXT_256K:
        case IDM_AI_CONTEXT_512K:
        case IDM_AI_CONTEXT_1M:
        {
            int size = 4096;
            if (commandId == IDM_AI_CONTEXT_32K)
                size = 32768;
            if (commandId == IDM_AI_CONTEXT_64K)
                size = 65536;
            if (commandId == IDM_AI_CONTEXT_128K)
                size = 131072;
            if (commandId == IDM_AI_CONTEXT_256K)
                size = 262144;
            if (commandId == IDM_AI_CONTEXT_512K)
                size = 524288;
            if (commandId == IDM_AI_CONTEXT_1M)
                size = 1048576;

            m_inferenceConfig.contextWindow = size;
            if (m_agenticBridge)
            {
                // Determine string representation
                std::string s = "4k";
                if (commandId == IDM_AI_CONTEXT_32K)
                    s = "32k";
                if (commandId == IDM_AI_CONTEXT_64K)
                    s = "64k";
                if (commandId == IDM_AI_CONTEXT_128K)
                    s = "128k";
                if (commandId == IDM_AI_CONTEXT_256K)
                    s = "256k";
                if (commandId == IDM_AI_CONTEXT_512K)
                    s = "512k";
                if (commandId == IDM_AI_CONTEXT_1M)
                    s = "1m";
                m_agenticBridge->SetContextSize(s);
            }
            appendToOutput("Context window set to " + std::to_string(size) + " tokens\n", "Output",
                           OutputSeverity::Info);

            // Uncheck all context items
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_4K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_32K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_64K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_128K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_256K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_512K, MF_UNCHECKED);
            CheckMenuItem(m_hMenu, IDM_AI_CONTEXT_1M, MF_UNCHECKED);

            // Check active item
            CheckMenuItem(m_hMenu, commandId, MF_CHECKED);
            break;
        }

        // --- Titan Kernel & 800B Dual-Engine ---
        case IDM_AI_TITAN_TOGGLE:
        {
            const bool requestedEnable = !m_useTitanKernel;
            m_useTitanKernel = requestedEnable;
            CheckMenuItem(m_hMenu, IDM_AI_TITAN_TOGGLE, m_useTitanKernel ? MF_CHECKED : MF_UNCHECKED);
            appendToOutput(std::string("Titan Kernel ") + (m_useTitanKernel ? "ENABLED" : "DISABLED") + "\n",
                           "Output", OutputSeverity::Info);
            if (m_useTitanKernel)
            {
                appendToOutput("Titan hotpatch route applied (context/capability independent).\n", "Output",
                               OutputSeverity::Info);
            }
            break;
        }
        case IDM_AI_800B_STATUS:
        {
            bool unlocked = RawrXD::EnterpriseLicense::is800BUnlocked();
            std::string msg = unlocked ? "800B Dual-Engine: UNLOCKED (Enterprise)"
                                       : "800B Dual-Engine: locked (requires Enterprise license)";
            appendToOutput(msg + "\n", "Output", OutputSeverity::Info);
            break;
        }
        // --- Multi-Agent ---
        case IDM_AI_AGENT_MULTI_ENABLE:
        {
            m_multiAgentEnabled = true;
            appendToOutput("Multi-Agent mode ENABLED\n", "Output", OutputSeverity::Info);
            if (m_agenticBridge)
            { /* propagate if bridge supports it */
            }
            break;
        }
        case IDM_AI_AGENT_MULTI_DISABLE:
        {
            m_multiAgentEnabled = false;
            appendToOutput("Multi-Agent mode DISABLED\n", "Output", OutputSeverity::Info);
            break;
        }
        case IDM_AI_AGENT_MULTI_STATUS:
        {
            std::string st = m_multiAgentEnabled ? "Multi-Agent: ENABLED" : "Multi-Agent: DISABLED";
            appendToOutput(st + "\n", "Output", OutputSeverity::Info);
            break;
        }

        // --- Reverse Engineering Commands ---
        case IDM_REVENG_ANALYZE:
            handleReverseEngineeringAnalyze();
            break;
        case IDM_REVENG_SET_BINARY_FROM_ACTIVE:
            handleReverseEngineeringSetBinaryFromActive();
            break;
        case IDM_REVENG_SET_BINARY_FROM_DEBUG_TARGET:
            handleReverseEngineeringSetBinaryFromDebugTarget();
            break;
        case IDM_REVENG_SET_BINARY_FROM_BUILD_OUTPUT:
            handleReverseEngineeringSetBinaryFromBuildOutput();
            break;
        case IDM_REVENG_DISASM_AT_RIP:
            handleReverseEngineeringDisassembleAtRIP();
            break;
        case IDM_REVENG_DISASM:
            handleReverseEngineeringDisassemble();
            break;
        case IDM_REVENG_DUMPBIN:
            handleReverseEngineeringDumpBin();
            break;
        case IDM_REVENG_COMPILE:
            handleReverseEngineeringCompile();
            break;
        case IDM_REVENG_COMPARE:
            handleReverseEngineeringCompare();
            break;
        case IDM_REVENG_DETECT_VULNS:
            handleReverseEngineeringDetectVulns();
            break;
        case IDM_REVENG_EXPORT_IDA:
            handleReverseEngineeringExportIDA();
            break;
        case IDM_REVENG_EXPORT_GHIDRA:
            handleReverseEngineeringExportGhidra();
            break;
        case IDM_REVENG_CFG:
            handleReverseEngineeringCFG();
            break;
        case IDM_REVENG_FUNCTIONS:
            handleReverseEngineeringFunctions();
            break;
        case IDM_REVENG_DEMANGLE:
            handleReverseEngineeringDemangle();
            break;
        case IDM_REVENG_SSA:
            handleReverseEngineeringSSA();
            break;
        case IDM_REVENG_RECURSIVE_DISASM:
            handleReverseEngineeringRecursiveDisasm();
            break;
        case IDM_REVENG_TYPE_RECOVERY:
            handleReverseEngineeringTypeRecovery();
            break;
        case IDM_REVENG_DATA_FLOW:
            handleReverseEngineeringDataFlow();
            break;
        case IDM_REVENG_LICENSE_INFO:
            handleReverseEngineeringLicenseInfo();
            break;
        case IDM_REVENG_DECOMPILER_VIEW:
            handleReverseEngineeringDecompilerView();
            break;
        case IDM_REVENG_DECOMP_RENAME:
        {
            // Prompt for SSA variable rename inside the active decompiler view
            if (isDecompilerViewActive())
            {
                // Use the programmatic rename API — the user will type old→new in the output bar
                // For interactive use, the in-pane right-click + F2 is preferred;
                // this route is for command-palette / hotkey invocations.
                appendToOutput("Decompiler: Use F2 or right-click a variable in the decompiler pane to rename.\n",
                               "Decompiler", OutputSeverity::Info);
            }
            else
            {
                appendToOutput("Decompiler View is not active — open it first with Ctrl+Shift+D.\n", "Decompiler",
                               OutputSeverity::Warning);
            }
            break;
        }
        case IDM_REVENG_DECOMP_SYNC:
        {
            // Sync both panes to the address under the cursor / last selected address
            if (isDecompilerViewActive())
            {
                appendToOutput("Decompiler: Panes are synchronized — click a line in either pane.\n", "Decompiler",
                               OutputSeverity::Info);
            }
            else
            {
                appendToOutput("Decompiler View is not active — open it first with Ctrl+Shift+D.\n", "Decompiler",
                               OutputSeverity::Warning);
            }
            break;
        }
        case IDM_REVENG_DECOMP_CLOSE:
            if (isDecompilerViewActive())
            {
                destroyDecompilerView();
                appendToOutput("Decompiler View closed.\n", "Decompiler", OutputSeverity::Info);
            }
            else
            {
                appendToOutput("Decompiler View is not active.\n", "Decompiler", OutputSeverity::Info);
            }
            break;

        default:
            appendToOutput("Unknown Agent Command ID: " + std::to_string(commandId) + "\n", "Debug",
                           OutputSeverity::Warning);
            break;
    }
}

// ============================================================================
// AI MODE TOGGLE HANDLERS
// Pure state mutation + engine propagation + logging.
// These mirror the inline switch logic in handleAgentCommand but provide
// a clean callable API for programmatic use (e.g., from SidebarProcImpl).
// ============================================================================

void Win32IDE::syncAgentModeUiFromBridge()
{
    bool maxMode = false;
    bool deep = false;
    bool research = false;
    bool noRefusal = false;
    if (m_agenticBridge)
    {
        maxMode = m_agenticBridge->GetMaxMode();
        deep = m_agenticBridge->GetDeepThinking();
        research = m_agenticBridge->GetDeepResearch();
        noRefusal = m_agenticBridge->GetNoRefusal();
    }
    else if (m_agent)
    {
        maxMode = m_agent->IsMaxMode();
        deep = m_agent->IsDeepThink();
        research = m_agent->IsDeepResearch();
        noRefusal = m_agent->IsNoRefusal();
    }
    else
    {
        return;
    }

    if (m_hMenu)
    {
        CheckMenuItem(m_hMenu, IDM_AI_MODE_MAX, maxMode ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_THINK, deep ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, research ? MF_CHECKED : MF_UNCHECKED);
        CheckMenuItem(m_hMenu, IDM_AI_MODE_NO_REFUSAL, noRefusal ? MF_CHECKED : MF_UNCHECKED);
    }
    if (m_hwndChkMaxMode)
        SendMessage(m_hwndChkMaxMode, BM_SETCHECK, maxMode ? BST_CHECKED : BST_UNCHECKED, 0);
    if (m_hwndChkDeepThink)
        SendMessage(m_hwndChkDeepThink, BM_SETCHECK, deep ? BST_CHECKED : BST_UNCHECKED, 0);
    if (m_hwndChkDeepResearch)
        SendMessage(m_hwndChkDeepResearch, BM_SETCHECK, research ? BST_CHECKED : BST_UNCHECKED, 0);
    if (m_hwndChkNoRefusal)
        SendMessage(m_hwndChkNoRefusal, BM_SETCHECK, noRefusal ? BST_CHECKED : BST_UNCHECKED, 0);

    if (m_agent)
    {
        m_agent->SetMaxMode(maxMode);
        m_agent->SetDeepThink(deep);
        m_agent->SetDeepResearch(research);
        m_agent->SetNoRefusal(noRefusal);
    }
}

void Win32IDE::onAIModeMax()
{
    LOG_INFO("onAIModeMax toggled");
    bool current = false;
    if (m_agenticBridge)
        current = m_agenticBridge->GetMaxMode();
    else if (m_hMenu)
        current = (GetMenuState(m_hMenu, IDM_AI_MODE_MAX, MF_BYCOMMAND) & MF_CHECKED) != 0;
    bool newState = !current;
    if (m_hMenu)
        CheckMenuItem(m_hMenu, IDM_AI_MODE_MAX, newState ? MF_CHECKED : MF_UNCHECKED);
    if (m_agenticBridge)
        m_agenticBridge->SetMaxMode(newState);
    if (m_nativeEngine)
        m_nativeEngine->SetMaxMode(newState);
    if (m_agent)
        m_agent->SetMaxMode(newState);
    if (m_hwndChkMaxMode)
        SendMessage(m_hwndChkMaxMode, BM_SETCHECK, newState ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput(std::string("Max Mode ") + (newState ? "ENABLED" : "DISABLED") + "\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::onAIModeDeepThink()
{
    LOG_INFO("onAIModeDeepThink toggled");
    bool current = false;
    if (m_agenticBridge)
        current = m_agenticBridge->GetDeepThinking();
    else if (m_hMenu)
        current = (GetMenuState(m_hMenu, IDM_AI_MODE_DEEP_THINK, MF_BYCOMMAND) & MF_CHECKED) != 0;
    bool newState = !current;
    if (m_hMenu)
        CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_THINK, newState ? MF_CHECKED : MF_UNCHECKED);
    if (m_agenticBridge)
        m_agenticBridge->SetDeepThinking(newState);
    if (m_nativeEngine)
        m_nativeEngine->SetDeepThinking(newState);
    if (m_hwndChkDeepThink)
        SendMessage(m_hwndChkDeepThink, BM_SETCHECK, newState ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput(std::string("Deep Thinking (CoT) ") + (newState ? "ENABLED" : "DISABLED") + "\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::onAIModeDeepResearch()
{
    LOG_INFO("onAIModeDeepResearch toggled");
    bool current = false;
    if (m_agenticBridge)
        current = m_agenticBridge->GetDeepResearch();
    else if (m_hMenu)
        current = (GetMenuState(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, MF_BYCOMMAND) & MF_CHECKED) != 0;
    bool newState = !current;
    if (m_hMenu)
        CheckMenuItem(m_hMenu, IDM_AI_MODE_DEEP_RESEARCH, newState ? MF_CHECKED : MF_UNCHECKED);
    if (m_agenticBridge)
        m_agenticBridge->SetDeepResearch(newState);
    if (m_nativeEngine)
        m_nativeEngine->SetDeepResearch(newState);
    if (m_agent)
        m_agent->SetDeepResearch(newState);
    if (m_hwndChkDeepResearch)
        SendMessage(m_hwndChkDeepResearch, BM_SETCHECK, newState ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput(std::string("Deep Research ") + (newState ? "ENABLED" : "DISABLED") + "\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::onAIModeNoRefusal()
{
    LOG_INFO("onAIModeNoRefusal toggled");
    bool current = false;
    if (m_agenticBridge)
        current = m_agenticBridge->GetNoRefusal();
    else if (m_hMenu)
        current = (GetMenuState(m_hMenu, IDM_AI_MODE_NO_REFUSAL, MF_BYCOMMAND) & MF_CHECKED) != 0;
    bool newState = !current;
    if (m_hMenu)
        CheckMenuItem(m_hMenu, IDM_AI_MODE_NO_REFUSAL, newState ? MF_CHECKED : MF_UNCHECKED);
    if (m_agenticBridge)
        m_agenticBridge->SetNoRefusal(newState);
    if (m_agent)
        m_agent->SetNoRefusal(newState);
    if (m_hwndChkNoRefusal)
        SendMessage(m_hwndChkNoRefusal, BM_SETCHECK, newState ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput(std::string("No Refusal Mode ") + (newState ? "ENABLED" : "DISABLED") + "\n", "Output",
                   OutputSeverity::Info);
}

// ============================================================================
// AGENTIC PLANNING ORCHESTRATOR — Full Approval Gate Handlers
// ============================================================================

void Win32IDE::onPlanningStart()
{
    LOG_INFO("onPlanningStart called");
    auto& orch = Agentic::OrchestratorIntegration::instance();
    if (!orch.getOrchestrator())
    {
        orch.initialize();
    }

    char taskDesc[1024] = {0};
    if (DialogBoxParamA(
            m_hInstance, "AGENT_PROMPT_DLG", m_hwndMain,
            [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> INT_PTR
            {
                switch (msg)
                {
                    case WM_INITDIALOG:
                        SetWindowTextA(GetDlgItem(hwnd, 101), "Enter task to plan:");
                        return TRUE;
                    case WM_COMMAND:
                        if (LOWORD(wp) == IDOK)
                        {
                            GetDlgItemTextA(hwnd, 102, (char*)lp, 1024);
                            EndDialog(hwnd, IDOK);
                            return TRUE;
                        }
                        else if (LOWORD(wp) == IDCANCEL)
                        {
                            EndDialog(hwnd, IDCANCEL);
                            return TRUE;
                        }
                        break;
                }
                return FALSE;
            },
            (LPARAM)taskDesc) != IDOK ||
        strlen(taskDesc) == 0)
    {
        return;
    }

    auto* plan = orch.planAndApproveTask(std::string(taskDesc));
    if (!plan)
    {
        appendToOutput("Failed to generate plan\n", "Errors", OutputSeverity::Error);
        return;
    }

    appendToOutput("Plan created: " + plan->plan_id + " (" + std::to_string(plan->steps.size()) + " steps, " +
                       std::to_string(plan->pending_approvals) + " pending approvals)\n",
                   "Output", OutputSeverity::Info);
    appendToOutput(plan->toSummary(), "Output", OutputSeverity::Info);
}

void Win32IDE::onPlanningShowQueue()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    auto pending = orc->getPendingApprovals();
    if (pending.empty())
    {
        appendToOutput("No pending approvals\n", "Output", OutputSeverity::Info);
        return;
    }

    std::stringstream ss;
    ss << "=== Pending Approvals (" << pending.size() << ") ===\n";
    for (const auto& [plan, idx] : pending)
    {
        if (idx >= 0 && idx < static_cast<int>(plan->steps.size()))
        {
            const auto& step = plan->steps[idx];
            ss << "  [" << plan->plan_id << "] Step " << idx << ": " << step.title
               << " (risk=" << static_cast<int>(step.risk_level) << ")\n";
        }
    }
    appendToOutput(ss.str(), "Output", OutputSeverity::Info);
}

void Win32IDE::onPlanningApproveStep()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    auto pending = orc->getPendingApprovals();
    if (pending.empty())
    {
        appendToOutput("No steps pending approval\n", "Output", OutputSeverity::Info);
        return;
    }

    // Approve the first pending step
    auto [plan, idx] = pending[0];
    orc->approveStep(plan, idx, "user", "Manually approved via IDE");
    appendToOutput("Approved: " + plan->steps[idx].title + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onPlanningRejectStep()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    auto pending = orc->getPendingApprovals();
    if (pending.empty())
    {
        appendToOutput("No steps pending rejection\n", "Output", OutputSeverity::Info);
        return;
    }

    auto [plan, idx] = pending[0];
    orc->rejectStep(plan, idx, "user", "Manually rejected via IDE");
    appendToOutput("Rejected: " + plan->steps[idx].title + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onPlanningExecuteStep()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    auto plans = orc->getActivePlans();
    if (plans.empty())
    {
        appendToOutput("No active plans\n", "Output", OutputSeverity::Info);
        return;
    }

    auto* plan = plans[0];
    if (orc->executeNextApprovedStep(plan))
    {
        appendToOutput("Executed next approved step in plan " + plan->plan_id + "\n", "Output", OutputSeverity::Info);
    }
    else
    {
        appendToOutput("No approved steps ready to execute\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::onPlanningExecuteAll()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    auto plans = orc->getActivePlans();
    if (plans.empty())
    {
        appendToOutput("No active plans\n", "Output", OutputSeverity::Info);
        return;
    }

    auto* plan = plans[0];
    appendToOutput("Executing entire plan " + plan->plan_id + "...\n", "Output", OutputSeverity::Info);

    std::thread(
        [this, orc, plan]()
        {
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled)
                return;
            orc->executeEntirePlan(plan);
            PostMessage(m_hwndMain, WM_APP + 200, 0, 0);
        })
        .detach();
}

void Win32IDE::onPlanningRollback()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    auto plans = orc->getActivePlans();
    if (plans.empty())
    {
        appendToOutput("No active plans\n", "Output", OutputSeverity::Info);
        return;
    }

    auto* plan = plans[0];
    // Rollback last executed step
    for (int i = static_cast<int>(plan->steps.size()) - 1; i >= 0; --i)
    {
        if (plan->steps[i].status == Agentic::ExecutionStatus::Success ||
            plan->steps[i].status == Agentic::ExecutionStatus::Failed)
        {
            orc->rollbackStep(plan, i);
            appendToOutput("Rolled back step: " + plan->steps[i].title + "\n", "Output", OutputSeverity::Info);
            return;
        }
    }
    appendToOutput("No steps to rollback\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onPlanningSetPolicy()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    int choice = MessageBoxA(m_hwndMain,
                             "Select approval policy:\n\n"
                             "YES = Conservative (approve almost nothing)\n"
                             "NO = Standard (auto-approve VeryLow only)\n"
                             "CANCEL = Aggressive (auto-approve Low + VeryLow)",
                             "Set Approval Policy", MB_YESNOCANCEL | MB_ICONQUESTION);

    if (choice == IDYES)
    {
        orc->setApprovalPolicy(Agentic::ApprovalPolicy::Conservative());
        appendToOutput("Policy set to: Conservative\n", "Output", OutputSeverity::Info);
    }
    else if (choice == IDNO)
    {
        orc->setApprovalPolicy(Agentic::ApprovalPolicy::Standard());
        appendToOutput("Policy set to: Standard\n", "Output", OutputSeverity::Info);
    }
    else
    {
        orc->setApprovalPolicy(Agentic::ApprovalPolicy::Aggressive());
        appendToOutput("Policy set to: Aggressive\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::onPlanningViewStatus()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    auto status = orc->getExecutionStatusJson();
    appendToOutput("=== Planning Orchestrator Status ===\n" + status.dump(2) + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onPlanningDiagnostics()
{
    auto* orc = AGENTIC_GET_ORCHESTRATOR();
    if (!orc)
    {
        appendToOutput("Orchestrator not initialized\n", "Output", OutputSeverity::Warning);
        return;
    }

    std::stringstream ss;
    ss << "=== Planning Orchestrator Diagnostics ===\n";

    auto policy = orc->getApprovalPolicy();
    ss << "Policy: auto_verylow=" << policy.auto_approve_very_low_risk << " auto_low=" << policy.auto_approve_low_risk
       << " partial_exec=" << policy.allow_partial_execution << "\n";

    auto plans = orc->getActivePlans();
    ss << "Active plans: " << plans.size() << "\n";
    ss << "Pending approvals: " << orc->getPendingApprovalCount() << "\n";

    for (const auto* plan : plans)
    {
        ss << "\n--- Plan: " << plan->plan_id << " ---\n";
        ss << plan->toSummary();
    }

    auto queue = orc->getApprovalQueueJson();
    if (!queue.empty())
    {
        ss << "\nApproval Queue:\n" << queue.dump(2) << "\n";
    }

    appendToOutput(ss.str(), "Output", OutputSeverity::Info);
}

// ============================================================================
// AGENTIC MODE SWITCHER — Plan / Agent / Ask (three-mode chat behavior)
// ============================================================================

void Win32IDE::setAgenticMode(RawrXD::AgenticMode mode)
{
    m_agenticMode = mode;
    LOG_INFO("Agentic mode set to " + std::string(RawrXD::AgenticModeToString(mode)));
    if (m_hwndAgenticModeAsk)
        SendMessage(m_hwndAgenticModeAsk, BM_SETCHECK, (mode == RawrXD::AgenticMode::Ask) ? BST_CHECKED : BST_UNCHECKED,
                    0);
    if (m_hwndAgenticModePlan)
        SendMessage(m_hwndAgenticModePlan, BM_SETCHECK,
                    (mode == RawrXD::AgenticMode::Plan) ? BST_CHECKED : BST_UNCHECKED, 0);
    if (m_hwndAgenticModeAgent)
        SendMessage(m_hwndAgenticModeAgent, BM_SETCHECK,
                    (mode == RawrXD::AgenticMode::Agent) ? BST_CHECKED : BST_UNCHECKED, 0);
    appendToOutput("Chat mode: " + std::string(RawrXD::AgenticModeToString(mode)) + "\n", "Output",
                   OutputSeverity::Info);
}

void Win32IDE::onAgenticModeChanged(RawrXD::AgenticMode mode)
{
    setAgenticMode(mode);
}

void Win32IDE::onAgenticModeAsk()
{
    setAgenticMode(RawrXD::AgenticMode::Ask);
}

void Win32IDE::onAgenticModePlan()
{
    setAgenticMode(RawrXD::AgenticMode::Plan);
}

void Win32IDE::onAgenticModeAgent()
{
    setAgenticMode(RawrXD::AgenticMode::Agent);
}

void Win32IDE::onAIContextSize(int sizeEnum)
{
    LOG_INFO("onAIContextSize: " + std::to_string(sizeEnum));

    // Map enum value to token count
    static const struct
    {
        int menuId;
        int tokens;
        const char* label;
    } contextMap[] = {
        {IDM_AI_CONTEXT_4K, 4096, "4K"},       {IDM_AI_CONTEXT_32K, 32768, "32K"},
        {IDM_AI_CONTEXT_64K, 65536, "64K"},    {IDM_AI_CONTEXT_128K, 131072, "128K"},
        {IDM_AI_CONTEXT_256K, 262144, "256K"}, {IDM_AI_CONTEXT_512K, 524288, "512K"},
        {IDM_AI_CONTEXT_1M, 1048576, "1M"},
    };

    int tokens = 4096;
    std::string label = "4K";
    for (const auto& entry : contextMap)
    {
        if (entry.menuId == sizeEnum)
        {
            tokens = entry.tokens;
            label = entry.label;
            break;
        }
    }

    m_inferenceConfig.contextWindow = tokens;
    m_currentContextSize = static_cast<size_t>(tokens);

    if (m_agenticBridge)
    {
        std::string sizeStr = label;
        std::transform(sizeStr.begin(), sizeStr.end(), sizeStr.begin(), ::tolower);
        m_agenticBridge->SetContextSize(sizeStr);
    }
    if (m_nativeEngine)
        m_nativeEngine->SetContextSize(static_cast<size_t>(tokens));

    // Update menu checkmarks
    for (const auto& entry : contextMap)
    {
        CheckMenuItem(m_hMenu, entry.menuId, (entry.menuId == sizeEnum) ? MF_CHECKED : MF_UNCHECKED);
    }

    updateContextSliderLabel();
    appendToOutput("Context window set to " + label + " (" + std::to_string(tokens) + " tokens)\n", "Output",
                   OutputSeverity::Info);
}

// ============================================================================
// AUTONOMY INITIALIZATION
// ============================================================================
void Win32IDE::initializeAutonomy()
{
    LOG_INFO("Initializing Autonomy Manager");

    if (!m_agenticBridge)
    {
        appendToOutput("⚠️ Cannot initialize autonomy: Agentic Bridge not ready\n", "Output", OutputSeverity::Warning);
        return;
    }

    if (!m_autonomyManager)
    {
        m_autonomyManager = std::make_unique<AutonomyManager>(m_agenticBridge);
    }

    appendToOutput("✅ Autonomy Manager initialized\n", "Output", OutputSeverity::Info);
}

// ============================================================================
// MEMORY PLUGIN SYSTEM
// ============================================================================
void Win32IDE::loadMemoryPlugin(const std::string& path)
{
    LOG_INFO("loadMemoryPlugin: " + path);

    if (path.empty())
    {
        appendToOutput("⚠️ Empty plugin path\n", "Output", OutputSeverity::Warning);
        return;
    }

    // Attempt to load the plugin DLL and register with the native engine
    // Enforce signature policy for native plugins before LoadLibrary.
    {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.length(), NULL, 0);
        std::wstring wPath(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), (int)path.length(), &wPath[0], size_needed);

        if (!verifyPluginBeforeLoad(wPath.c_str()))
        {
            appendToOutput("❌ Plugin blocked by signature policy: " + path + "\n", "Security", OutputSeverity::Error);
            return;
        }
    }

    HMODULE hPlugin = LoadLibraryA(path.c_str());
    if (!hPlugin)
    {
        DWORD err = GetLastError();
        appendToOutput("❌ Failed to load memory plugin: " + path + " (error " + std::to_string(err) + ")\n", "Errors",
                       OutputSeverity::Error);
        return;
    }

    // Look for the standard plugin factory export
    using CreatePluginFn = RawrXD::IMemoryPlugin* (*)();
    auto createFn = (CreatePluginFn)GetProcAddress(hPlugin, "CreateMemoryPlugin");
    if (!createFn)
    {
        appendToOutput("❌ Plugin missing CreateMemoryPlugin export: " + path + "\n", "Errors", OutputSeverity::Error);
        FreeLibrary(hPlugin);
        return;
    }

    auto* rawPlugin = createFn();
    if (rawPlugin && m_nativeEngine)
    {
        m_nativeEngine->RegisterMemoryPlugin(std::shared_ptr<RawrXD::IMemoryPlugin>(rawPlugin));
        appendToOutput("✅ Memory plugin loaded: " + path + "\n", "Output", OutputSeverity::Info);
    }
    else
    {
        appendToOutput("⚠️ Plugin created but no native engine available\n", "Output", OutputSeverity::Warning);
        delete rawPlugin;
        FreeLibrary(hPlugin);
    }
}
