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
#include "../agentic_engine.h"
#include "../native_agent.hpp"

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
// Integrates Native C++ Agentic Engine with Win32IDE
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
    void SetOllamaServer(const std::string& serverUrl); // Kept for API interface compatibility but unused internally now
    
    // Advanced Features
    void SetMaxMode(bool enabled);
    void SetDeepThinking(bool enabled);
    void SetDeepResearch(bool enabled);
    void SetNoRefusal(bool enabled);
    void SetAutoCorrect(bool enabled);

    // New: Max tokens
    void SetMaxTokens(int tokens);
    
    bool IsMaxModeEnabled() const { return m_maxMode; }
    bool IsDeepThinkingEnabled() const { return m_deepThinking; }
    bool IsDeepResearchEnabled() const { return m_deepResearch; }
    bool IsNoRefusalEnabled() const { return m_noRefusal; }
    bool IsAutoCorrectEnabled() const { return m_autoCorrect; }

    std::string GetCurrentModel() const { return m_modelName; }
    
    // Output callback
    using OutputCallback = std::function<void(const std::string&, const std::string&)>;
    void SetOutputCallback(OutputCallback callback);

private:
   
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
    
    // Native Engine
    std::shared_ptr<AgenticEngine> m_nativeEngine;
    std::shared_ptr<RawrXD::NativeAgent> m_nativeAgent; // New Native Agent
    
    OutputCallback m_outputCallback;

    // Config Cache
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;
    bool m_noRefusal = false;
    bool m_autoCorrect = false;
};
