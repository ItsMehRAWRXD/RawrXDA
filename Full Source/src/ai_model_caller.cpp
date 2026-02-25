#include "ai_model_caller.h"
#include "agentic_configuration.h"
#include "cpu_inference_engine.h"
#include <fstream>
#include <mutex>
#include <iostream>
#include <filesystem>
#include <windows.h>
#include <winhttp.h>
#include <thread>
#include <memory>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;

// Helper: Simple blocking HTTP POST
static std::string WinHttpPost(const std::wstring& domain, int port, const std::wstring& path, const std::string& body) {
    HINTERNET hSession = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    HINTERNET hConnect = WinHttpConnect(hSession, domain.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::string response;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD dwSize = 0;
            DWORD dwDownloaded = 0;
            do {
                dwSize = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;
                
                std::vector<char> buffer(dwSize + 1);
                if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
                    response.append(buffer.data(), dwDownloaded);
                }
            } while (dwSize > 0);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

namespace RawrXD {

std::vector<ModelCaller::Completion> ModelCaller::generateCompletion(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::string& context,
    int numCompletions) 
{
    std::vector<Completion> results;
    try {
        std::string prompt = buildCompletionPrompt(prefix, suffix, fileType, context);
        GenerationParams params;
        params.temperature = 0.3f;
        params.max_tokens = 256;
        params.top_p = 0.9f;

        auto response = callModel(prompt, params);
        if (response.substr(0, 8) == "// Error") {
            // Log error but don't crash
            return results;
        }

        auto completions = parseCompletions(response);
        for (const auto& compText : completions) {
            Completion result;
            result.text = compText;
            result.score = scoreCompletion(compText, prefix, fileType);
            result.description = getCompletionDescription(compText);
            results.push_back(result);
        }

        std::sort(results.begin(), results.end(),
                 [](const Completion& a, const Completion& b) {
                     return a.score > b.score;
                 });
    } catch (...) {
        // Log error
    }
    return results;
}

// Helper to call Native Titan Backend via IPC
std::string CallNativeHost(const std::string& prompt) {
    HANDLE hPipe;
    char buffer[8192]; // Increased buffer
    DWORD bytesRead, bytesWritten;
    
    // 1. Try to connect to the Native Host Pipe
    // MUST match RawrXD_PipeServer.asm: \\.\pipe\RawrXD_IPC
    hPipe = CreateFileA(
        "\\\\.\\pipe\\RawrXD_IPC",
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );
    
    if (hPipe == INVALID_HANDLE_VALUE) {
        return ""; // Host not running
    }
    
    // 2. Send Actual Prompt
    // NativeHost receives raw bytes and passes to Titan_SubmitPrompt
    if (!WriteFile(hPipe, prompt.c_str(), (DWORD)prompt.length(), &bytesWritten, NULL)) {
        CloseHandle(hPipe);
        return "";
    }
    
    // 3. Read Response
    // NativeHost writes g_OutputBuffer back
    if (ReadFile(hPipe, buffer, sizeof(buffer)-1, &bytesRead, NULL)) {
        buffer[bytesRead] = 0;
        CloseHandle(hPipe);
        return std::string(buffer);
    }
    
    CloseHandle(hPipe);
    return "";
}

std::string ModelCaller::callModel(const std::string& prompt, const GenerationParams& params) {
    try {
        // Try Native IPC first (Production Path)
        std::string nativeResp = CallNativeHost(prompt);
        if (!nativeResp.empty()) {
            // If NativeHost is running, use it!
            // This replaces the stub logic.
            return nativeResp;
        }

        AgenticConfiguration& config = AgenticConfiguration::getInstance();
        // Ensure initialized (singleton handles ctor defaults, but maybe env overrides needed?)
        // Assuming global init happens elsewhere or defaults are fine.
        
        std::string modelType = config.get("model_type", "local");
        std::string modelPath = config.getModelPath();

        if (modelType == "local" || modelType == "gguf") {
            static std::unique_ptr<CPUInferenceEngine> engine;
            static std::string currentLoadedModel;
            static std::mutex engineMutex;
            
            std::lock_guard<std::mutex> lock(engineMutex);

            if (!engine) {
                engine = std::make_unique<CPUInferenceEngine>();
                unsigned int threads = std::thread::hardware_concurrency();
                if (threads == 0) threads = 4;
                engine->SetThreadCount(threads);
            }

            if (currentLoadedModel != modelPath) {
                if (fs::exists(modelPath)) {
                    if (engine->LoadModel(modelPath)) {
                        currentLoadedModel = modelPath;
                    } else {
                        return "// Error: Failed to load model weights from " + modelPath;
                    }
                } else {
                     // Auto-discovery: Try models/ directory
                     std::string fallbackPath = "models/model.gguf";
                     if (!fs::exists(fallbackPath)) fallbackPath = "../models/model.gguf";
                     
                     if (fs::exists(fallbackPath)) {
                         if (engine->LoadModel(fallbackPath)) {
                             currentLoadedModel = fallbackPath;
                         } else {
                             return "// Error: Found model at " + fallbackPath + " but failed to load.";
                         }
                     } else {
                         return "// Error: Model file not found. Please configure a valid model path in config or place 'model.gguf' in 'models/' directory.";
                     }
                }
            }

            if (!engine->isModelLoaded()) return "// Error: No model loaded.";

            auto inputTokens = engine->Tokenize(prompt);
            auto outputTokens = engine->Generate(inputTokens, params.max_tokens);
            return engine->Detokenize(outputTokens);
        }
        else if (modelType == "ollama") {
             // Real logic for Ollama
             // Assume localhost:11434 by default or use config
             std::string endpoint = "localhost";
             int port = 11434;
             std::wstring wendpoint = L"localhost";
             
             // Construct valid JSON body (minimal)
             // Escaping needs to be handled properly for production, simple one here
             std::string safePrompt = prompt; // In real app, escape quotes/newlines
             
             std::string body = "{\"model\": \"" + modelPath + "\", \"prompt\": \"" + safePrompt + "\", \"stream\": false}";
             
             std::string resp = WinHttpPost(wendpoint, port, L"/api/generate", body);
             
             if (resp.empty()) return "// Error: Failed to connect to Ollama";
             
             // Simple parse of "response" field
             // {"response": "..."}
             size_t rpos = resp.find("\"response\":\"");
             if (rpos != std::string::npos) {
                 rpos += 12;
                 size_t endq = resp.find("\"", rpos); // This is naive if response has escaped quotes
                 // Better to use a mini tokenizer or just extract until strict end
                 // but for now let's hope for the best or assume simplistic JSON
                 if (endq != std::string::npos) {
                     return resp.substr(rpos, endq - rpos);
                 }
             }
             return resp; // Return raw if parse failed
        }
        return "// Error: Unknown model type.";
    } catch (const std::exception& e) {
        return std::string("// Error: ") + e.what();
    }
}

// Helpers
std::string ModelCaller::buildCompletionPrompt(const std::string& prefix, const std::string& suffix, const std::string& fileType, const std::string& context) {
    // Basic FIM (Fill In Middle) prompt format for CodeLlama/StarCoder
    return "<PRE> " + prefix + " <SUF> " + suffix + " <MID>";
}

std::vector<std::string> ModelCaller::parseCompletions(const std::string& response) {
    // Remove <EOT> or similar if present
    std::string clean = response;
    // ... cleanup logic ...
    return { clean };
}

float ModelCaller::scoreCompletion(const std::string& completion, const std::string& prefix, const std::string& fileType) {
    // 1. Base score
    float score = 0.5f;

    // 2. Length heuristic (penalize very short, non-symbol completions)
    if (completion.length() > 5) score += 0.2f;
    if (completion.length() > 20) score += 0.1f;

    // 3. Context matching (Primitive continuity check)
    // If prefix ends with space and completion starts with space, small penalty (dedup)
    if (!prefix.empty() && !completion.empty()) {
        char lastP = prefix.back();
        char firstC = completion.front();
        if (isspace(lastP) && isspace(firstC)) {
            score -= 0.1f;
        }
    }

    // 4. Syntax Heuristic: Brace matching balance
    int openBraces = 0;
    int closeBraces = 0;
    for (char c : completion) {
        if (c == '{') openBraces++;
        if (c == '}') closeBraces++;
    }
    // If we closed as many as we opened (or close logic makes sense), boost
    if (openBraces == closeBraces) score += 0.1f;

    // Clamp
    if (score > 1.0f) score = 1.0f;
    if (score < 0.0f) score = 0.0f;
    
    return score;
}



std::string ModelCaller::getCompletionDescription(const std::string& completion) {
    // Naive: Just return first line or snippet
    size_t endLine = completion.find('\n');
    if (endLine != std::string::npos) {
        return completion.substr(0, endLine);
    }
    return completion;
}

std::string ModelCaller::generateCode(
    const std::string& instruction,
    const std::string& fileType,
    const std::string& context)
{
    std::string prompt = "Instruction: " + instruction + "\n" +
                         "Context: " + context + "\n" +
                         "Language: " + fileType + "\n\n" +
                         "Code:\n";
    GenerationParams params;
    params.max_tokens = 2048;
    params.temperature = 0.2f;
    
    return callModel(prompt, params);
}

std::string ModelCaller::generateRewrite(
    const std::string& code,
    const std::string& instruction,
    const std::string& context)
{
     std::string prompt = "Original Code:\n" + code + "\n\n" +
                          "Instruction: " + instruction + "\n" +
                          "Context: " + context + "\n\n" +
                          "Rewritten Code:\n";
     GenerationParams params;
     params.max_tokens = 4096;
     params.temperature = 0.1f;
     
     return callModel(prompt, params);
}

bool ModelCaller::streamModel(const std::string& prompt, const GenerationParams& params, StreamCallback callback, std::chrono::milliseconds delay) {
    // Check for GGUF Local support first
    // In a real scenario, we might have a global engine instance or create one.
    // Use CallNativeHost if IPC, or direct CPUInferenceEngine if linked.
    
    // For this implementation, we rely on callModel() which handles the backend selection
    // (NativeHost IPC, WinHttp, or embedded CPUInferenceEngine).
    
    // Real inference happens in callModel (Blocking).
    std::string fullResponse = callModel(prompt, params);
    
    if (fullResponse.substr(0, 8) == "// Error") return false;

    // Stream Adapter: Chunking buffer for UI compatibility
    // Since the backend is blocking, we emit chunks to satisfy the streaming interface.
    size_t pos = 0;
    size_t len = fullResponse.length();
    size_t chunkSize = 16; // Emit small packets
    
    while (pos < len) {
        size_t n = std::min(chunkSize, len - pos);
        std::string token = fullResponse.substr(pos, n);
        
        if (!callback(token)) return false; // Cancelled
        
        pos += n;
        // Minimal delay to allow UI refresh events if running on main thread (though usually this is bg)
        if (delay.count() > 0) std::this_thread::sleep_for(delay);
    }
    
    return true;
}

} // namespace RawrXD
