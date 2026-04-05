#pragma once

#include <windows.h>

// Undefine Windows ERROR macro
#ifdef ERROR
#undef ERROR
#endif

#include "../cpu_inference_engine.h"
#include "../native_agent.hpp"
#include "Win32IDE_SubAgent.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declaration
class Win32IDE;

// Agent response types
enum class AgentResponseType
{
    TOOL_CALL,
    ANSWER,
    AGENT_ERROR,
    THINKING
};  // Agent response structure
struct AgentResponse
{
    AgentResponseType type;
    std::string content;
    std::string toolName;
    std::string toolArgs;
    std::string rawOutput;
};

// Agentic Framework Bridge for Win32IDE
// Integrates PowerShell-based agentic framework with C++ IDE
class AgenticBridge
{
  public:
  enum class UILogSeverity : uint8_t
  {
    Info = 0,
    Warning = 1,
    Error = 2,
  };

    AgenticBridge(Win32IDE* ide);
    ~AgenticBridge();

    // Core agent operations
    bool Initialize(const std::string& frameworkPath, const std::string& modelName = "");
    bool IsInitialized() const { return m_initialized; }
    /// True when local weights are mapped or a remote/orchestrator path is configured (stricter than IsInitialized).
    bool HasUsableBackend() const;

    // Execute single agent command
    AgentResponse ExecuteAgentCommand(const std::string& prompt);

    // Start multi-turn agent loop
    bool StartAgentLoop(const std::string& initialPrompt, int maxIterations = 10);
    void StopAgentLoop();
    bool IsAgentLoopRunning() const { return m_agentLoopRunning; }

    // Get agent capabilities
    std::vector<std::string> GetAvailableTools();
    std::string GetAgentStatus();

    // Configuration
    void SetModel(const std::string& modelName);
    void SetOllamaServer(const std::string& serverUrl);
    void SetMaxMode(bool enabled);
    bool GetMaxMode() const { return m_maxMode; }
    void SetDeepThinking(bool enabled);
    bool GetDeepThinking() const { return m_deepThinking; }
    void SetDeepResearch(bool enabled);
    bool GetDeepResearch() const { return m_deepResearch; }
    void SetNoRefusal(bool enabled);
    bool GetNoRefusal() const { return m_noRefusal; }
    void SetSwarmMode(bool enabled);
    bool GetSwarmMode() const { return m_swarmMode; }
    bool LoadSwarmFromDirectory(const std::string& directoryPath, int maxModels = 5);
    const std::string& GetLastLoadError() const { return m_lastModelLoadError; }
    void SetAutoCorrect(bool enabled);
    bool GetAutoCorrect() const { return m_autoCorrect; }

    /// Prompt-layer hotpatch: inject SubAgent/tool syntax so local or tool-naive models still emit dispatchable lines.
    void SetHotpatchSubAgentToolProtocol(bool enabled);
    bool GetHotpatchSubAgentToolProtocol() const { return m_hotpatchSubAgentToolProtocol; }
    /// Prompt-layer hotpatch: require <thought>...</thought> when Deep Thinking is on (models without native CoT).
    void SetHotpatchThoughtProtocol(bool enabled);
    bool GetHotpatchThoughtProtocol() const { return m_hotpatchThoughtProtocol; }

    void SetContextSize(const std::string& sizeName);
    bool LoadModel(const std::string& path);
    /** Forwards to CPUInferenceEngine::GetSharedInstance() (same object as Win32IDE::m_nativeEngine). */
    void SetCpuEngineLayerProgressCallback(std::function<void(const std::string&)> cb);
    void SetCpuEngineSwarmTelemetryOutputCallback(std::function<void(const std::string&)> cb);
    std::string GetCurrentModel() const { return m_modelName; }

    // Language-aware context propagation
    void SetLanguageContext(const std::string& language, const std::string& filePath);
    std::string GetLanguageContext() const { return m_languageContext; }
    std::string GetFileContext() const { return m_fileContext; }

    // Workspace root for agent context (project folder / drive)
    void SetWorkspaceRoot(const std::string& workspaceRoot);
    std::string GetWorkspaceRoot() const { return m_workspaceRoot; }

    // Ghost stream telemetry (monotonic sequence auditing)
    void ObserveGhostStreamSeq(uint64_t seq);
    uint64_t GetLastGhostSeq() const;
    uint64_t GetGhostSeqBacktracks() const;
    uint64_t GetGhostSeqGapEvents() const;
    void ResetGhostSeqTelemetry();

    // Output callback
    using OutputCallback = std::function<void(const std::string&, const std::string&)>;
    void SetOutputCallback(OutputCallback callback);

    // Compatibility callbacks used by Win32IDE_AgentCommands.cpp
    using ErrorCallback = std::function<void(const std::string&)>;
    using ProgressCallback = std::function<void(const std::string&)>;
    using ModelLoadErrorCallback = std::function<void(const std::string&)>;
    void SetErrorCallback(ErrorCallback cb) { m_errorCallback = std::move(cb); }
    void SetProgressCallback(ProgressCallback cb) { m_progressCallback = std::move(cb); }
    void SetModelLoadErrorCallback(ModelLoadErrorCallback cb) { m_modelLoadErrorCallback = std::move(cb); }
    const std::string& GetLastModelLoadError() const { return m_lastModelLoadError; }

    // Output panel integration
    void SetMainWindow(HWND hwnd);

    // RE Tools Access
    std::string RunDumpbin(const std::string& path, const std::string& mode);
    std::string RunCodex(const std::string& path);
    std::string RunCompiler(const std::string& path);

    // ---- SubAgent / Chaining / HexMag Swarm ----

    /// Access the SubAgentManager (lazy-initialized)
    SubAgentManager* GetSubAgentManager();

    /// Spawn a sub-agent from user/model request
    std::string RunSubAgent(const std::string& description, const std::string& prompt);

    /// Execute a sequential chain of prompts
    std::string ExecuteChain(const std::vector<std::string>& steps, const std::string& initialInput = "");

    /// Execute a HexMag swarm (parallel fan-out with merge)
    std::string ExecuteSwarm(const std::vector<std::string>& prompts, const std::string& mergeStrategy = "concatenate",
                             int maxParallel = 4);

    /// Cancel all running sub-agents
    void CancelAllSubAgents();

    /// Get sub-agent status summary
    std::string GetSubAgentStatus() const;

    /// Dispatch tool calls detected in model output
    bool DispatchModelToolCalls(const std::string& modelOutput, std::string& toolResult);

    // Compatibility wrappers (older UI command layer)
    void ExecuteSubAgentChain(const std::string& taskDescription);
    void ExecuteSubAgentSwarm(const std::string& taskDescription);
    std::vector<std::string> GetSubAgentTodoList();
    void ClearSubAgentTodoList();
    std::string ExportAgentMemory();
    void ClearAgentMemory();
    void ExecuteBoundedAgentLoop(const std::string& prompt, int maxIterations);
    bool LoadConfiguration(const std::string& configPath);
    void EnableMultiAgent(bool enabled) { m_multiAgentEnabled = enabled; }
    void WarmUpModel();

  private:
    // Native Integration
    std::shared_ptr<RawrXD::CPUInferenceEngine> m_nativeEngine;
    std::unique_ptr<RawrXD::NativeAgent> m_nativeAgent;

    // SubAgent Manager (lazy-initialized)
    std::unique_ptr<SubAgentManager> m_subAgentManager;

    // PowerShell process management
    bool SpawnPowerShellProcess(const std::string& scriptPath, const std::string& arguments);
    bool ReadProcessOutput(std::string& output, DWORD timeoutMs = 5000);
    void KillPowerShellProcess();

    // Response parsing
    AgentResponse ParseAgentResponse(const std::string& rawOutput);
    bool IsToolCall(const std::string& line);
    bool IsAnswer(const std::string& line);
    std::string BuildOpenTabsPromptContext() const;

    // Path resolution
    std::string ResolveFrameworkPath();
    std::string ResolveToolsModulePath();
    
    // Tool implementation helpers
    std::string RunCompilerImpl(const std::string& path, const std::string& arch);

    /// Pre-inference prompt augmentation (SubAgent + thought protocols).
    void applyAgentCapabilityHotpatches(std::string& refinedPrompt);
    bool postLogToMainWindow(UILogSeverity severity, const std::string& message) const;

    Win32IDE* m_ide;
    bool m_initialized;
    bool m_agentLoopRunning;

    std::string m_frameworkPath;
    std::string m_toolsModulePath;
    std::string m_modelName;
    std::string m_ollamaServer;

    HANDLE m_hProcess;
    HANDLE m_hStdoutRead;
    HANDLE m_hStdoutWrite;
    HANDLE m_hStdinRead;
    HANDLE m_hStdinWrite;

    // Config Cache
    // Default ON: agent chat panel should ship with full reasoning modes enabled (user can toggle off).
    bool m_maxMode = true;
    bool m_deepThinking = true;
    bool m_deepResearch = true;
    bool m_noRefusal = true;
    bool m_autoCorrect = false;
    /// Default ON: inject tool/subagent line format for models without native function-calling.
    bool m_hotpatchSubAgentToolProtocol = true;
    /// Default ON: inject <thought> scaffolding when Deep Thinking is enabled.
    bool m_hotpatchThoughtProtocol = true;
    std::string m_languageContext;  // Current language (e.g. "C/C++")
    std::string m_fileContext;      // Current file path
    std::string m_workspaceRoot;    // Project/workspace folder for agent context

    // Output callback for streaming results to UI
    OutputCallback m_outputCallback;
    ErrorCallback m_errorCallback;
    ProgressCallback m_progressCallback;
    ModelLoadErrorCallback m_modelLoadErrorCallback;
    std::string m_lastModelLoadError;
    bool m_multiAgentEnabled = false;
    bool m_swarmMode = false;
    std::atomic<uint64_t> m_lastGhostSeq{0};
    std::atomic<uint64_t> m_ghostSeqBacktracks{0};
    std::atomic<uint64_t> m_ghostSeqGapEvents{0};
    HWND m_hwndMain = nullptr;
};
