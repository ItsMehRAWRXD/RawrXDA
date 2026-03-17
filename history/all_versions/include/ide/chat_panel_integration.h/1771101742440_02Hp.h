// chat_panel_integration.h — Production chat system for IDE (C++20/Win32)
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <windows.h>

namespace rawrxd::ide {

// Chat message structure
struct ChatMessage {
    std::string sender;  // "user", "assistant", "system"
    std::string content;
    std::string timestamp;
    std::string messageId;
};

// Chat context for conversation history
struct ChatContext {
    std::string sessionId;
    std::vector<ChatMessage> history;
    std::string currentModel;
    std::string currentProvider;  // "github-copilot", "amazonq", "local-agent"
};

// Agent information for autonomous operation
struct AgentInfo {
    std::string name;
    std::string description;
    std::string capabilities;
    bool autonomous;
    std::string version;
};

// Callback types
using ChatMessageCallback = std::function<void(const ChatMessage& msg)>;
using ChatErrorCallback = std::function<void(const std::string& error)>;
using AgentStateCallback = std::function<void(const AgentInfo& agent, bool active)>;
using FileSelectCallback = std::function<void(const std::string& filePath)>;

/**
 * @class ChatPanelIntegration
 * @brief Complete chat system integration with API backends
 *
 * Manages:
 * - GitHub Copilot chat (if extension installed)
 * - Amazon Q chat (if extension installed)
 * - Local agentic AI system
 * - Conversation history and context
 * - Agent autonomous mode control
 * - File explorer integration with chat context
 */
class ChatPanelIntegration {
public:
    ChatPanelIntegration(void* vsixLoader = nullptr); // Avoid circular dependency in header
    ~ChatPanelIntegration();

    // ========================================================================
    // Chat Interface
    // ========================================================================

    /**
     * Send message to current chat provider
     * @param message User message text
     * @param context Optional file/project context
     */
    void sendMessage(const std::string& message, const std::string& context = "");

    /**
     * Receive and display assistant message
     * Automatically called by backend
     */
    void receiveMessage(const ChatMessage& message);

    /**
     * Clear conversation history
     */
    void clearHistory();

    /**
     * Get conversation history
     */
    std::vector<ChatMessage> getHistory() const { return m_context.history; }

    // ========================================================================
    // Provider Selection and Management
    // ========================================================================

    /**
     * Switch between chat providers
     * @param provider "github-copilot", "amazonq", "local-agent"
     * @return true if provider available and switched
     */
    bool switchProvider(const std::string& provider);

    /**
     * Get currently active provider
     */
    std::string getCurrentProvider() const { return m_context.currentProvider; }

    /**
     * Get available chat providers
     */
    std::vector<std::string> getAvailableProviders() const;

    // ========================================================================
    // Agent Integration
    // ========================================================================

    /**
     * Set active agent for autonomous operation
     * @param agentName Name of agent to activate
     * @return true if agent activated
     */
    bool activateAgent(const std::string& agentName);

    /**
     * Deactivate current agent
     */
    void deactivateAgent();

    /**
     * Get currently active agent
     */
    std::string getActiveAgent() const { return m_activeAgent; }

    /**
     * Register custom agent
     */
    void registerAgent(const AgentInfo& agent);

    /**
     * List available agents
     */
    std::vector<AgentInfo> getAvailableAgents() const;

    /**
     * Enable/disable autonomous mode for agent
     */
    void setAutonomousMode(bool enabled) { m_autonomousMode = enabled; }
    bool isAutonomousMode() const { return m_autonomousMode; }

    // ========================================================================
    // File Explorer Integration
    // ========================================================================

    /**
     * Set currently selected file in editor (for context)
     */
    void setSelectedFile(const std::string& filePath);

    /**
     * Get currently selected file
     */
    std::string getSelectedFile() const { return m_selectedFile; }

    /**
     * Set current project root (for context)
     */
    void setProjectRoot(const std::string& rootPath) { m_projectRoot = rootPath; }

    /**
     * Inject file contents into chat context
     */
    void injectFileContext(const std::string& filePath);

    /**
     * Inject project outline into context
     */
    void injectProjectContext(const std::string& rootPath);

    // ========================================================================
    // Callback Management
    // ========================================================================

    void setOnMessage(ChatMessageCallback cb) { m_onMessage = cb; }
    void setOnError(ChatErrorCallback cb) { m_onError = cb; }
    void setOnAgentStateChange(AgentStateCallback cb) { m_onAgentState = cb; }
    void setOnFileSelected(FileSelectCallback cb) { m_onFileSelected = cb; }

    // ========================================================================
    // GitHub Chat Specific
    // ========================================================================

    /**
     * Invoke GitHub Copilot Chat
     * If extension is installed, uses native API
     * Otherwise proxies through local agent
     */
    void invokeGithubChat();

    /**
     * Submit slash command to GitHub Copilot
     * Examples: /explain, /fix, /test, /document
     */
    void executeSlashCommand(const std::string& command, const std::string& target);

    // ========================================================================
    // Window Management (for UI)
    // ========================================================================

    /**
     * Create Win32 chat panel widget
     * @param hwndParent Parent window handle
     * @return Handle to new chat panel window
     */
    HWND createPanel(HWND hwndParent);

    /**
     * Show/hide the chat panel
     */
    void showPanel(bool visible);

    /**
     * Destroy the chat panel
     */
    void destroyPanel();

private:
    // State
    ChatContext m_context;
    std::string m_activeAgent;
    bool m_autonomousMode = false;
    std::string m_selectedFile;
    std::string m_projectRoot;

    // Available extensions
    std::map<std::string, bool> m_providers;  // name -> is_available
    std::map<std::string, AgentInfo> m_agents;

    // UI
    HWND m_hwndPanel = nullptr;
    HWND m_hwndInput = nullptr;
    HWND m_hwndHistory = nullptr;
    HWND m_hwndAgentControl = nullptr;

    // Callbacks
    ChatMessageCallback m_onMessage;
    ChatErrorCallback m_onError;
    AgentStateCallback m_onAgentState;
    FileSelectCallback m_onFileSelected;

    // Internal methods
    void initializeProviders();
    void initializeAgents();
    std::string callGithubCopilotAPI(const std::string& message);
    std::string callAmazonQAPI(const std::string& message);
    std::string callLocalAgentAPI(const std::string& message);
    void processTokenStream(const std::string& tokens);
};

}  // namespace rawrxd::ide
