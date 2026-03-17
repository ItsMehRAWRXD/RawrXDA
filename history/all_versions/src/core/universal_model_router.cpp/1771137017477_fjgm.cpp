#include "universal_model_router.hpp"
#include <windows.h>
#include <winhttp.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <functional>

#pragma comment(lib, "winhttp.lib")

namespace RawrXD {

struct ModelCapability {
    bool supportsVision;
    bool supportsFunctionCalling;
    bool supportsStreaming;
    size_t maxContextLength;
    float qualityScore;
};

class UniversalModelRouter {
    struct Backend {
        std::string name;
        std::string endpoint;
        bool isLocal;
        ModelCapability caps;
        float latencyMs;
        bool healthy;
    };
    
    std::vector<Backend> backends_;
    std::mutex mtx_;
    Backend* activeBackend_ = nullptr;
    
public:
    UniversalModelRouter() {
        // Auto-discover Ollama
        Backend ollama;
        ollama.name = "ollama-local";
        ollama.endpoint = "http://localhost:11434";
        ollama.isLocal = true;
        ollama.caps = {true, true, true, 131072, 0.85f};
        ollama.latencyMs = 0.0f;
        ollama.healthy = true;
        backends_.push_back(ollama);
        
        // Auto-discover local GGUF
        Backend local;
        local.name = "gguf-native";
        local.endpoint = "local";
        local.isLocal = true;
        local.caps = {false, false, true, 8192, 0.75f};
        local.latencyMs = 0.0f;
        local.healthy = true;
        backends_.push_back(local);
        
        if (!backends_.empty()) {
            activeBackend_ = &backends_[0];
        }
    }
    
    ~UniversalModelRouter() = default;
    
    void initializeLocalEngine(const std::string& modelPath) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (backends_.size() > 1) {
            activeBackend_ = &backends_[1]; // GGUF
        }
    }
    
    void routeRequest(const std::string& prompt, 
                      const std::string& systemPrompt,
                      std::function<void(const std::string&, bool)> callback) {
        std::lock_guard<std::mutex> lock(mtx_);
        
        if (!activeBackend_) {
            callback("Error: No backend initialized", true);
            return;
        }
        
        if (activeBackend_->name == "ollama-local") {
            routeToOllama(prompt, systemPrompt, callback);
        } else {
            callback("Local GGUF inference not yet implemented", true);
        }
    }
    
    std::vector<std::string> getAvailableBackends() const {
        std::vector<std::string> names;
        for (const auto& b : backends_) {
            if (b.healthy) names.push_back(b.name);
        }
        return names;
    }

private:
    void routeToOllama(const std::string& prompt, 
                       const std::string& systemPrompt,
                       std::function<void(const std::string&, bool)> cb) {
        // Real WinHTTP implementation
        HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0", 
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (!hSession) {
            cb("HTTP Error: Failed to open session", true);
            return;
        }
        
        HINTERNET hConnect = WinHttpConnect(hSession, L"localhost", 11434, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            cb("HTTP Error: Failed to connect", true);
            return;
        }
        
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", 
            L"/api/generate", NULL, WINHTTP_NO_REFERER, 
            WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
            
        if (!hRequest) {
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            cb("HTTP Error: Failed to create request", true);
            return;
        }
        
        // Build JSON payload
        std::string body = "{\"model\":\"llama3\",\"prompt\":\"" + prompt + 
                          "\",\"system\":\"" + systemPrompt + "\",\"stream\":true}";
        
        std::wstring headers = L"Content-Type: application/json";
        BOOL success = WinHttpSendRequest(hRequest, headers.c_str(), -1, 
            (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0);
            
        if (!success) {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            cb("HTTP Error: Failed to send request", true);
            return;
        }
        
        WinHttpReceiveResponse(hRequest, NULL);
        
        // Stream handling
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (dwSize == 0) break;
            
            std::vector<char> buffer(dwSize + 1);
            WinHttpReadData(hRequest, (LPVOID)buffer.data(), dwSize, &dwDownloaded);
            buffer[dwDownloaded] = '\0';
            
            // Simple streaming - just pass through for now
            cb(std::string(buffer.data(), dwDownloaded), false);
        } while (dwSize > 0);
        
        cb("", true); // Signal completion
        
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
    }
};

} // namespace RawrXD
