// ollama_model_provider.h - Unified Ollama model provider plugin
#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <windows.h>

namespace RawrXD::Extensions::Ollama {

// Model types
enum class ModelType {
    LocalOllama,
    CloudAPI,
    Embedded
};

// Model capability flags
enum ModelCapability {
    CAP_AGENT = 1 << 0,
    CAP_ASK    = 1 << 1,
    CAP_PLAN   = 1 << 2,
    CAP_MAX    = 1 << 3,  // Combined agent/ask/plan mode
    CAP_MULTI_FILE = 1 << 4,  // Multi-file operations
    CAP_CODEBASE = 1 << 5,    // Codebase-wide operations
    CAP_AUTONOMOUS = 1 << 6,  // Autonomous agent capabilities
    CAP_VISION = 1 << 7,      // Visual/context understanding
    CAP_TOOL_USE = 1 << 8,    // Tool calling and execution
    CAP_MEMORY = 1 << 9       // Persistent memory and context
};

// Model information
struct ModelInfo {
    std::string id;
    std::string name;
    std::string displayName;
    std::string endpoint;
    ModelType type;
    uint32_t capabilities;
    std::string description;
    size_t contextSize;
    bool available;
    bool requiresInternet;
};

// Chat window configuration
struct ChatWindowConfig {
    std::string title;
    ModelInfo initialModel;
    uint32_t initialMode; // CAP_AGENT, CAP_ASK, CAP_PLAN, CAP_MAX
    bool showModelDropdown;
    bool showModeSelector;
    bool enableMultiFile;
    bool enableAutonomous;
    bool enableToolUse;
};

// Multi-file operation request
struct MultiFileRequest {
    std::vector<std::string> filePaths;
    std::string operation; // "analyze", "refactor", "document", "test"
    std::string instruction;
    std::string context;   // Additional context for the operation
};

// Autonomous agent task
struct AutonomousTask {
    std::string goal;
    std::vector<std::string> constraints;
    std::vector<std::string> availableTools;
    uint32_t maxSteps;
    bool allowRecursion;
};

// Tool execution result
struct ToolExecutionResult {
    std::string toolName;
    std::string parameters;
    std::string output;
    bool success;
    std::string error;
};

// Agentic session state
struct AgenticSession {
    std::string sessionId;
    std::vector<std::string> contextFiles;
    std::vector<ToolExecutionResult> toolHistory;
    std::string currentGoal;
    uint32_t stepsTaken;
    bool completed;
};

// Plugin interface
class OllamaModelProvider {
public:
    virtual ~OllamaModelProvider() = default;

    // Initialization
    virtual bool Initialize(const std::string& config) = 0;
    virtual void Shutdown() = 0;

    // Model discovery
    virtual std::vector<ModelInfo> DiscoverModels(bool forceRefresh = false) = 0;
    virtual ModelInfo GetCurrentModel() const = 0;
    virtual bool SetCurrentModel(const std::string& modelId) = 0;

    // Chat management
    virtual HWND CreateChatWindow(const ChatWindowConfig& config) = 0;
    virtual bool CloseChatWindow(HWND hwnd) = 0;
    virtual std::vector<HWND> GetActiveChatWindows() const = 0;

    // Mode management
    virtual uint32_t GetCurrentMode(HWND chatWindow) const = 0;
    virtual bool SetCurrentMode(HWND chatWindow, uint32_t mode) = 0;

    // Advanced agentic capabilities
    virtual std::string ProcessMultiFileOperation(const MultiFileRequest& request) = 0;
    virtual AgenticSession StartAutonomousTask(const AutonomousTask& task) = 0;
    virtual bool StopAutonomousTask(const std::string& sessionId) = 0;
    virtual AgenticSession GetAutonomousSession(const std::string& sessionId) const = 0;
    virtual std::vector<AgenticSession> GetActiveSessions() const = 0;
    
    // Tool execution
    virtual ToolExecutionResult ExecuteTool(const std::string& toolName, 
                                          const std::string& parameters) = 0;
    virtual std::vector<std::string> GetAvailableTools() const = 0;
    
    // Memory and context management
    virtual void AddToContext(const std::string& key, const std::string& value) = 0;
    virtual std::string GetFromContext(const std::string& key) const = 0;
    virtual void ClearContext() = 0;
    
    // Codebase operations
    virtual std::string AnalyzeCodebase(const std::string& path, 
                                      const std::string& analysisType) = 0;
    virtual std::string GenerateDocumentation(const std::vector<std::string>& files) = 0;
    virtual std::string RefactorCode(const std::vector<std::string>& files, 
                                   const std::string& refactoringPattern) = 0;

    // Event callbacks
    using ModelChangedCallback = std::function<void(const ModelInfo&)>;
    using ModeChangedCallback = std::function<void(HWND, uint32_t)>;
    using ChatCreatedCallback = std::function<void(HWND)>;
    using ChatClosedCallback = std::function<void(HWND)>;

    virtual void SetModelChangedCallback(ModelChangedCallback cb) = 0;
    virtual void SetModeChangedCallback(ModeChangedCallback cb) = 0;
    virtual void SetChatCreatedCallback(ChatCreatedCallback cb) = 0;
    virtual void SetChatClosedCallback(ChatClosedCallback cb) = 0;

    // Status
    virtual bool IsConnected() const = 0;
    virtual std::string GetStatus() const = 0;
};

// Plugin exports
extern "C" {
    __declspec(dllexport) OllamaModelProvider* CreateOllamaProvider();
    __declspec(dllexport) void DestroyOllamaProvider(OllamaModelProvider* provider);
    __declspec(dllexport) const char* GetPluginVersion();
}

} // namespace RawrXD::Extensions::Ollama
