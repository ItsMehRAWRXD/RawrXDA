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
        // Try Native IPC first (Production Path — MASM pipe server)
        std::string nativeResp = CallNativeHost(prompt);
        if (!nativeResp.empty()) {
            return nativeResp;
        }

        AgenticConfiguration& config = AgenticConfiguration::getInstance();
        
        std::string modelType = config.get("model_type", "auto");
        std::string modelPath = config.getModelPath();

        // ====================================================================
        // AUTO mode: try local GGUF first, fall back to Ollama automatically
        // ====================================================================
        if (modelType == "auto" || modelType == "local" || modelType == "gguf") {
            static std::unique_ptr<CPUInferenceEngine> engine;
            static std::string currentLoadedModel;
            static std::mutex engineMutex;
            static bool localFailed = false;  // Remember if local load already failed
            
            std::lock_guard<std::mutex> lock(engineMutex);

            if (!localFailed) {
                if (!engine) {
                    engine = std::make_unique<CPUInferenceEngine>();
                    unsigned int threads = std::thread::hardware_concurrency();
                    if (threads == 0) threads = 4;
                    engine->SetThreadCount(threads);
                }

                if (currentLoadedModel != modelPath) {
                    bool loaded = false;
                    if (fs::exists(modelPath)) {
                        loaded = engine->LoadModel(modelPath);
                    }
                    if (!loaded) {
                        // Try fallback paths
                        std::string fallbackPath = "models/model.gguf";
                        if (!fs::exists(fallbackPath)) fallbackPath = "../models/model.gguf";
                        if (fs::exists(fallbackPath)) {
                            loaded = engine->LoadModel(fallbackPath);
                            if (loaded) modelPath = fallbackPath;
                        }
                    }
                    if (loaded) {
                        currentLoadedModel = modelPath;
                    } else {
                        // Local load failed — fall through to Ollama
                        localFailed = true;
                    }
                }

                if (engine && engine->isModelLoaded()) {
                    auto inputTokens = engine->Tokenize(prompt);
                    auto outputTokens = engine->Generate(inputTokens, params.max_tokens);
                    std::string result = engine->Detokenize(outputTokens);
                    if (!result.empty()) return result;
                }
            }

            // If modelType was explicitly "local"/"gguf" and failed, don't fall through
            if (modelType != "auto" && localFailed) {
                return "// Error: Local model failed to load. No model at " + modelPath + ". Set model_type to 'ollama' or 'auto'.";
            }
        }

        // ====================================================================
        // Ollama path — works for "ollama" and "auto" (fallback)
        // ====================================================================
        if (modelType == "ollama" || modelType == "auto") {
            std::wstring wendpoint = L"localhost";
            int port = 11434;

            // Determine model name for Ollama
            std::string ollamaModel = modelPath;
            
            // If modelPath points to a file (not an Ollama model name), try defaults
            if (ollamaModel.find('/') != std::string::npos || 
                ollamaModel.find('\\') != std::string::npos ||
                ollamaModel.find(".gguf") != std::string::npos) {
                // Try config for Ollama model name, else use a sensible default
                ollamaModel = config.get("ollama_model", "");
                if (ollamaModel.empty()) {
                    // Auto-detect from /api/tags
                    std::string tagsResp = WinHttpPost(wendpoint, port, L"/api/tags", "");
                    // Actually this needs GET, let's use a quick GET helper
                    // Since WinHttpPost sends POST which returns 405 on /api/tags,
                    // we need a GET. Use direct WinHTTP GET:
                    HINTERNET hS = WinHttpOpen(L"RawrXD/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
                    if (hS) {
                        HINTERNET hC = WinHttpConnect(hS, L"localhost", 11434, 0);
                        if (hC) {
                            HINTERNET hR = WinHttpOpenRequest(hC, L"GET", L"/api/tags", 
                                NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
                            if (hR) {
                                if (WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) &&
                                    WinHttpReceiveResponse(hR, NULL)) {
                                    DWORD sz = 0, rd = 0;
                                    tagsResp.clear();
                                    do {
                                        if (!WinHttpQueryDataAvailable(hR, &sz) || sz == 0) break;
                                        std::vector<char> buf(sz + 1);
                                        if (WinHttpReadData(hR, buf.data(), sz, &rd))
                                            tagsResp.append(buf.data(), rd);
                                    } while (sz > 0);
                                }
                                WinHttpCloseHandle(hR);
                            }
                            WinHttpCloseHandle(hC);
                        }
                        WinHttpCloseHandle(hS);
                    }

                    // Extract first model name from tags response
                    if (!tagsResp.empty()) {
                        size_t namePos = tagsResp.find("\"name\":\"");
                        if (namePos != std::string::npos) {
                            namePos += 8;
                            size_t endQ = tagsResp.find("\"", namePos);
                            if (endQ != std::string::npos) {
                                ollamaModel = tagsResp.substr(namePos, endQ - namePos);
                            }
                        }
                    }
                    if (ollamaModel.empty()) ollamaModel = "llama3.2";
                }
            }

            // Properly escape the prompt for JSON
            std::string safePrompt;
            safePrompt.reserve(prompt.size() + 32);
            for (char c : prompt) {
                switch (c) {
                    case '"':  safePrompt += "\\\""; break;
                    case '\\': safePrompt += "\\\\"; break;
                    case '\n': safePrompt += "\\n";  break;
                    case '\r': safePrompt += "\\r";  break;
                    case '\t': safePrompt += "\\t";  break;
                    default:   safePrompt += c;      break;
                }
            }

            std::string body = "{\"model\":\"" + ollamaModel + "\","
                "\"prompt\":\"" + safePrompt + "\","
                "\"stream\":false,"
                "\"options\":{\"temperature\":" + std::to_string(params.temperature) +
                ",\"num_predict\":" + std::to_string(params.max_tokens) + "}}";

            std::string resp = WinHttpPost(wendpoint, port, L"/api/generate", body);

            if (resp.empty()) return "// Error: Failed to connect to Ollama at localhost:11434";

            // Parse "response" field with proper escape handling
            size_t rpos = resp.find("\"response\":\"");
            if (rpos != std::string::npos) {
                rpos += 12;
                std::string extracted;
                for (size_t i = rpos; i < resp.size(); i++) {
                    if (resp[i] == '\\' && i + 1 < resp.size()) {
                        char esc = resp[i + 1];
                        if (esc == '"')  { extracted += '"';  i++; }
                        else if (esc == '\\') { extracted += '\\'; i++; }
                        else if (esc == 'n')  { extracted += '\n'; i++; }
                        else if (esc == 'r')  { extracted += '\r'; i++; }
                        else if (esc == 't')  { extracted += '\t'; i++; }
                        else { extracted += resp[i]; }
                    } else if (resp[i] == '"') {
                        break;
                    } else {
                        extracted += resp[i];
                    }
                }
                return extracted;
            }
            return resp; // Return raw if parse failed
        }

        return "// Error: Unknown model type '" + modelType + "'. Use 'auto', 'local', or 'ollama'.";
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
