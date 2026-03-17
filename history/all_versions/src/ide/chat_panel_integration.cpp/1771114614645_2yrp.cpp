// chat_panel_integration.cpp — Full chat system implementation
#include "chat_panel_integration.h"
#include <windowsx.h>
#include <chrono>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>

#include "marketplace/vsix_loader.h"
#include "../modules/vscode_extension_api.h"

#ifdef _WIN32
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace rawrxd::ide {

namespace {
std::string toLowerAscii(const std::string& value) {
    std::string out = value;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}
} // namespace

ChatPanelIntegration::ChatPanelIntegration(void* vsixLoader) {
    m_extensionLoader = static_cast<rawrxd::marketplace::VsixLoader*>(vsixLoader);
    
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
    // Chat panel UI is created by the Win32 IDE main window (RawrXD_Win32_IDE.cpp /
    // Win32_IDE_Complete.cpp): g_hwndChatHistory, g_hwndChatInput, chat buttons.
    // This entry point is for headless/embedded hosts that want a standalone panel.
    // When the host does not create a panel (e.g. IDE owns it), return nullptr.
    (void)hwndParent;
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

void ChatPanelIntegration::syncWithExtensions() {
    initializeProviders();
}

void ChatPanelIntegration::initializeProviders() {
    // Check what extensions are installed
    m_providers["local-agent"] = true;  // Always available

    bool hasCopilot = false;
    bool hasAmazonQ = false;
    if (m_extensionLoader) {
        const auto installed = m_extensionLoader->getInstalledExtensions();
        for (const auto& ext : installed) {
            const std::string id = toLowerAscii(ext.publisher + "." + ext.name);
            if (id.find("github") != std::string::npos && id.find("copilot") != std::string::npos) {
                hasCopilot = true;
            }
            if (id.find("amazonwebservices") != std::string::npos &&
                (id.find("aws-toolkit") != std::string::npos || id.find("amazonq") != std::string::npos)) {
                hasAmazonQ = true;
            }
        }
    }

    auto& api = vscode::VSCodeExtensionAPI::instance();
    const bool apiReady = api.isInitialized();
    m_providers["github-copilot"] = hasCopilot || apiReady;
    m_providers["amazonq"] = hasAmazonQ || apiReady;
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
    // Prefer routing through the VS Code extension API (extension handles auth/session).
    auto& api = vscode::VSCodeExtensionAPI::instance();
    if (api.isInitialized()) {
        std::string args = std::string("{\"prompt\":\"") + escapeJson(message) +
                           "\",\"context\":\"ide.chat_panel\"}";
        auto apiResult = api.executeCommand("github.copilot.chat.proxy", args.c_str());
        if (apiResult.success) {
            if (apiResult.detail && apiResult.detail[0] && strcmp(apiResult.detail, "Success") != 0) {
                return apiResult.detail;
            }
            return "[GitHub Copilot] Extension command dispatched. Awaiting response stream...";
        }
        return std::string("[GitHub Copilot] ") + (apiResult.detail ? apiResult.detail : "Extension call failed");
    }

    // Fallback: environment-based hint (no direct REST integration here).
    const char* token = std::getenv("GITHUB_COPILOT_TOKEN");
    if (!token || !token[0]) {
        return "[GitHub Copilot] Not configured. Set GITHUB_COPILOT_TOKEN or install/enable the Copilot extension. Switch to 'local-agent' for RawrXD/Ollama chat.";
    }
    return "[GitHub Copilot] Extension API not initialized. Start the full IDE or use local-agent.";
}

std::string ChatPanelIntegration::callAmazonQAPI(const std::string& message) {
    // Prefer routing through the VS Code extension API (extension handles auth/session).
    auto& api = vscode::VSCodeExtensionAPI::instance();
    if (api.isInitialized()) {
        std::string args = std::string("{\"prompt\":\"") + escapeJson(message) + "\"}";
        auto apiResult = api.executeCommand("amazon.q.chat.proxy", args.c_str());
        if (apiResult.success) {
            if (apiResult.detail && apiResult.detail[0] && strcmp(apiResult.detail, "Success") != 0) {
                return apiResult.detail;
            }
            return "[Amazon Q] Extension command dispatched. Awaiting response stream...";
        }
        return std::string("[Amazon Q] ") + (apiResult.detail ? apiResult.detail : "Extension call failed");
    }

    // Fallback: environment-based hint (no direct REST integration here).
    const char* key = std::getenv("AWS_ACCESS_KEY_ID");
    const char* region = std::getenv("AWS_REGION");
    if (!key || !key[0]) {
        return "[Amazon Q] Not configured. Set AWS_ACCESS_KEY_ID (and AWS_SECRET_ACCESS_KEY, AWS_REGION) or use local-agent for RawrXD/Ollama chat.";
    }
    (void)region;
    return "[Amazon Q] Extension API not initialized. Start the full IDE or use local-agent.";
}

std::string ChatPanelIntegration::callLocalAgentAPI(const std::string& message) {
#ifdef _WIN32
    // POST to local RawrEngine/RawrXD_CLI on port 23959 (Win32 IDE) or 8080
    const wchar_t* host = L"localhost";
    int port = 23959;  // Win32 IDE default; RawrEngine uses 8080
    const char* portEnv = std::getenv("RAWRXD_CHAT_PORT");
    if (portEnv) port = std::atoi(portEnv);
    if (port <= 0) port = 23959;

    std::string body = "{\"message\":\"";
    for (char c : message) {
        if (c == '"') body += "\\\"";
        else if (c == '\\') body += "\\\\";
        else if (c == '\n') body += "\\n";
        else if (c == '\r') body += "\\r";
        else body += c;
    }
    body += "\"}";

    HINTERNET hSession = WinHttpOpen(L"RawrXD-ChatPanel/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return "Local Agent: connection failed (WinHttpOpen)";

    wchar_t portBuf[16];
    swprintf_s(portBuf, L"%d", port);
    HINTERNET hConnect = WinHttpConnect(hSession, host, (INTERNET_PORT)port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "Local Agent: connection failed (WinHttpConnect — is RawrEngine/RawrXD_CLI running on port " + std::to_string(port) + "?)";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/chat",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "Local Agent: connection failed (WinHttpOpenRequest)";
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.size(),
            (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "Local Agent: send failed";
    }
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "Local Agent: receive failed";
    }

    std::string response;
    DWORD bytesRead;
    char buf[4096];
    while (WinHttpReadData(hRequest, buf, sizeof(buf), &bytesRead) && bytesRead > 0) {
        response.append(buf, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    // Parse JSON response: {"response":"..."} or {"error":"..."}
    size_t r = response.find("\"response\":\"");
    if (r != std::string::npos) {
        r += 11;
        size_t end = r;
        while (end < response.size()) {
            if (response[end] == '\\' && end + 1 < response.size()) { end += 2; continue; }
            if (response[end] == '"') break;
            end++;
        }
        std::string extracted;
        for (size_t i = r; i < end; i++) {
            if (response[i] == '\\' && i + 1 < end) {
                if (response[i+1] == 'n') extracted += '\n';
                else if (response[i+1] == 'r') extracted += '\r';
                else if (response[i+1] == '"') extracted += '"';
                else extracted += response[i+1];
                i++;
            } else {
                extracted += response[i];
            }
        }
        return extracted;
    }
    size_t e = response.find("\"error\":\"");
    if (e != std::string::npos) {
        e += 9;
        size_t end = e;
        while (end < response.size() && response[end] != '"') end++;
        return "Local Agent error: " + response.substr(e, end - e);
    }
    return response.empty() ? "Local Agent: empty response" : response;
#else
    (void)message;
    return "Local Agent: Win32 only (start RawrEngine on port 23959 for chat)";
#endif
}

void ChatPanelIntegration::processTokenStream(const std::string& tokens) {
    // Streaming response tokens (for real-time display). Callback can append to chat history
    // or update UI; when no callback is set, tokens are ignored (batch response still added in sendMessage).
    if (m_onMessage && !tokens.empty()) {
        ChatMessage streamMsg;
        streamMsg.sender = "assistant";
        streamMsg.content = tokens;
        streamMsg.messageId = "stream_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        m_onMessage(streamMsg);
    }
}

}  // namespace rawrxd::ide
