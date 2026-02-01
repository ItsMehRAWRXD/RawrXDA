/**
 * @file model_invoker.cpp
 * @brief Implementation of LLM invocation layer
 *
 * Provides synchronous and asynchronous wish→plan transformation
 * with support for Ollama (local) and cloud LLMs.
 */

#include "model_invoker.hpp"
#include <regex>
#include <future>
#include <algorithm>
#include <iostream>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <filesystem>
#include <mutex>

// Explicit Logic Integration: Bring in the real CPU Inference Engine
#include "../cpu_inference_engine.h"

// Global static instance for embedded inference (Lazy initialized)
// Using native raw pointer or unique_ptr to avoid header dependency issues if unique_ptr undefined
static std::unique_ptr<RawrXD::CPUInferenceEngine> g_embeddedEngine;
static std::mutex g_engineMutex;  

#pragma comment(lib, "winhttp.lib")

/**
 * @brief Constructor
 */
ModelInvoker::ModelInvoker()
{
    m_backend = "ollama";
    m_endpoint = "http://localhost:11434";
}

/**
 * @brief Destructor
 */
ModelInvoker::~ModelInvoker() = default;

/**
 * @brief Set the LLM backend and endpoint
 */
void ModelInvoker::setLLMBackend(const std::string& backend,
                                  const std::string& endpoint,
                                  const std::string& apiKey)
{
    m_backend = backend;
    std::transform(m_backend.begin(), m_backend.end(), m_backend.begin(), ::tolower);
    m_endpoint = endpoint;
    m_apiKey = apiKey;

    // Set default model based on backend
    if (m_backend == "ollama") {
        m_model = "mistral";
    } else if (m_backend == "claude") {
        m_model = "claude-3-sonnet-20240229";
    } else if (m_backend == "openai") {
        m_model = "gpt-4-turbo";
    }
}

/**
 * @brief Set custom system prompt template
 */
void ModelInvoker::setSystemPromptTemplate(const std::string& template_)
{
    m_customSystemPrompt = template_;
}

/**
 * @brief Set codebase embeddings for RAG
 */
void ModelInvoker::setCodebaseEmbeddings(const std::map<std::string, float>& embeddings)
{
    m_codebaseEmbeddings = embeddings;
}

/**
 * @brief Synchronous invocation (blocks caller)
 */
LLMResponse ModelInvoker::invoke(const InvocationParams& params)
{
    // Check cache first
    if (m_cachingEnabled) {
        std::string cacheKey = getCacheKey(params);
        LLMResponse cached = getCachedResponse(cacheKey);
        if (cached.success) {
            return cached;
        }
    }

    m_isInvoking = true;
    if (onPlanGenerationStarted) {
        onPlanGenerationStarted(params.wish);
    }

    LLMResponse response;

    try {
        // Build prompts
        std::string systemPrompt = m_customSystemPrompt.empty()
                                   ? buildSystemPrompt(params.availableTools)
                                   : m_customSystemPrompt;
        std::string userMessage = buildUserMessage(params);

        nlohmann::json llmResponse;

        // Invoke appropriate backend
        if (m_backend == "ollama") {
            llmResponse = sendOllamaRequest(m_model, userMessage, params.maxTokens, params.temperature);
        } else if (m_backend == "claude") {
            llmResponse = sendClaudeRequest(userMessage, params.maxTokens, params.temperature);
        } else if (m_backend == "openai") {
            llmResponse = sendOpenAIRequest(userMessage, params.maxTokens, params.temperature);
        } else if (m_backend == "embedded") {
            // Explicit Logic: Use local Titan Engine
            std::lock_guard<std::mutex> lock(g_engineMutex);
            if (!g_embeddedEngine) {
                g_embeddedEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
                // Treat m_endpoint as model path if provided
                std::string modelPath = m_endpoint;
                if (modelPath.starts_with("http") || modelPath.empty()) modelPath = "models/titan-model.gguf"; 
                
                if (!g_embeddedEngine->loadModel(modelPath)) {
                     response.error = "Failed to load embedded model: " + modelPath;
                     m_isInvoking = false;
                     if (onInvocationError) onInvocationError(response.error, true);
                     return response;
                }
            }
            
            // Perform Inference
            response.rawOutput = g_embeddedEngine->infer(userMessage);
            response.tokensUsed = (int)response.rawOutput.length() / 4; 
            
            // Use dummy JSON to bypass empty check below, but we will check backend type 
            llmResponse = nlohmann::json::object(); 
            llmResponse["content"] = response.rawOutput; 
        } else if (m_backend == "rawrxd") {
            // [NEW] RawrXD_Agent.exe IPC Backend
             llmResponse = sendRawrXDRequest(userMessage, params.maxTokens, params.temperature);
        } else {
            response.error = "Unknown backend: " + m_backend;
            m_isInvoking = false;
            return response;
        }

        // Extract response text
        if (llmResponse.empty()) {
            response.error = "Empty response from LLM";
            m_isInvoking = false;
            if (onInvocationError) onInvocationError(response.error, true);
            return response;
        }

        // Parse backend-specific response format
        if (m_backend == "ollama") {
            if (llmResponse.contains("response")) response.rawOutput = llmResponse["response"].get<std::string>();
            if (llmResponse.contains("eval_count") && llmResponse.contains("prompt_eval_count")) {
                response.tokensUsed = llmResponse["eval_count"].get<int>() + 
                                      llmResponse["prompt_eval_count"].get<int>();
            }
        } else if (m_backend == "claude") {
            if (llmResponse.contains("content") && llmResponse["content"].is_array() && !llmResponse["content"].empty()) {
                response.rawOutput = llmResponse["content"][0]["text"].get<std::string>();
            }
            if (llmResponse.contains("usage")) {
                response.tokensUsed = llmResponse["usage"]["output_tokens"].get<int>();
            }
        } else if (m_backend == "openai") {
            if (llmResponse.contains("choices") && llmResponse["choices"].is_array() && !llmResponse["choices"].empty()) {
                response.rawOutput = llmResponse["choices"][0]["message"]["content"].get<std::string>();
            }
            if (llmResponse.contains("usage")) {
                response.tokensUsed = llmResponse["usage"]["completion_tokens"].get<int>();
            }
        } else if (m_backend == "rawrxd") {
            // RawrXD returns direct text in "response"
            if (llmResponse.contains("response")) {
                response.rawOutput = llmResponse["response"].get<std::string>();
            }
        }

        if (params.enforceJsonFormat) {
            // Parse into structured plan
            response.parsedPlan = parsePlan(response.rawOutput);

            // Validate sanity
            if (!validatePlanSanity(response.parsedPlan)) {
                response.error = "Plan failed sanity checks";
                response.success = false;
                m_isInvoking = false;
                if (onInvocationError) onInvocationError(response.error, true);
                return response;
            }
        }

        response.success = true;
        if (llmResponse.contains("reasoning")) response.reasoning = llmResponse["reasoning"].get<std::string>();

        // Cache successful response
        if (m_cachingEnabled) {
            cacheResponse(getCacheKey(params), response);
        }

    } catch (const std::exception& e) {
        response.error = "Exception: " + std::string(e.what());
        response.success = false;
        if (onInvocationError) onInvocationError(response.error, false);
    }

    m_isInvoking = false;
    return response;
}

/**
 * @brief Asynchronous invocation (non-blocking)
 */
void ModelInvoker::invokeAsync(const InvocationParams& params)
{
    std::thread([this, params]() {
        LLMResponse response = invoke(params);
        if (onPlanGenerated) {
            onPlanGenerated(response);
        }
    }).detach();
}

/**
 * @brief Cancel pending request
 */
void ModelInvoker::cancelPendingRequest()
{
    m_isInvoking = false;
}

/**
 * @brief Build system prompt with tool descriptions
 */
std::string ModelInvoker::buildSystemPrompt(const std::vector<std::string>& tools)
{
    std::string prompt = R"(You are an intelligent IDE agent for the RawrXD code generation framework.

Your role is to transform natural language wishes into structured action plans that can be executed by an automated system.

# Available Tools
You can use the following tools:
)";

    for (const auto& tool : tools) {
        prompt += "- " + tool + "\n";
    }

    prompt += R"(
# Response Format
You MUST respond with a valid JSON array of actions. Each action must have:
- type: string (action type name)
- target: string (file, command, or target)
- params: object (action-specific parameters)
- description: string (human-readable description)

Example:
```json
[
  {
    "type": "search_files",
    "target": "src/",
    "params": { "pattern": "*.{cpp,hpp,h,c}", "query": "TODO|FIXME|XXX" },
    "description": "Scan C/C++ source files for temporary markers and technical debt."
  }
]
```

# Constraints
- Do NOT suggest destructive operations without explicit user intent
- Do NOT modify system files or configuration files without user approval
- Do NOT create infinite loops or recursive procedures
- Always break complex tasks into manageable steps
- Use existing patterns found in the codebase

# Context
The system is RawrXD: A production-grade IDE for GGUF quantization and model serving.
Current capabilities include: file search, text editing, project builds, test execution, and code generation.
)";

    return prompt;
}

/**
 * @brief Build user message with wish and context
 */
std::string ModelInvoker::buildUserMessage(const InvocationParams& params)
{
    std::string message = "User Wish: " + params.wish + "\n\n";

    if (!params.context.empty()) {
        message += "Context: " + params.context + "\n\n";
    }

    if (!params.codebaseContext.empty()) {
        message += "Relevant Codebase:\n" + params.codebaseContext + "\n\n";
    }

    if (params.enforceJsonFormat) {
        message += "Please generate a structured action plan to fulfill this wish. "
                   "Respond with ONLY valid JSON array, no additional text.";
    } else {
        // Raw text mode (e.g. for code completion)
        message += "Please complete the code. Respond with ONLY the code, no markdown block markers.";
    }

    return message;
}

nlohmann::json ModelInvoker::sendOllamaRequest(const std::string& model,
                                                const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = model;
    payload["prompt"] = prompt;
    payload["temperature"] = temperature;
    payload["num_predict"] = maxTokens;
    payload["stream"] = false;

    std::string url = m_endpoint + "/api/generate";
    std::string responseData = performHttpRequest(url, "POST", payload.dump(), {{"Content-Type", "application/json"}});

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

nlohmann::json ModelInvoker::sendClaudeRequest(const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;
    payload["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});

    std::string url = (m_endpoint == "https://api.anthropic.com") ? m_endpoint + "/v1/messages" : m_endpoint + "/v1/messages";
    std::string responseData = performHttpRequest(url, "POST", payload.dump(), {
        {"Content-Type", "application/json"},
        {"x-api-key", m_apiKey},
        {"anthropic-version", "2023-06-01"}
    });

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

nlohmann::json ModelInvoker::sendOpenAIRequest(const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = m_model;
    payload["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;

    std::string url = (m_endpoint == "https://api.openai.com") ? m_endpoint + "/v1/chat/completions" : m_endpoint + "/v1/chat/completions";
    std::string responseData = performHttpRequest(url, "POST", payload.dump(), {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer " + m_apiKey}
    });

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

    // [NEW] RawrXD IPC Implementation
nlohmann::json ModelInvoker::sendRawrXDRequest(const std::string& messageBlock, int maxTokens, float temperature) {
    nlohmann::json result;
    result["response"] = "";

    HANDLE hPipe = CreateFileA(
        "\\\\.\\pipe\\RawrXD_IPC",
        GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL);

    if (hPipe == INVALID_HANDLE_VALUE) {
        // Fallback or silent fail
        return result; 
    }

    // Tokenize Locally (Simple ASCII/Byte Packing for now)
    // We convert std::string chars to int32 tokens so the ASM backend receives the expected format.
    std::vector<int32_t> tokens;
    for (unsigned char c : messageBlock) {
        tokens.push_back(static_cast<int32_t>(c));
    }

    // Send Prompt
    DWORD bytesWritten;
    bool success = WriteFile(hPipe, tokens.data(), tokens.size() * sizeof(int32_t), &bytesWritten, NULL);
    
    if (success) {
        int32_t buffer[2048]; // Read up to 2048 tokens
        DWORD bytesRead;
        if (ReadFile(hPipe, buffer, sizeof(buffer), &bytesRead, NULL)) {
            // Detokenize (Int32 -> Char)
            std::string resp;
            int count = bytesRead / sizeof(int32_t);
            for (int i = 0; i < count; ++i) {
                int32_t tok = buffer[i];
                if (tok < 256 && tok > 0) {
                    resp += static_cast<char>(tok);
                }
            }
             result["response"] = resp;
        }
    }

    CloseHandle(hPipe);
    return result;
}

/**
 * @brief Invoke Ollama API
 */
nlohmann::json ModelInvoker::sendOllamaRequest(const std::string& model, 
                                                const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = model;
    payload["prompt"] = prompt;
    payload["temperature"] = temperature;
    payload["num_predict"] = maxTokens;
    payload["stream"] = false;

    std::string url = m_endpoint + "/api/generate";
    std::string responseData = performHttpRequest(url, "POST", payload.dump(), {{"Content-Type", "application/json"}});

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

nlohmann::json ModelInvoker::sendClaudeRequest(const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;
    payload["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});

    std::string url = (m_endpoint == "https://api.anthropic.com") ? m_endpoint + "/v1/messages" : m_endpoint + "/v1/messages";
    std::string responseData = performHttpRequest(url, "POST", payload.dump(), {
        {"Content-Type", "application/json"},
        {"x-api-key", m_apiKey},
        {"anthropic-version", "2023-06-01"}
    });

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

nlohmann::json ModelInvoker::sendOpenAIRequest(const std::string& prompt,
                                                int maxTokens,
                                                double temperature)
{
    nlohmann::json payload;
    payload["model"] = m_model;
    payload["messages"] = nlohmann::json::array({{{"role", "user"}, {"content", prompt}}});
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;

    std::string url = (m_endpoint == "https://api.openai.com") ? m_endpoint + "/v1/chat/completions" : m_endpoint + "/v1/chat/completions";
    std::string responseData = performHttpRequest(url, "POST", payload.dump(), {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer " + m_apiKey}
    });

    try {
        return nlohmann::json::parse(responseData);
    } catch (...) {
        return nlohmann::json::object();
    }
}

// Helper: HTTP Request implementation via WinHTTP
std::string ModelInvoker::performHttpRequest(const std::string& url, 
                                            const std::string& method, 
                                            const std::string& body, 
                                            const std::map<std::string, std::string>& headers)
{
    // Basic URL parsing (assuming full URL in var)
    // Actually we need to split host/path/port
    std::string hostName;
    std::string path;
    int port = 443;
    bool isHttps = true;

    // Really naive parsing for the sake of example given context constraints
    size_t protocolPos = url.find("://");
    if (protocolPos != std::string::npos) {
        std::string protocol = url.substr(0, protocolPos);
        if (protocol == "http") { isHttps = false; port = 80; }
        
        size_t pathPos = url.find('/', protocolPos + 3);
        if (pathPos != std::string::npos) {
            hostName = url.substr(protocolPos + 3, pathPos - (protocolPos + 3));
            path = url.substr(pathPos);
        } else {
            hostName = url.substr(protocolPos + 3);
            path = "/";
        }
    } else {
        hostName = "localhost";
        path = url;
    }

    size_t colon = hostName.find(':');
    if (colon != std::string::npos) {
        try {
            port = std::stoi(hostName.substr(colon + 1));
            hostName = hostName.substr(0, colon);
        } catch(...) {}
    }

    HINTERNET hSession = WinHttpOpen(L"RawrXD Agent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    std::wstring wHost(hostName.begin(), hostName.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring wPath(path.begin(), path.end());
    std::wstring wMethod(method.begin(), method.end());
    
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    // Add headers
    for (const auto& [key, val] : headers) {
        std::wstring wKey(key.begin(), key.end());
        std::wstring wVal(val.begin(), val.end());
        std::wstring wHeader = wKey + L": " + wVal;
        WinHttpAddRequestHeaders(hRequest, wHeader.c_str(), (DWORD)wHeader.length(), WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Retry loop for robustness
    int maxRetries = 3;
    for (int i = 0; i <= maxRetries; ++i) {
    }

    std::string response;
    DWORD dwSize = 0;
    DWORD dwDownloaded = 0;
    
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        std::vector<char> buffer(dwSize + 1);
        if (!WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) break;
        
        if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body.c_str(), (DWORD)body.length(), (DWORD)body.length(), 0)) {   response.append(buffer.data(), dwDownloaded);
            if (i == maxRetries) {    } while (dwSize > 0);
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);e(hRequest);
                WinHttpCloseHandle(hSession);nect);
                return "";WinHttpCloseHandle(hSession);
            }
            Sleep(1000 * (i + 1)); // Backoff;
            continue;
        }
        nlohmann::json ModelInvoker::parsePlan(const std::string& llmOutput)
        if (!WinHttpReceiveResponse(hRequest, NULL)) {
            if (i == maxRetries) {
                WinHttpCloseHandle(hRequest);{
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return "";
            } blocks
            Sleep(1000 * (i + 1));on\s*([\s\S]*?)\s*```)");
            continue;
        }if (std::regex_search(llmOutput, match, jsonBlock)) {

        // Success           return nlohmann::json::parse(match[1].str());
        break;        } catch (...) {}
    }

    std::string response;ray brackets [ ... ]
    DWORD dwSize = 0;t start = llmOutput.find('[');
    DWORD dwDownloaded = 0;
    ::string::npos && end != std::string::npos && end > start) {
    do {        try {
        dwSize = 0;(llmOutput.substr(start, end - start + 1));
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        std::vector<char> buffer(dwSize + 1);ohmann::json::array();
        if (!WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) break;
        
        response.append(buffer.data(), dwDownloaded);ModelInvoker::validatePlanSanity(const nlohmann::json& plan)
    } while (dwSize > 0);{

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);invalid (sometimes valid)
    WinHttpCloseHandle(hSession);
    
    return response;
}bject()) return false;
   if (!action.contains("type")) return false;
nlohmann::json ModelInvoker::parsePlan(const std::string& llmOutput)        if (!action.contains("target")) return false;
{
    // Try raw JSON parse first
    try {    return true;
        return nlohmann::json::parse(llmOutput);
    } catch (...) {}
onst InvocationParams& params) const
    // Extract from markdown code blocks
    std::regex jsonBlock(R"(```json\s*([\s\S]*?)\s*```)");
    std::smatch match;
    if (std::regex_search(llmOutput, match, jsonBlock)) {    size_t seed = 0;
        try {e3779b9 + (seed<<6) + (seed>>2);
            return nlohmann::json::parse(match[1].str());+ ... (too large?)
        } catch (...) {}
    }

    // Fallback: Try to find array brackets [ ... ]static std::string getCacheDir() {
    size_t start = llmOutput.find('[');tem::path p("cache");
    size_t end = llmOutput.rfind(']');   if (!std::filesystem::exists(p)) {
    if (start != std::string::npos && end != std::string::npos && end > start) {        std::filesystem::create_directories(p);
        try {
            return nlohmann::json::parse(llmOutput.substr(start, end - start + 1));   return p.string();
        } catch (...) {}
    }
oker::getCachedResponse(const std::string& key) const
    return nlohmann::json::array();
}

bool ModelInvoker::validatePlanSanity(const nlohmann::json& plan)
{    std::filesystem::path cacheFile = std::filesystem::path(getCacheDir()) / (key + ".json");
    if (!plan.is_array()) return false;cheFile)) {
    
    // Check for empty plan if that's invalid (sometimes valid);
    if (plan.empty()) return true;
       f >> j;
    for (const auto& action : plan) {nt = j["content"];
        if (!action.is_object()) return false;           resp.model = j["model"];
        if (!action.contains("type")) return false;            resp.durationMs = j["durationMs"];
        if (!action.contains("target")) return false;
    }       } catch(...) {}

    return true;
}    return resp;

std::string ModelInvoker::getCacheKey(const InvocationParams& params) const
{oker::cacheResponse(const std::string& key, const LLMResponse& response)
    // Simple hash of wish + codebase context
    std::hash<std::string> hasher;turn;
    size_t seed = 0;
    seed ^= hasher(params.wish) + 0x9e3779b9 + (seed<<6) + (seed>>2);d::filesystem::path(getCacheDir()) / (key + ".json");
    // seed ^= hasher(params.codebaseContext) + ... (too large?)
    return std::to_string(seed);
}content;
sponse.model;
static std::string getCacheDir() {   j["durationMs"] = response.durationMs;
    std::filesystem::path p("cache");    
    if (!std::filesystem::exists(p)) {tream f(cacheFile);
        std::filesystem::create_directories(p);       f << j.dump(2);
    }    } catch (...) {}
    return p.string();
}
(const std::string& systemPrompt, const std::string& userPrompt, int maxTokens)
LLMResponse ModelInvoker::getCachedResponse(const std::string& key) const
{
    LLMResponse resp;nvoking = true;
    resp.success = false; 

    std::filesystem::path cacheFile = std::filesystem::path(getCacheDir()) / (key + ".json");
    if (std::filesystem::exists(cacheFile)) {
        try {// Construct prompt based on backend capabilities
            std::ifstream f(cacheFile);em prompt for simplicity across backends
            nlohmann::json j;lPrompt = systemPrompt;
            f >> j;mpt.empty()) fullPrompt += "\n\n";
            resp.content = j["content"];       fullPrompt += userPrompt;
            resp.model = j["model"];
            resp.durationMs = j["durationMs"];
            resp.success = true;           llmResponse = sendOllamaRequest(m_model, fullPrompt, maxTokens, 0.7);
        } catch(...) {}end == "claude") {
    }pically supports system prompt in top level, but our helper is simple.
                // We'll wrap in user message for now.
    return resp;   llmResponse = sendClaudeRequest(fullPrompt, maxTokens, 0.7);
}enai") {
    llmResponse = sendOpenAIRequest(fullPrompt, maxTokens, 0.7);
void ModelInvoker::cacheResponse(const std::string& key, const LLMResponse& response)
{
    if (!response.success) return;
    
    std::filesystem::path cacheFile = std::filesystem::path(getCacheDir()) / (key + ".json");ains("response")) response.rawOutput = llmResponse["response"].get<std::string>();
    try {        } else if (m_backend == "claude") {
        nlohmann::json j;("content") && llmResponse["content"].is_array() && !llmResponse["content"].empty()) {
        j["content"] = response.content;tring>();
        j["model"] = response.model;
        j["durationMs"] = response.durationMs;
        & llmResponse["choices"].is_array() && !llmResponse["choices"].empty()) {
        std::ofstream f(cacheFile);]["content"].get<std::string>();
        f << j.dump(2);
    } catch (...) {}
}
        response.success = !response.rawOutput.empty();
LLMResponse ModelInvoker::queryRaw(const std::string& systemPrompt, const std::string& userPrompt, int maxTokens)
{) {
    LLMResponse response;
    m_isInvoking = true;

    try {
        nlohmann::json llmResponse;ing = false;
        
        // Construct prompt based on backend capabilities
        // For now, we prepend system prompt for simplicity across backends
        std::string fullPrompt = systemPrompt;odelInvoker::buildSystemPrompt(const std::vector<std::string>& tools)
        if (!fullPrompt.empty()) fullPrompt += "\n\n";
        fullPrompt += userPrompt;    // Basic system prompt for agent
ramming agent named RawrXD. "
        if (m_backend == "ollama") {                         "You are capable of editing code, running builds, and executing tests on your own environment.\n\n"
            llmResponse = sendOllamaRequest(m_model, fullPrompt, maxTokens, 0.7);JSON action plans to fulfill user wishes.\n"
        } else if (m_backend == "claude") {le tools:\n";
            // Claude typically supports system prompt in top level, but our helper is simple.
            // We'll wrap in user message for now.   prompt += "- " + t + "\n";
            llmResponse = sendClaudeRequest(fullPrompt, maxTokens, 0.7);    }
        } else if (m_backend == "openai") {
            llmResponse = sendOpenAIRequest(fullPrompt, maxTokens, 0.7);pond with a valid JSON array of action objects. "
        }             "Example: [{\"type\": \"edit_source\", \"target\": \"main.cpp\", \"old_code\": \"...\", \"new_code\": \"...\"}]";
    
        // Extract raw output
        if (m_backend == "ollama") {
             if (llmResponse.contains("response")) response.rawOutput = llmResponse["response"].get<std::string>();
        } else if (m_backend == "claude") {
            if (llmResponse.contains("content") && llmResponse["content"].is_array() && !llmResponse["content"].empty()) {
                response.rawOutput = llmResponse["content"][0]["text"].get<std::string>();
            }
        } else if (m_backend == "openai") {
             if (llmResponse.contains("choices") && llmResponse["choices"].is_array() && !llmResponse["choices"].empty()) {
                response.rawOutput = llmResponse["choices"][0]["message"]["content"].get<std::string>();            }        }        response.success = !response.rawOutput.empty();    } catch (const std::exception& e) {        response.success = false;        response.error = e.what();    }    m_isInvoking = false;    return response;}
std::string ModelInvoker::buildSystemPrompt(const std::vector<std::string>& tools)
{
    // Basic system prompt for agent
    std::string prompt = "You are an autonomous AI programming agent named RawrXD. "
                         "You are capable of editing code, running builds, and executing tests on your own environment.\n\n"
                         "You generate JSON action plans to fulfill user wishes.\n"
                         "Available tools:\n";
    for(const auto& t : tools) {
        prompt += "- " + t + "\n";
    }
    
    prompt += "\nRespond with a valid JSON array of action objects. "
              "Example: [{\"type\": \"edit_source\", \"target\": \"main.cpp\", \"old_code\": \"...\", \"new_code\": \"...\"}]";
    
    return prompt;
}
        response.error = e.what();
        response.success = false;
    }

    m_isInvoking = false;
    return response;
}
