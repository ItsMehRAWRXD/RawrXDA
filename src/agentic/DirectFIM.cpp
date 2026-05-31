// ============================================================================
// DirectFIM.cpp — ONE working method for real ghost text tokens
// ============================================================================
// This file provides a single function: DirectFIM_Complete()
// 
// It uses EXISTING working backends in priority order:
//   1. RawrXD_Titan.dll (native, fastest) — already loaded by GhostText
//   2. BackendOrchestrator (queue-based, reliable) — already used by AgentOllamaClient
//   3. Ollama HTTP direct (fallback, always works if Ollama running)
//
// No new abstractions. No routers. No bridges. Just real tokens.
// ============================================================================

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <chrono>
#include <future>
#include <json.hpp>

#pragma comment(lib, "winhttp.lib")

// --- Existing RawrXD includes (these already compile in your build) ---
#include "../backend/BackendOrchestrator.h"
#include "../RawrXD_Exports.h"  // For Titan DLL function pointers

// ============================================================================
// CONFIGURATION — Adjust these to match your setup
// ============================================================================

static const char* OLLAMA_HOST = "localhost";
static const int   OLLAMA_PORT = 11434;
static const char* OLLAMA_MODEL = "codellama:7b-code";  // Change to your loaded model

// Titan DLL path — already used by GhostText
static const char* TITAN_DLL_PATH = "D:\\rawrxd\\bin\\RawrXD_Titan.dll";

// ============================================================================
// TITAN DLL DYNAMIC LOAD (same pattern as Win32IDE_GhostText.cpp)
// ============================================================================

typedef int (*RawrXD_InitializeFn)(void);
typedef int (*RawrXD_ShutdownFn)(void);
typedef int (*RawrXD_LoadModelFn)(const char* path, uint64_t* outHandle);
typedef int (*RawrXD_SelectModelFn)(uint64_t handle);
typedef int (*RawrXD_SetSamplingParamsFn)(float temp, float topP, int maxTokens);
typedef int (*RawrXD_InferAsyncFn)(const char* prompt, uint64_t* outHandle);
typedef int (*RawrXD_WaitForInferenceFn)(uint64_t handle, char* outBuf, size_t outSize, uint32_t timeoutMs);
typedef int (*RawrXD_CancelInferenceFn)(uint64_t handle);

struct TitanState {
    HMODULE hMod = nullptr;
    bool initialized = false;
    uint64_t activeModel = 0;
    std::string loadedPath;
    
    RawrXD_InitializeFn      fnInit = nullptr;
    RawrXD_ShutdownFn        fnShutdown = nullptr;
    RawrXD_LoadModelFn       fnLoadModel = nullptr;
    RawrXD_SelectModelFn     fnSelectModel = nullptr;
    RawrXD_SetSamplingParamsFn fnSetParams = nullptr;
    RawrXD_InferAsyncFn      fnInferAsync = nullptr;
    RawrXD_WaitForInferenceFn fnWait = nullptr;
    RawrXD_CancelInferenceFn fnCancel = nullptr;
};

static TitanState g_titan;

static bool EnsureTitanReady(const std::string& modelPath) {
    if (g_titan.hMod && g_titan.initialized && g_titan.loadedPath == modelPath) {
        return true; // Already ready
    }
    
    // Load DLL
    if (!g_titan.hMod) {
        g_titan.hMod = LoadLibraryA(TITAN_DLL_PATH);
        if (!g_titan.hMod) {
            // Try relative path
            g_titan.hMod = LoadLibraryA("RawrXD_Titan.dll");
        }
        if (!g_titan.hMod) return false;
        
        g_titan.fnInit       = (RawrXD_InitializeFn)GetProcAddress(g_titan.hMod, "RawrXD_Initialize");
        g_titan.fnShutdown   = (RawrXD_ShutdownFn)GetProcAddress(g_titan.hMod, "RawrXD_Shutdown");
        g_titan.fnLoadModel  = (RawrXD_LoadModelFn)GetProcAddress(g_titan.hMod, "RawrXD_LoadModel");
        g_titan.fnSelectModel= (RawrXD_SelectModelFn)GetProcAddress(g_titan.hMod, "RawrXD_SelectModel");
        g_titan.fnSetParams  = (RawrXD_SetSamplingParamsFn)GetProcAddress(g_titan.hMod, "RawrXD_SetSamplingParams");
        g_titan.fnInferAsync = (RawrXD_InferAsyncFn)GetProcAddress(g_titan.hMod, "RawrXD_InferAsync");
        g_titan.fnWait       = (RawrXD_WaitForInferenceFn)GetProcAddress(g_titan.hMod, "RawrXD_WaitForInference");
        g_titan.fnCancel     = (RawrXD_CancelInferenceFn)GetProcAddress(g_titan.hMod, "RawrXD_CancelInference");
        
        if (!g_titan.fnInit || !g_titan.fnLoadModel || !g_titan.fnInferAsync || !g_titan.fnWait) {
            FreeLibrary(g_titan.hMod);
            g_titan.hMod = nullptr;
            return false;
        }
    }
    
    // Initialize
    if (!g_titan.initialized) {
        if (g_titan.fnInit() != 0) return false;
        g_titan.initialized = true;
    }
    
    // Load model
    if (g_titan.loadedPath != modelPath || g_titan.activeModel == 0) {
        uint64_t handle = 0;
        if (g_titan.fnLoadModel(modelPath.c_str(), &handle) != 0 || handle == 0) {
            return false;
        }
        if (g_titan.fnSelectModel(handle) != 0) {
            return false;
        }
        g_titan.activeModel = handle;
        g_titan.loadedPath = modelPath;
    }
    
    return true;
}

// ============================================================================
// BACKEND 1: TITAN NATIVE (fastest, direct DLL)
// ============================================================================

static std::string FIM_Titan(const std::string& prefix, const std::string& suffix, 
                              const std::string& modelPath, int maxTokens) {
    if (!EnsureTitanReady(modelPath)) {
        return ""; // Titan not available
    }
    
    // Build FIM prompt (CodeLlama format)
    std::string prompt = "<PRE> " + prefix + " <SUF>" + suffix + " <MID>";
    
    // Set params
    g_titan.fnSetParams(0.2f, 0.95f, maxTokens);
    
    // Run inference
    uint64_t handle = 0;
    if (g_titan.fnInferAsync(prompt.c_str(), &handle) != 0 || handle == 0) {
        return "";
    }
    
    char outBuf[4096] = {};
    int result = g_titan.fnWait(handle, outBuf, sizeof(outBuf), 30000); // 30s timeout
    
    if (result == 0) {
        return std::string(outBuf);
    }
    
    g_titan.fnCancel(handle);
    return "";
}

// ============================================================================
// BACKEND 2: BACKEND ORCHESTRATOR (queue-based, reliable)
// ============================================================================

static std::string FIM_Orchestrator(const std::string& prefix, const std::string& suffix,
                                     int maxTokens) {
    auto& bo = RawrXD::BackendOrchestrator::Instance();
    if (!bo.IsInitialized()) {
        return "";
    }
    
    std::string prompt = "<PRE> " + prefix + " <SUF>" + suffix + " <MID>";
    
    RawrXD::InferRequest req;
    req.id = 0; // Auto-assigned
    req.prompt = prompt;
    req.max_tokens = maxTokens;
    req.tenant_id = "ghost_text_fim";
    
    std::promise<std::string> completionPromise;
    req.complete_cb = [&](const std::string& completion, const std::string& metadata) {
        completionPromise.set_value(completion);
    };
    
    uint64_t reqId = bo.Enqueue(req);
    
    auto future = completionPromise.get_future();
    if (future.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
        bo.Cancel(reqId);
        return "";
    }
    
    return future.get();
}

// ============================================================================
// BACKEND 3: OLLAMA HTTP DIRECT (fallback, no dependencies)
// ============================================================================

static std::string EscapeJson(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c;
        }
    }
    return out;
}

static std::string ExtractJsonField(const std::string& json, const char* fieldName) {
    std::string search = std::string("\"") + fieldName + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos += search.length();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";
    return json.substr(pos, end - pos);
}

static std::string FIM_OllamaDirect(const std::string& prefix, const std::string& suffix,
                                       int maxTokens) {
    // Build prompt
    std::string fimPrompt = "<PRE> " + prefix + " <SUF>" + suffix + " <MID>";
    
    // Build JSON body
    std::string jsonBody = "{";
    jsonBody += "\"model\":\"" + std::string(OLLAMA_MODEL) + "\",";
    jsonBody += "\"prompt\":\"" + EscapeJson(fimPrompt) + "\",";
    jsonBody += "\"stream\":false,";
    jsonBody += "\"options\":{";
    jsonBody += "\"temperature\":0.2,";
    jsonBody += "\"num_predict\":" + std::to_string(maxTokens) + ",";
    jsonBody += "\"stop\":[\"\u003cEOT\u003e\",\"\u003cPRE\u003e\",\"\u003cSUF\u003e\",\"\u003cMID\u003e\"]";
    jsonBody += "}}";
    
    // WinHTTP request
    HINTERNET hSession = WinHttpOpen(L"RawrXD-DirectFIM/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    
    HINTERNET hConnect = WinHttpConnect(hSession, 
        std::wstring(OLLAMA_HOST, OLLAMA_HOST + strlen(OLLAMA_HOST)).c_str(),
        OLLAMA_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    // Send
    std::wstring headers = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (ULONG)headers.length(),
        (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.length(), (DWORD)jsonBody.length(), 0);
    
    if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }
    
    // Read response
    std::string response;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize > 0) {
            std::vector<char> buffer(dwSize + 1);
            WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded);
            buffer[dwDownloaded] = '\0';
            response += buffer.data();
        }
    } while (dwSize > 0);
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    // Extract completion from JSON
    return ExtractJsonField(response, "response");
}

// ============================================================================
// PUBLIC API: ONE FUNCTION
// ============================================================================

/**
 * DirectFIM_Complete — Get real FIM completion using existing working backends.
 * 
 * @param prefix     Code before cursor
 * @param suffix     Code after cursor  
 * @param modelPath  Path to GGUF model (for Titan backend)
 * @param maxTokens  Max tokens to generate
 * @return           Completion string (empty if all backends failed)
 */
std::string DirectFIM_Complete(const std::string& prefix, const std::string& suffix,
                                const std::string& modelPath, int maxTokens) {
    // Try backends in priority order
    std::string result;
    
    // 1. Titan Native (fastest)
    if (!modelPath.empty()) {
        result = FIM_Titan(prefix, suffix, modelPath, maxTokens);
        if (!result.empty()) return result;
    }
    
    // 2. Backend Orchestrator (reliable queue)
    result = FIM_Orchestrator(prefix, suffix, maxTokens);
    if (!result.empty()) return result;
    
    // 3. Ollama HTTP (fallback, always works if Ollama running)
    result = FIM_OllamaDirect(prefix, suffix, maxTokens);
    if (!result.empty()) return result;
    
    // All backends failed
    return "";
}

/**
 * DirectFIM_CompleteWithStream — Streaming version for real-time ghost text.
 * 
 * @param prefix     Code before cursor
 * @param suffix     Code after cursor
 * @param modelPath  Path to GGUF model (for Titan backend)
 * @param maxTokens  Max tokens to generate
 * @param onToken    Callback for each token (called on caller's thread)
 * @return           true if streaming started, false if all backends failed
 */
bool DirectFIM_CompleteWithStream(const std::string& prefix, const std::string& suffix,
                                   const std::string& modelPath, int maxTokens,
                                   std::function<void(const std::string&)> onToken) {
    if (!onToken) return false;
    
    // For streaming, we use Ollama HTTP with stream=true
    // (Titan streaming requires more complex async setup)
    
    std::string fimPrompt = "<PRE> " + prefix + " <SUF>" + suffix + " <MID>";
    
    std::string jsonBody = "{";
    jsonBody += "\"model\":\"" + std::string(OLLAMA_MODEL) + "\",";
    jsonBody += "\"prompt\":\"" + EscapeJson(fimPrompt) + "\",";
    jsonBody += "\"stream\":true,";
    jsonBody += "\"options\":{";
    jsonBody += "\"temperature\":0.2,";
    jsonBody += "\"num_predict\":" + std::to_string(maxTokens) + ",";
    jsonBody += "\"stop\":[\"\u003cEOT\u003e\",\"\u003cPRE\u003e\",\"\u003cSUF\u003e\",\"\u003cMID\u003e\"]";
    jsonBody += "}}";
    
    // WinHTTP request
    HINTERNET hSession = WinHttpOpen(L"RawrXD-DirectFIM/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;
    
    HINTERNET hConnect = WinHttpConnect(hSession,
        std::wstring(OLLAMA_HOST, OLLAMA_HOST + strlen(OLLAMA_HOST)).c_str(),
        OLLAMA_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate",
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    std::wstring headers = L"Content-Type: application/json";
    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), (ULONG)headers.length(),
        (LPVOID)jsonBody.c_str(), (DWORD)jsonBody.length(), (DWORD)jsonBody.length(), 0);
    
    if (!sent || !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    // Read streaming response line by line
    std::string buffer;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    
    while (true) {
        dwSize = 0;
        WinHttpQueryDataAvailable(hRequest, &dwSize);
        if (dwSize == 0) break;
        
        std::vector<char> chunk(dwSize + 1);
        WinHttpReadData(hRequest, chunk.data(), dwSize, &dwDownloaded);
        if (dwDownloaded == 0) break;
        chunk[dwDownloaded] = '\0';
        
        buffer += chunk.data();
        
        // Process complete lines (JSON objects)
        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            
            if (line.empty()) continue;
            
            // Extract token from {"response":"token","done":false}
            std::string token = ExtractJsonField(line, "response");
            if (!token.empty()) {
                onToken(token);
            }
            
            // Check if done
            if (line.find("\"done\":true") != std::string::npos) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return true;
            }
        }
    }
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// ============================================================================
// SIMPLE TEST / VERIFICATION
// ============================================================================

#ifdef DIRECT_FIM_TEST_MAIN
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "=== DirectFIM Test ===\n";
    
    std::string prefix = "void hello() {\n    std::cout << \"";
    std::string suffix = "\";\n}";
    std::string modelPath = (argc > 1) ? argv[1] : "";
    
    std::cout << "Prefix: " << prefix << "\n";
    std::cout << "Suffix: " << suffix << "\n";
    std::cout << "Model:  " << (modelPath.empty() ? "(none, using Ollama)" : modelPath) << "\n\n";
    
    // Test sync completion
    std::cout << "Testing sync completion...\n";
    std::string result = DirectFIM_Complete(prefix, suffix, modelPath, 50);
    
    if (!result.empty()) {
        std::cout << "SUCCESS: \"" << result << "\"\n";
    } else {
        std::cout << "FAILED: All backends returned empty\n";
        std::cout << "  - Is Ollama running on " << OLLAMA_HOST << ":" << OLLAMA_PORT << "?\n";
        std::cout << "  - Is model '" << OLLAMA_MODEL << "' loaded?\n";
        std::cout << "  - Is Titan DLL at " << TITAN_DLL_PATH << "?\n";
    }
    
    // Test streaming
    std::cout << "\nTesting streaming...\n";
    bool streamOk = DirectFIM_CompleteWithStream(prefix, suffix, modelPath, 50,
        [](const std::string& token) {
            std::cout << token << std::flush;
        });
    
    if (streamOk) {
        std::cout << "\nSTREAM SUCCESS\n";
    } else {
        std::cout << "\nSTREAM FAILED\n";
    }
    
    return result.empty() ? 1 : 0;
}
#endif
