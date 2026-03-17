#pragma once

#include <windows.h>

// Undefine Windows ERROR macro
#ifdef ERROR
#undef ERROR
#endif

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "../native_agent.hpp"
#include "../cpu_inference_engine.h"

// Forward declaration
class Win32IDE;

// Agent response types
enum class AgentResponseType {
    TOOL_CALL,
    ANSWER,
    AGENT_ERROR,
    THINKING
};// Agent response structure
struct AgentResponse {
    AgentResponseType type;
    std::string content;
    std::string toolName;
    std::string toolArgs;
    std::string rawOutput;
};

// Agentic Framework Bridge for Win32IDE
// Integrates PowerShell-based agentic framework with C++ IDE
class AgenticBridge {
public:
    AgenticBridge(Win32IDE* ide);
    ~AgenticBridge();

    // Core agent operations
    bool Initialize(const std::string& frameworkPath, const std::string& modelName = "");
    bool IsInitialized() const { return m_initialized; }
    
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
    void SetAutoCorrect(bool enabled);
    bool GetAutoCorrect() const { return m_autoCorrect; }
    void SetContextSize(const std::string& sizeName);
    bool LoadModel(const std::string& path);
    std::string GetCurrentModel() const { return m_modelName; }
    
    // Output callback
    using OutputCallback = std::function<void(const std::string&, const std::string&)>;
    void SetOutputCallback(OutputCallback callback);

    // RE Tools Access
    std::string RunDumpbin(const std::string& path, const std::string& mode);
    std::string RunCodex(const std::string& path);
    std::string RunCompiler(const std::string& path);

private:
   // Native Integration
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_nativeEngine;
    std::unique_ptr<RawrXD::NativeAgent> m_nativeAgent;

    // PowerShell process management
    bool SpawnPowerShellProcess(const std::string& scriptPath, const std::string& arguments);
    bool ReadProcessOutput(std::string& output, DWORD timeoutMs = 5000);
    void KillPowerShellProcess();
    
    // Response parsing
    AgentResponse ParseAgentResponse(const std::string& rawOutput);
    bool IsToolCall(const std::string& line);
    bool IsAnswer(const std::string& line);
    
    // Path resolution
    std::string ResolveFrameworkPath();
    std::string ResolveToolsModulePath();
    
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
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;
    bool m_noRefusal = false;
    bool m_autoCorrect = false;

    // Output callback for streaming results to UI
    OutputCallback m_outputCallback;
};
