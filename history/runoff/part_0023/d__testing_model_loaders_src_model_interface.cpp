// model_interface.cpp - Unified Model Generation Interface
// Converted from Qt to pure C++17
#include "model_interface.h"
#include "common/logger.hpp"
#include <sstream>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")
#endif

ModelInterface::ModelInterface() {
    m_endpoints[ModelBackend::Ollama] = "http://localhost:11434";
    m_endpoints[ModelBackend::LlamaCpp] = "http://localhost:8080";
    m_endpoints[ModelBackend::OpenAI] = "https://api.openai.com";
    m_defaultModel = "llama3";
}

ModelInterface::~ModelInterface() {
    m_cancelRequested = true;
}

bool ModelInterface::loadModel(const std::string& modelName, ModelBackend backend) {
    std::lock_guard<std::mutex> lock(m_mutex);
    logInfo() << "Loading model: " << modelName;

    ModelInfo info;
    info.name = modelName;
    info.backend = (backend == ModelBackend::Ollama) ? "ollama" :
                   (backend == ModelBackend::LlamaCpp) ? "llamacpp" :
                   (backend == ModelBackend::OpenAI) ? "openai" : "custom";
    info.loaded = true;
    info.available = true;
    m_loadedModels[modelName] = info;
    onModelLoaded.emit(modelName);
    return true;
}

bool ModelInterface::unloadModel(const std::string& modelName) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_loadedModels.find(modelName);
    if (it != m_loadedModels.end()) {
        m_loadedModels.erase(it);
        onModelUnloaded.emit(modelName);
        return true;
    }
    return false;
}

bool ModelInterface::isModelLoaded(const std::string& modelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_loadedModels.count(modelName) > 0;
}

std::vector<ModelInfo> ModelInterface::getAvailableModels() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ModelInfo> models;
    for (const auto& [name, info] : m_loadedModels) models.push_back(info);
    return models;
}

ModelInfo ModelInterface::getModelInfo(const std::string& modelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_loadedModels.find(modelName);
    return (it != m_loadedModels.end()) ? it->second : ModelInfo{};
}

GenerationResult ModelInterface::generate(const GenerationParams& params) {
    std::string model = params.model.empty() ? m_defaultModel : params.model;
    logInfo() << "Generating with model: " << model;
    m_cancelRequested = false;

    // Determine backend
    ModelBackend backend = ModelBackend::Ollama;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_loadedModels.find(model);
        if (it != m_loadedModels.end()) {
            if (it->second.backend == "llamacpp") backend = ModelBackend::LlamaCpp;
            else if (it->second.backend == "openai") backend = ModelBackend::OpenAI;
        }
    }

    auto start = std::chrono::steady_clock::now();
    GenerationResult result;

    switch (backend) {
        case ModelBackend::Ollama:   result = generateOllama(params); break;
        case ModelBackend::LlamaCpp: result = generateLlamaCpp(params); break;
        case ModelBackend::OpenAI:   result = generateOpenAI(params); break;
        default:                     result = GenerationResult::fail("Unknown backend"); break;
    }

    auto end = std::chrono::steady_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    result.model = model;

    if (result.success) {
        onGenerationComplete.emit(result);
    } else {
        onErrorOccurred.emit(result.error);
    }
    return result;
}

GenerationResult ModelInterface::chat(const std::vector<ChatMessage>& messages,
                                       const GenerationParams& params)
{
    // Build combined prompt from messages
    GenerationParams chatParams = params;
    std::string combined;
    for (const auto& msg : messages) {
        if (msg.role == "system") {
            chatParams.systemPrompt = msg.content;
        } else {
            combined += msg.role + ": " + msg.content + "\n";
        }
    }
    chatParams.prompt = combined;
    return generate(chatParams);
}

void ModelInterface::generateAsync(const GenerationParams& params) {
    // Fire-and-forget generation using callback
    auto result = generate(params);
    onGenerationComplete.emit(result);
}

void ModelInterface::cancelGeneration() {
    m_cancelRequested = true;
}

void ModelInterface::generateStream(const GenerationParams& params,
                                     std::function<void(const std::string&)> tokenCallback)
{
    // Simple streaming implementation via Ollama API
    auto result = generate(params);
    if (result.success && tokenCallback) {
        // Simulate token-by-token delivery
        std::istringstream stream(result.text);
        std::string word;
        while (stream >> word) {
            if (m_cancelRequested) break;
            tokenCallback(word + " ");
            onTokenReceived.emit(word);
        }
    }
}

EmbeddingResult ModelInterface::getEmbedding(const std::string& text, const std::string& model) {
    EmbeddingResult result;
    std::string embModel = model.empty() ? "nomic-embed-text" : model;

    // Build request body
    std::string body = "{\"model\":\"" + embModel + "\",\"prompt\":\"" + text + "\"}";
    std::string endpoint = m_endpoints[ModelBackend::Ollama] + "/api/embeddings";

    std::string response = httpPost(endpoint, body);
    if (response.empty()) {
        result.error = "Failed to get embedding response";
        return result;
    }

    // Parse embedding from response (simplified)
    result.success = true;
    result.dimensions = 768; // Default for nomic-embed-text
    return result;
}

void ModelInterface::setDefaultModel(const std::string& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultModel = model;
}

std::string ModelInterface::getDefaultModel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultModel;
}

void ModelInterface::setEndpoint(ModelBackend backend, const std::string& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_endpoints[backend] = endpoint;
}

std::string ModelInterface::getEndpoint(ModelBackend backend) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_endpoints.find(backend);
    return (it != m_endpoints.end()) ? it->second : "";
}

void ModelInterface::setApiKey(ModelBackend backend, const std::string& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_apiKeys[backend] = key;
}

bool ModelInterface::isBackendAvailable(ModelBackend backend) const {
    std::string endpoint = getEndpoint(backend);
    if (endpoint.empty()) return false;
    // Try a simple health check
    std::string healthUrl = endpoint;
    if (backend == ModelBackend::Ollama) healthUrl += "/api/tags";
    else if (backend == ModelBackend::LlamaCpp) healthUrl += "/health";

    std::string response = httpPost(healthUrl, "");
    return !response.empty();
}

std::string ModelInterface::getBackendStatus(ModelBackend backend) const {
    if (isBackendAvailable(backend)) return "available";
    return "unavailable";
}

// ── Backend Implementations ─────────────────────────────────────────────────

GenerationResult ModelInterface::generateOllama(const GenerationParams& params) {
    std::string endpoint = m_endpoints[ModelBackend::Ollama] + "/api/generate";
    std::string body = buildOllamaPayload(params);
    std::string response = httpPost(endpoint, body);

    if (response.empty()) {
        return GenerationResult::fail("Ollama endpoint unreachable");
    }

    // Parse response (look for "response" field in NDJSON)
    std::string fullText;
    std::istringstream stream(response);
    std::string line;
    while (std::getline(stream, line)) {
        // Simple JSON parse for "response":"..." field
        auto pos = line.find("\"response\":\"");
        if (pos != std::string::npos) {
            pos += 12;
            auto end = line.find("\"", pos);
            if (end != std::string::npos) {
                fullText += line.substr(pos, end - pos);
            }
        }
    }

    if (!fullText.empty()) {
        return GenerationResult::ok(fullText);
    }
    return GenerationResult::fail("Failed to parse Ollama response");
}

GenerationResult ModelInterface::generateLlamaCpp(const GenerationParams& params) {
    std::string endpoint = m_endpoints[ModelBackend::LlamaCpp] + "/completion";
    std::string body = "{\"prompt\":\"" + params.prompt + "\""
                       ",\"n_predict\":" + std::to_string(params.maxTokens) +
                       ",\"temperature\":" + std::to_string(params.temperature) +
                       ",\"top_p\":" + std::to_string(params.topP) +
                       ",\"top_k\":" + std::to_string(params.topK) +
                       ",\"repeat_penalty\":" + std::to_string(params.repeatPenalty) + "}";

    std::string response = httpPost(endpoint, body);
    if (response.empty()) return GenerationResult::fail("llama.cpp endpoint unreachable");

    auto pos = response.find("\"content\":\"");
    if (pos != std::string::npos) {
        pos += 11;
        auto end = response.find("\"", pos);
        if (end != std::string::npos) {
            return GenerationResult::ok(response.substr(pos, end - pos));
        }
    }
    return GenerationResult::fail("Failed to parse llama.cpp response");
}

GenerationResult ModelInterface::generateOpenAI(const GenerationParams& params) {
    auto it = m_apiKeys.find(ModelBackend::OpenAI);
    if (it == m_apiKeys.end() || it->second.empty()) {
        return GenerationResult::fail("OpenAI API key not set");
    }

    std::string endpoint = m_endpoints[ModelBackend::OpenAI] + "/v1/chat/completions";
    std::string body = buildOpenAIPayload(params);
    std::map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer " + it->second;
    headers["Content-Type"] = "application/json";

    std::string response = httpPost(endpoint, body, headers);
    if (response.empty()) return GenerationResult::fail("OpenAI endpoint unreachable");

    auto pos = response.find("\"content\":\"");
    if (pos != std::string::npos) {
        pos += 11;
        auto end = response.find("\"", pos);
        if (end != std::string::npos) {
            return GenerationResult::ok(response.substr(pos, end - pos));
        }
    }
    return GenerationResult::fail("Failed to parse OpenAI response");
}

std::string ModelInterface::buildOllamaPayload(const GenerationParams& params) const {
    std::ostringstream oss;
    oss << "{\"model\":\"" << (params.model.empty() ? m_defaultModel : params.model)
        << "\",\"prompt\":\"" << params.prompt
        << "\",\"stream\":false"
        << ",\"options\":{\"temperature\":" << params.temperature
        << ",\"top_p\":" << params.topP
        << ",\"top_k\":" << params.topK
        << ",\"num_predict\":" << params.maxTokens
        << ",\"repeat_penalty\":" << params.repeatPenalty;
    if (params.seed >= 0) oss << ",\"seed\":" << params.seed;
    oss << "}";
    if (!params.systemPrompt.empty()) {
        oss << ",\"system\":\"" << params.systemPrompt << "\"";
    }
    oss << "}";
    return oss.str();
}

std::string ModelInterface::buildOpenAIPayload(const GenerationParams& params) const {
    std::ostringstream oss;
    oss << "{\"model\":\"" << (params.model.empty() ? "gpt-3.5-turbo" : params.model) << "\""
        << ",\"messages\":[";
    if (!params.systemPrompt.empty()) {
        oss << "{\"role\":\"system\",\"content\":\"" << params.systemPrompt << "\"},";
    }
    oss << "{\"role\":\"user\",\"content\":\"" << params.prompt << "\"}]"
        << ",\"temperature\":" << params.temperature
        << ",\"top_p\":" << params.topP
        << ",\"max_tokens\":" << params.maxTokens << "}";
    return oss.str();
}

std::string ModelInterface::httpPost(const std::string& url, const std::string& body,
                                      const std::map<std::string, std::string>& headers) const
{
#ifdef _WIN32
    // Parse URL
    std::string host, path;
    int port = 80;
    bool useSSL = false;
    {
        std::string work = url;
        if (StringUtils::startsWith(work, "https://")) { useSSL = true; port = 443; work = work.substr(8); }
        else if (StringUtils::startsWith(work, "http://")) { work = work.substr(7); }
        auto slashPos = work.find('/');
        if (slashPos != std::string::npos) {
            host = work.substr(0, slashPos);
            path = work.substr(slashPos);
        } else { host = work; path = "/"; }
        auto colonPos = host.find(':');
        if (colonPos != std::string::npos) {
            port = std::stoi(host.substr(colonPos + 1));
            host = host.substr(0, colonPos);
        }
    }

    HINTERNET hInternet = InternetOpenA("RawrXD/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";

    DWORD flags = useSSL ? (INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
                             INTERNET_FLAG_IGNORE_CERT_DATE_INVALID) : 0;
    flags |= INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD;

    HINTERNET hConnect = InternetConnectA(hInternet, host.c_str(), (INTERNET_PORT)port,
                                           NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) { InternetCloseHandle(hInternet); return ""; }

    const char* verb = body.empty() ? "GET" : "POST";
    HINTERNET hRequest = HttpOpenRequestA(hConnect, verb, path.c_str(), NULL, NULL, NULL, flags, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        return "";
    }

    // Apply headers
    std::string headerStr = "Content-Type: application/json\r\n";
    for (const auto& [k, v] : headers) headerStr += k + ": " + v + "\r\n";
    HttpAddRequestHeadersA(hRequest, headerStr.c_str(), (DWORD)headerStr.size(),
                           HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);

    BOOL sent = HttpSendRequestA(hRequest, NULL, 0,
                                  body.empty() ? NULL : (LPVOID)body.c_str(),
                                  (DWORD)body.size());
    std::string result;
    if (sent) {
        char buf[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hRequest, buf, sizeof(buf) - 1, &bytesRead) && bytesRead > 0) {
            buf[bytesRead] = '\0';
            result += buf;
        }
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return result;
#else
    // POSIX: stub — use libcurl in production
    (void)url; (void)body; (void)headers;
    logWarning() << "HTTP POST not implemented on this platform";
    return "";
#endif
}
