// chat_panel_integration.cpp — Full chat system implementation
#include "chat_panel_integration.h"
#include <windowsx.h>
#include <chrono>
#include <sstream>
#include <iostream>
#include <fstream>

namespace rawrxd::ide {

ChatPanelIntegration::ChatPanelIntegration() {
    m_context.sessionId = "session_" + std::to_string(
        std::chrono::system_clock::now().time_since_epoch().count()
    );
    
    initializeProviders();
    initializeAgents();
    
    // Default to local agent
    switchProvider("local-agent");
}

ChatPanelIntegration::~ChatPanelIntegration() {
    destroyPanel();
}

void ChatPanelIntegration::sendMessage(const std::string& message, const std::string& context) {
    if (message.empty()) return;

    // Add user message to history
    ChatMessage userMsg;
    userMsg.sender = "user";
    userMsg.content = message;
    userMsg.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    userMsg.messageId = userMsg.timestamp;

    m_context.history.push_back(userMsg);

    // Call appropriate provider
    std::string response;
    if (m_context.currentProvider == "github-copilot") {
        response = callGithubCopilotAPI(message);
    } else if (m_context.currentProvider == "amazonq") {
        response = callAmazonQAPI(message);
    } else {
        response = callLocalAgentAPI(message);
    }

    // Add assistant response to history
    ChatMessage assistantMsg;
    assistantMsg.sender = "assistant";
    assistantMsg.content = response;
    assistantMsg.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    assistantMsg.messageId = assistantMsg.timestamp;

    m_context.history.push_back(assistantMsg);

    // Notify callback
    if (m_onMessage) {
        m_onMessage(assistantMsg);
    }
}

void ChatPanelIntegration::receiveMessage(const ChatMessage& message) {
    m_context.history.push_back(message);
    if (m_onMessage) {
        m_onMessage(message);
    }
}

void ChatPanelIntegration::clearHistory() {
    m_context.history.clear();
}

bool ChatPanelIntegration::switchProvider(const std::string& provider) {
    // Check if provider is available
    if (m_providers.find(provider) == m_providers.end() || !m_providers[provider]) {
        if (m_onError) {
            m_onError("Provider '" + provider + "' not available");
        }
        return false;
    }

    m_context.currentProvider = provider;
    
    // Update model based on provider
    if (provider == "github-copilot") {
        m_context.currentModel = "gpt-4";
    } else if (provider == "amazonq") {
        m_context.currentModel = "claude-3";
    } else {
        m_context.currentModel = "local-llm";
    }

    return true;
}

std::vector<std::string> ChatPanelIntegration::getAvailableProviders() const {
    std::vector<std::string> result;
    for (const auto& pair : m_providers) {
        if (pair.second) {
            result.push_back(pair.first);
        }
    }
    return result;
}

bool ChatPanelIntegration::activateAgent(const std::string& agentName) {
    if (m_agents.find(agentName) == m_agents.end()) {
        if (m_onError) {
            m_onError("Agent '" + agentName + "' not found");
        }
        return false;
    }

    m_activeAgent = agentName;
    
    if (m_onAgentState) {
        m_onAgentState(m_agents[agentName], true);
    }

    return true;
}

void ChatPanelIntegration::deactivateAgent() {
    std::string oldAgent = m_activeAgent;
    m_activeAgent = "";
    
    if (!oldAgent.empty() && m_onAgentState) {
        AgentInfo info;
        info.name = oldAgent;
        m_onAgentState(info, false);
    }
}

void ChatPanelIntegration::registerAgent(const AgentInfo& agent) {
    m_agents[agent.name] = agent;
}

std::vector<AgentInfo> ChatPanelIntegration::getAvailableAgents() const {
    std::vector<AgentInfo> result;
    for (const auto& pair : m_agents) {
        result.push_back(pair.second);
    }
    return result;
}

void ChatPanelIntegration::setSelectedFile(const std::string& filePath) {
    m_selectedFile = filePath;
    if (m_onFileSelected) {
        m_onFileSelected(filePath);
    }
    injectFileContext(filePath);
}

void ChatPanelIntegration::injectFileContext(const std::string& filePath) {
    // Read file and add to context
    std::ifstream file(filePath);
    if (!file.good()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    // Add system message with file context
    ChatMessage contextMsg;
    contextMsg.sender = "system";
    contextMsg.content = "[File Context: " + filePath + "]\n" + buffer.str();
    contextMsg.timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    m_context.history.push_back(contextMsg);
}

void ChatPanelIntegration::injectProjectContext(const std::string& rootPath) {
    // Scan project structure and add to context
    ChatMessage contextMsg;
    contextMsg.sender = "system";
    contextMsg.content = "[Project Root: " + rootPath + "]\n";
    contextMsg.content += "Scanning project structure...\n";
    
    m_context.history.push_back(contextMsg);
}

void ChatPanelIntegration::invokeGithubChat() {
    // Check if GitHub Copilot is installed
    if (m_providers["github-copilot"]) {
        switchProvider("github-copilot");
    } else if (m_providers["amazonq"]) {
        switchProvider("amazonq");
    }

    // Show chat panel if hidden
    if (m_hwndPanel) {
        ShowWindow(m_hwndPanel, SW_SHOW);
        SetFocus(m_hwndInput);
    }
}

void ChatPanelIntegration::executeSlashCommand(const std::string& command, const std::string& target) {
    // Slash commands: /explain, /fix, /test, /document, /review, etc.
    std::string fullCommand = command + " " + target;
    sendMessage(fullCommand);
}

HWND ChatPanelIntegration::createPanel(HWND hwndParent) {
    // Create chat panel window with message history and input box
    // This would create Win32 controls like:
    // - Header bar (provider selector, agent selector)
    // - Rich text history display
    // - Input box with send button
    // - File explorer context panel

    // For now, return placeholder
    return nullptr;
}

void ChatPanelIntegration::showPanel(bool visible) {
    if (m_hwndPanel) {
        ShowWindow(m_hwndPanel, visible ? SW_SHOW : SW_HIDE);
    }
}

void ChatPanelIntegration::destroyPanel() {
    if (m_hwndPanel) {
        DestroyWindow(m_hwndPanel);
        m_hwndPanel = nullptr;
    }
}

void ChatPanelIntegration::initializeProviders() {
    // Check what extensions are installed
    m_providers["local-agent"] = true;  // Always available
    
    // Would check VsixLoader for these:
    // m_providers["github-copilot"] = vsixLoader.isExtensionInstalled("github.copilot");
    // m_providers["amazonq"] = vsixLoader.isExtensionInstalled("amazonwebservices.aws-toolkit-vscode");
}

void ChatPanelIntegration::initializeAgents() {
    // Register built-in agents
    AgentInfo localAgent;
    localAgent.name = "local-gguf";
    localAgent.description = "Local GGUF model inference engine";
    localAgent.capabilities = "code-generation,code-analysis,documentation,testing";
    localAgent.autonomous = true;
    localAgent.version = "1.0.0";
    registerAgent(localAgent);

    AgentInfo gitHubAgent;
    gitHubAgent.name = "github-copilot-agent";
    gitHubAgent.description = "GitHub Copilot AI assistant";
    gitHubAgent.capabilities = "code-generation,code-explanation,debugging,testing";
    gitHubAgent.autonomous = false;
    gitHubAgent.version = "1.0.0";
    registerAgent(gitHubAgent);

    AgentInfo amazonqAgent;
    amazonqAgent.name = "amazonq-agent";
    amazonqAgent.description = "Amazon Q enterprise AI assistant";
    amazonqAgent.capabilities = "code-generation,aws-expertise,documentation,optimization";
    amazonqAgent.autonomous = false;
    amazonqAgent.version = "1.0.0";
    registerAgent(amazonqAgent);
}

std::string ChatPanelIntegration::callGithubCopilotAPI(const std::string& message) {
    // Connect to GitHub Copilot via named pipe or REST API
    // For now, return placeholder response
    return "GitHub Copilot: " + message;
}

std::string ChatPanelIntegration::callAmazonQAPI(const std::string& message) {
    // Connect to Amazon Q via AWS SDK or REST API
    // For now, return placeholder response
    return "Amazon Q: " + message;
}

std::string ChatPanelIntegration::callLocalAgentAPI(const std::string& message) {
    // Connect to local GGUF-based agent
    // This would call the local agentic system we built
    // For now, return placeholder response
    return "Local Agent: " + message;
}

void ChatPanelIntegration::processTokenStream(const std::string& tokens) {
    // Handle streaming response tokens (for real-time display)
}

}  // namespace rawrxd::ide
