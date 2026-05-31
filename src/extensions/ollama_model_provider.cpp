// ollama_model_provider.cpp - Unified Ollama model provider implementation
#include "ollama_model_provider.h"
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <url/curl.h>
#include <shlwapi.h>
#include <algorithm>
#include <thread>
#include <future>
#include <fstream>
#include <chrono>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")

namespace RawrXD::Extensions::Ollama {

namespace {

// Configuration defaults
constexpr const char* DEFAULT_OLLAMA_ENDPOINT = "http://localhost:11435";
constexpr int DEFAULT_TIMEOUT_MS = 5000;

// Utility functions
std::string WideToUTF8(const std::wstring& wide) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring UTF8ToWide(const std::string& utf8) {
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wide(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), size);
    return wide;
}

bool IsInternetAvailable() {
    return InternetGetConnectedState(nullptr, 0) != FALSE;
}

std::string HTTPGet(const std::string& url) {
    HINTERNET hSession = WinHttpOpen(L"RawrXD Ollama Provider", 
                                    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                                    WINHTTP_NO_PROXY_NAME, 
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    std::wstring wideUrl = UTF8ToWide(url);
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwSchemeLength = -1;
    urlComp.dwHostNameLength = -1;
    urlComp.dwUrlPathLength = -1;
    urlComp.dwExtraInfoLength = -1;

    if (!WinHttpCrackUrl(wideUrl.c_str(), wideUrl.length(), 0, &urlComp)) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
        path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), 
                                       urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), 
                                           nullptr, WINHTTP_NO_REFERER, 
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                           (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? 
                                           WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::string response;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        std::vector<char> buffer(dwSize + 1);
        if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;
        response.append(buffer.data(), dwDownloaded);
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

} // namespace

class OllamaModelProviderImpl : public OllamaModelProvider {
private:
    std::string m_ollamaEndpoint;
    std::vector<std::string> m_cloudEndpoints;
    std::vector<ModelInfo> m_models;
    ModelInfo m_currentModel;
    std::map<HWND, uint32_t> m_chatModes;
    std::map<HWND, ChatWindowConfig> m_chatConfigs;
    
    // Advanced agentic capabilities
    std::map<std::string, AgenticSession> m_autonomousSessions;
    std::map<std::string, std::string> m_contextStorage;
    
    mutable std::mutex m_mutex;
    
    ModelChangedCallback m_modelChangedCb;
    ModeChangedCallback m_modeChangedCb;
    ChatCreatedCallback m_chatCreatedCb;
    ChatClosedCallback m_chatClosedCb;
    
    bool m_initialized;
    bool m_autoDiscover;

public:
    OllamaModelProviderImpl() 
        : m_ollamaEndpoint(DEFAULT_OLLAMA_ENDPOINT)
        , m_initialized(false)
        , m_autoDiscover(true) {
        
        // Default model
        m_currentModel = {
            "default", "Default Model", "Default", m_ollamaEndpoint,
            ModelType::LocalOllama, CAP_MAX, "Default model", 4096, true, false
        };
    }

    ~OllamaModelProviderImpl() override {
        Shutdown();
    }

    bool Initialize(const std::string& config) override {
        std::lock_guard lock(m_mutex);
        
        try {
            auto json = nlohmann::json::parse(config);
            m_ollamaEndpoint = json.value("ollamaEndpoint", DEFAULT_OLLAMA_ENDPOINT);
            m_cloudEndpoints = json.value("cloudModels", std::vector<std::string>());
            m_autoDiscover = json.value("autoDiscover", true);
        } catch (...) {
            // Use defaults if config parsing fails
        }

        if (m_autoDiscover) {
            DiscoverModels(true);
        }

        m_initialized = true;
        return true;
    }

    void Shutdown() override {
        std::lock_guard lock(m_mutex);
        
        // Close all chat windows
        auto windows = GetActiveChatWindows();
        for (HWND hwnd : windows) {
            CloseChatWindow(hwnd);
        }
        
        m_initialized = false;
    }

    std::vector<ModelInfo> DiscoverModels(bool forceRefresh) override {
        std::lock_guard lock(m_mutex);
        
        if (!forceRefresh && !m_models.empty()) {
            return m_models;
        }

        m_models.clear();
        
        // Discover local Ollama models
        std::string response = HTTPGet(m_ollamaEndpoint + "/api/tags");
        if (!response.empty()) {
            try {
                auto json = nlohmann::json::parse(response);
                if (json.contains("models")) {
                    for (const auto& model : json["models"]) {
                        ModelInfo info;
                        info.id = model.value("name", "");
                        info.name = info.id;
                        info.displayName = info.id;
                        info.endpoint = m_ollamaEndpoint;
                        info.type = ModelType::LocalOllama;
                        info.capabilities = CAP_MAX; // Local models support all modes
                        info.description = model.value("description", "");
                        info.contextSize = model.value("size", 4096);
                        info.available = true;
                        info.requiresInternet = false;
                        
                        m_models.push_back(info);
                    }
                }
            } catch (...) {
                // JSON parsing failed
            }
        }

        // Add cloud models if internet is available
        if (IsInternetAvailable()) {
            for (const auto& endpoint : m_cloudEndpoints) {
                ModelInfo info;
                info.id = "cloud_" + endpoint;
                info.name = endpoint;
                info.displayName = "Cloud: " + endpoint;
                info.endpoint = endpoint;
                info.type = ModelType::CloudAPI;
                info.capabilities = CAP_MAX;
                info.description = "Cloud-based model endpoint";
                info.contextSize = 8192;
                info.available = true;
                info.requiresInternet = true;
                
                m_models.push_back(info);
            }
        }

        // Add default model if no models found
        if (m_models.empty()) {
            m_models.push_back(m_currentModel);
        }

        return m_models;
    }

    ModelInfo GetCurrentModel() const override {
        std::lock_guard lock(m_mutex);
        return m_currentModel;
    }

    bool SetCurrentModel(const std::string& modelId) override {
        std::lock_guard lock(m_mutex);
        
        auto it = std::find_if(m_models.begin(), m_models.end(), 
                             [&](const ModelInfo& m) { return m.id == modelId; });
        
        if (it != m_models.end()) {
            m_currentModel = *it;
            if (m_modelChangedCb) {
                m_modelChangedCb(m_currentModel);
            }
            return true;
        }
        
        return false;
    }

    HWND CreateChatWindow(const ChatWindowConfig& config) override {
        std::lock_guard lock(m_mutex);
        
        // Create chat window (simplified - actual implementation would use Win32 API)
        HWND hwnd = reinterpret_cast<HWND>(
            static_cast<intptr_t>(rand() % 1000000 + 100000));
        
        m_chatConfigs[hwnd] = config;
        m_chatModes[hwnd] = config.initialMode;
        
        if (m_chatCreatedCb) {
            m_chatCreatedCb(hwnd);
        }
        
        return hwnd;
    }

    bool CloseChatWindow(HWND hwnd) override {
        std::lock_guard lock(m_mutex);
        
        auto it = m_chatConfigs.find(hwnd);
        if (it != m_chatConfigs.end()) {
            m_chatConfigs.erase(it);
            m_chatModes.erase(hwnd);
            
            if (m_chatClosedCb) {
                m_chatClosedCb(hwnd);
            }
            return true;
        }
        
        return false;
    }

    std::vector<HWND> GetActiveChatWindows() const override {
        std::lock_guard lock(m_mutex);
        
        std::vector<HWND> windows;
        for (const auto& pair : m_chatConfigs) {
            windows.push_back(pair.first);
        }
        
        return windows;
    }

    uint32_t GetCurrentMode(HWND chatWindow) const override {
        std::lock_guard lock(m_mutex);
        
        auto it = m_chatModes.find(chatWindow);
        if (it != m_chatModes.end()) {
            return it->second;
        }
        
        return CAP_MAX; // Default to MAX mode
    }

    bool SetCurrentMode(HWND chatWindow, uint32_t mode) override {
        std::lock_guard lock(m_mutex);
        
        auto it = m_chatModes.find(chatWindow);
        if (it != m_chatModes.end()) {
            it->second = mode;
            if (m_modeChangedCb) {
                m_modeChangedCb(chatWindow, mode);
            }
            return true;
        }
        
        return false;
    }

    void SetModelChangedCallback(ModelChangedCallback cb) override {
        std::lock_guard lock(m_mutex);
        m_modelChangedCb = cb;
    }

    void SetModeChangedCallback(ModeChangedCallback cb) override {
        std::lock_guard lock(m_mutex);
        m_modeChangedCb = cb;
    }

    void SetChatCreatedCallback(ChatCreatedCallback cb) override {
        std::lock_guard lock(m_mutex);
        m_chatCreatedCb = cb;
    }

    void SetChatClosedCallback(ChatClosedCallback cb) override {
        std::lock_guard lock(m_mutex);
        m_chatClosedCb = cb;
    }

    bool IsConnected() const override {
        std::lock_guard lock(m_mutex);
        return m_initialized && !m_models.empty();
    }

    std::string GetStatus() const override {
        std::lock_guard lock(m_mutex);
        
        if (!m_initialized) return "Not initialized";
        if (m_models.empty()) return "No models available";
        
        return "Connected with " + std::to_string(m_models.size()) + " models";
    }

    // Advanced agentic capabilities implementation
    std::string ProcessMultiFileOperation(const MultiFileRequest& request) override {
        std::lock_guard lock(m_mutex);
        
        if (!m_initialized || !m_provider) return "Provider not initialized";
        
        try {
            nlohmann::json payload;
            payload["operation"] = request.operation;
            payload["instruction"] = request.instruction;
            payload["context"] = request.context;
            payload["files"] = request.filePaths;
            
            std::string response = HTTPPost(m_ollamaEndpoint + "/api/multi-file", payload.dump());
            
            if (!response.empty()) {
                auto json = nlohmann::json::parse(response);
                if (json.contains("result")) {
                    return json["result"].get<std::string>();
                }
            }
            return "Multi-file operation failed";
        } catch (const std::exception& e) {
            return "Error: " + std::string(e.what());
        }
    }

    AgenticSession StartAutonomousTask(const AutonomousTask& task) override {
        std::lock_guard lock(m_mutex);
        
        AgenticSession session;
        session.sessionId = "session_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        session.currentGoal = task.goal;
        session.stepsTaken = 0;
        session.completed = false;
        
        // Store session
        m_autonomousSessions[session.sessionId] = session;
        
        // Start background thread for autonomous execution
        std::thread([this, session, task]() {
            ExecuteAutonomousTask(session.sessionId, task);
        }).detach();
        
        return session;
    }

    bool StopAutonomousTask(const std::string& sessionId) override {
        std::lock_guard lock(m_mutex);
        
        auto it = m_autonomousSessions.find(sessionId);
        if (it != m_autonomousSessions.end()) {
            it->second.completed = true;
            return true;
        }
        return false;
    }

    AgenticSession GetAutonomousSession(const std::string& sessionId) const override {
        std::lock_guard lock(m_mutex);
        
        auto it = m_autonomousSessions.find(sessionId);
        if (it != m_autonomousSessions.end()) {
            return it->second;
        }
        
        AgenticSession empty;
        empty.sessionId = sessionId;
        empty.completed = true;
        return empty;
    }

    std::vector<AgenticSession> GetActiveSessions() const override {
        std::lock_guard lock(m_mutex);
        
        std::vector<AgenticSession> sessions;
        for (const auto& pair : m_autonomousSessions) {
            if (!pair.second.completed) {
                sessions.push_back(pair.second);
            }
        }
        return sessions;
    }

    ToolExecutionResult ExecuteTool(const std::string& toolName, 
                                  const std::string& parameters) override {
        std::lock_guard lock(m_mutex);
        
        ToolExecutionResult result;
        result.toolName = toolName;
        result.parameters = parameters;
        
        try {
            // Implement tool execution logic here
            if (toolName == "file_read") {
                std::ifstream file(parameters);
                if (file) {
                    std::string content((std::istreambuf_iterator<char>(file)), 
                                       std::istreambuf_iterator<char>());
                    result.output = content;
                    result.success = true;
                } else {
                    result.error = "File not found: " + parameters;
                    result.success = false;
                }
            } else if (toolName == "file_write") {
                // Parse parameters: "path|content"
                size_t pos = parameters.find('|');
                if (pos != std::string::npos) {
                    std::string path = parameters.substr(0, pos);
                    std::string content = parameters.substr(pos + 1);
                    
                    std::ofstream file(path);
                    if (file) {
                        file << content;
                        result.output = "File written successfully: " + path;
                        result.success = true;
                    } else {
                        result.error = "Failed to write file: " + path;
                        result.success = false;
                    }
                } else {
                    result.error = "Invalid parameters format";
                    result.success = false;
                }
            } else {
                result.error = "Unknown tool: " + toolName;
                result.success = false;
            }
        } catch (const std::exception& e) {
            result.error = "Tool execution error: " + std::string(e.what());
            result.success = false;
        }
        
        return result;
    }

    std::vector<std::string> GetAvailableTools() const override {
        return {
            "file_read",
            "file_write", 
            "code_analyze",
            "test_generate",
            "document_generate",
            "refactor",
            "search_codebase",
            "execute_command"
        };
    }

    void AddToContext(const std::string& key, const std::string& value) override {
        std::lock_guard lock(m_mutex);
        m_contextStorage[key] = value;
    }

    std::string GetFromContext(const std::string& key) const override {
        std::lock_guard lock(m_mutex);
        
        auto it = m_contextStorage.find(key);
        if (it != m_contextStorage.end()) {
            return it->second;
        }
        return "";
    }

    void ClearContext() override {
        std::lock_guard lock(m_mutex);
        m_contextStorage.clear();
    }

    std::string AnalyzeCodebase(const std::string& path, 
                              const std::string& analysisType) override {
        std::lock_guard lock(m_mutex);
        
        try {
            nlohmann::json payload;
            payload["path"] = path;
            payload["analysis_type"] = analysisType;
            
            std::string response = HTTPPost(m_ollamaEndpoint + "/api/analyze-codebase", payload.dump());
            
            if (!response.empty()) {
                auto json = nlohmann::json::parse(response);
                if (json.contains("analysis")) {
                    return json["analysis"].get<std::string>();
                }
            }
            return "Codebase analysis failed";
        } catch (const std::exception& e) {
            return "Error: " + std::string(e.what());
        }
    }

    std::string GenerateDocumentation(const std::vector<std::string>& files) override {
        std::lock_guard lock(m_mutex);
        
        try {
            nlohmann::json payload;
            payload["files"] = files;
            
            std::string response = HTTPPost(m_ollamaEndpoint + "/api/generate-docs", payload.dump());
            
            if (!response.empty()) {
                auto json = nlohmann::json::parse(response);
                if (json.contains("documentation")) {
                    return json["documentation"].get<std::string>();
                }
            }
            return "Documentation generation failed";
        } catch (const std::exception& e) {
            return "Error: " + std::string(e.what());
        }
    }

    std::string RefactorCode(const std::vector<std::string>& files, 
                           const std::string& refactoringPattern) override {
        std::lock_guard lock(m_mutex);
        
        try {
            nlohmann::json payload;
            payload["files"] = files;
            payload["pattern"] = refactoringPattern;
            
            std::string response = HTTPPost(m_ollamaEndpoint + "/api/refactor", payload.dump());
            
            if (!response.empty()) {
                auto json = nlohmann::json::parse(response);
                if (json.contains("refactored_code")) {
                    return json["refactored_code"].get<std::string>();
                }
            }
            return "Refactoring failed";
        } catch (const std::exception& e) {
            return "Error: " + std::string(e.what());
        }
    }

private:
    // Helper methods for autonomous execution
    void ExecuteAutonomousTask(const std::string& sessionId, const AutonomousTask& task) {
        // Autonomous task execution logic
        for (uint32_t step = 0; step < task.maxSteps; ++step) {
            {
                std::lock_guard lock(m_mutex);
                auto it = m_autonomousSessions.find(sessionId);
                if (it == m_autonomousSessions.end() || it->second.completed) {
                    break;
                }
                it->second.stepsTaken = step + 1;
            }
            
            // Execute one step of the autonomous task
            if (!ExecuteAutonomousStep(sessionId, task, step)) {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        {
            std::lock_guard lock(m_mutex);
            auto it = m_autonomousSessions.find(sessionId);
            if (it != m_autonomousSessions.end()) {
                it->second.completed = true;
            }
        }
    }

    bool ExecuteAutonomousStep(const std::string& sessionId, const AutonomousTask& task, uint32_t step) {
        // Implement autonomous step execution logic
        // This would involve calling the model, executing tools, and updating session state
        return true;
    }

    std::string HTTPPost(const std::string& url, const std::string& data) {
        // Similar to HTTPGet but for POST requests
        HINTERNET hSession = WinHttpOpen(L"RawrXD Ollama Provider", 
                                        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                                        WINHTTP_NO_PROXY_NAME, 
                                        WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) return "";

        std::wstring wideUrl = UTF8ToWide(url);
        URL_COMPONENTS urlComp = {0};
        urlComp.dwStructSize = sizeof(urlComp);
        urlComp.dwSchemeLength = -1;
        urlComp.dwHostNameLength = -1;
        urlComp.dwUrlPathLength = -1;
        urlComp.dwExtraInfoLength = -1;

        if (!WinHttpCrackUrl(wideUrl.c_str(), wideUrl.length(), 0, &urlComp)) {
            WinHttpCloseHandle(hSession);
            return "";
        }

        std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
        std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
        if (urlComp.dwExtraInfoLength > 0) {
            path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
        }

        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), 
                                           urlComp.nPort, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            return "";
        }

        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), 
                                               nullptr, WINHTTP_NO_REFERER, 
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                               (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? 
                                               WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        // Set headers
        std::wstring headers = L"Content-Type: application/json\r\n";
        WinHttpAddRequestHeaders(hRequest, headers.c_str(), headers.length(), 
                                WINHTTP_ADDREQ_FLAG_ADD);

        // Send request
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, 
                               (LPVOID)data.c_str(), data.length(), data.length(), 0)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return "";
        }

        std::string response;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize + 1);
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) break;
            response.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
    }
};

// Plugin exports
extern "C" {
    __declspec(dllexport) OllamaModelProvider* CreateOllamaProvider() {
        return new OllamaModelProviderImpl();
    }

    __declspec(dllexport) void DestroyOllamaProvider(OllamaModelProvider* provider) {
        delete provider;
    }

    __declspec(dllexport) const char* GetPluginVersion() {
        return "1.0.0";
    }
}

} // namespace RawrXD::Extensions::Ollama
