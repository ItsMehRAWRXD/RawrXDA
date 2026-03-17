/**
 * @file model_invoker.cpp
 * @brief Implementation of LLM invocation layer
 *
 * Provides synchronous and asynchronous wish→plan transformation
 * with support for Ollama (local) and cloud LLMs.
 */

#include "model_invoker.hpp"

#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <future>
#include <filesystem>
#include <sstream>
#include <regex>

#pragma comment(lib, "winhttp.lib")

/**
 * @brief Constructor - initializes network manager and default settings
 */
ModelInvoker::ModelInvoker()
{
    m_backend = "ollama";
    m_endpoint = "http://localhost:11434";

    // Initialize cache directory
    try {
        std::filesystem::path cacheDir = std::filesystem::temp_directory_path() / "RawrXD/agent_cache";
        if (!std::filesystem::exists(cacheDir)) {
            std::filesystem::create_directories(cacheDir);
        }
    } catch (...) {
        std::cerr << "[ModelInvoker] Failed to initialize cache directory" << std::endl;
    }
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

    std::cout << "[ModelInvoker] Backend set to " << m_backend << " at " << m_endpoint << std::endl;
}

/**
 * @brief Set custom system prompt template
 */
void ModelInvoker::setSystemPromptTemplate(const QString& template_)
{
    m_customSystemPrompt = template_;
}

/**
 * @brief Set codebase embeddings for RAG
 */
void ModelInvoker::setCodebaseEmbeddings(const QMap<QString, float>& embeddings)
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
            std::cout << "[ModelInvoker] Cache hit for: " << params.wish << std::endl;
            return cached;
        }
    }

    std::cout << "[ModelInvoker] Invoking LLM with wish: " << params.wish << std::endl;
    m_isInvoking = true;
    if (m_onPlanGenerationStarted) {
        m_onPlanGenerationStarted(params.wish);
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
        } else {
            response.error = "Unknown backend: " + m_backend;
            m_isInvoking = false;
            return response;
        }

        // Extract response text
        if (llmResponse.empty()) {
            response.error = "Empty response from LLM";
            m_isInvoking = false;
            if (m_onInvocationError) {
                m_onInvocationError(response.error, true);
            }
            return response;
        }

        // Parse backend-specific response format
        if (m_backend == "ollama") {
            response.rawOutput = llmResponse.contains("response") ? llmResponse["response"].get<std::string>() : "";
            response.tokensUsed = (llmResponse.contains("eval_count") ? llmResponse["eval_count"].get<int>() : 0) + 
                                  (llmResponse.contains("prompt_eval_count") ? llmResponse["prompt_eval_count"].get<int>() : 0);
        } else if (m_backend == "claude") {
            if (llmResponse.contains("content") && llmResponse["content"].is_array() && !llmResponse["content"].empty()) {
                response.rawOutput = llmResponse["content"][0].contains("text") ? llmResponse["content"][0]["text"].get<std::string>() : "";
            }
            if (llmResponse.contains("usage") && llmResponse["usage"].contains("output_tokens")) {
                response.tokensUsed = llmResponse["usage"]["output_tokens"].get<int>();
            }
        } else if (m_backend == "openai") {
            if (llmResponse.contains("choices") && llmResponse["choices"].is_array() && !llmResponse["choices"].empty()) {
                response.rawOutput = llmResponse["choices"][0]["message"].contains("content") ? llmResponse["choices"][0]["message"]["content"].get<std::string>() : "";
            }
            if (llmResponse.contains("usage") && llmResponse["usage"].contains("completion_tokens")) {
                response.tokensUsed = llmResponse["usage"]["completion_tokens"].get<int>();
            }
        }

        std::cout << "[ModelInvoker] LLM response: " << (response.rawOutput.length() > 200 ? response.rawOutput.substr(0, 200) : response.rawOutput) << std::endl;

        // Parse into structured plan
        response.parsedPlan = parsePlan(response.rawOutput);

        // Validate sanity
        if (!validatePlanSanity(response.parsedPlan)) {
            response.error = "Plan failed sanity checks";
            response.success = false;
            m_isInvoking = false;
            if (m_onInvocationError) {
                m_onInvocationError(response.error, true);
            }
            return response;
        }

        response.success = true;
        response.reasoning = llmResponse.contains("reasoning") ? llmResponse["reasoning"].get<std::string>() : "";

        // Cache successful response
        if (m_cachingEnabled) {
            cacheResponse(getCacheKey(params), response);
        }

        std::cout << "[ModelInvoker] Generated plan with " << response.parsedPlan.size() << " actions" << std::endl;

    } catch (const std::exception& e) {
        response.error = std::string("Exception: ") + e.what();
        response.success = false;
        if (m_onInvocationError) {
            m_onInvocationError(response.error, false);
        }
    }

    m_isInvoking = false;
    return response;
}

/**
 * @brief Asynchronous invocation (non-blocking)
 */
void ModelInvoker::invokeAsync(const InvocationParams& params)
{
    std::async(std::launch::async, [this, params]() {
        LLMResponse response = invoke(params);
        if (m_onPlanGenerated) {
            m_onPlanGenerated(response.parsedPlan, response.reasoning);
        }
    });
}

/**
 * @brief Cancel pending request
 */
void ModelInvoker::cancelPendingRequest()
{
    m_isInvoking = false;
    qDebug() << "[ModelInvoker] Request cancelled";
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

    for (const std::string& tool : tools) {
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
    "params": { "pattern": "*.cpp", "query": "TODO" },
    "description": "Find all TODO comments in C++ files"
  },
  {
    "type": "file_edit",
    "target": "src/main.cpp",
    "params": { "action": "append", "content": "// new code" },
    "description": "Add new functionality"
  },
  {
    "type": "build",
    "target": "all",
    "params": { "config": "Release" },
    "description": "Build all targets"
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

    message += "Please generate a structured action plan to fulfill this wish. "
               "Respond with ONLY valid JSON array, no additional text.";

    return message;
}

/**
 * @brief Send request to Ollama API
 */
QJsonObject ModelInvoker::sendOllamaRequest(const QString& model,
                                            const QString& prompt,
                                            int maxTokens,
                                            double temperature)
{
    QUrl url(m_endpoint + "/api/generate");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload["model"] = model;
    payload["prompt"] = prompt;
    payload["temperature"] = temperature;
    payload["num_predict"] = maxTokens;
    payload["stream"] = false;

    QJsonDocument doc(payload);
    QByteArray data = doc.toJson();

    qDebug() << "[ModelInvoker] Sending request to Ollama:" << url.toString();

    // Synchronous request using event loop
    QEventLoop loop;
    QNetworkReply* reply = m_networkManager->post(request, data);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer::singleShot(30000, &loop, &QEventLoop::quit); // 30s timeout

    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[ModelInvoker] Network error:" << reply->errorString();
        reply->deleteLater();
        return QJsonObject();
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    return responseDoc.object();
}

/**
 * @brief Send request to Claude API
 */
QJsonObject ModelInvoker::sendClaudeRequest(const QString& prompt,
                                            int maxTokens,
                                            double temperature)
{
    QUrl url("https://api.anthropic.com/v1/messages");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("x-api-key", m_apiKey.toUtf8());
    request.setRawHeader("anthropic-version", "2023-06-01");

    QJsonObject payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;

    QJsonArray messages;
    QJsonObject message;
    message["role"] = "user";
    message["content"] = prompt;
    messages.append(message);
    payload["messages"] = messages;

    QJsonDocument doc(payload);
    QByteArray data = doc.toJson();

    QEventLoop loop;
    QNetworkReply* reply = m_networkManager->post(request, data);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(30000, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[ModelInvoker] Claude API error:" << reply->errorString();
        reply->deleteLater();
        return QJsonObject();
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    return responseDoc.object();
}

/**
 * @brief Send request to Ollama (local)
 */
nlohmann::json ModelInvoker::sendOllamaRequest(const std::string& model,
                                             const std::string& prompt,
                                             int maxTokens,
                                             double temperature)
{
    nlohmann::json payload;
    payload["model"] = model;
    payload["prompt"] = prompt;
    payload["stream"] = false;
    payload["options"]["num_predict"] = maxTokens;
    payload["options"]["temperature"] = temperature;

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";

    return sendHttpRequest("POST", m_endpoint + "/api/generate", headers, payload.dump(), 30000);
}

/**
 * @brief Send request to Claude (Anthropic)
 */
nlohmann::json ModelInvoker::sendClaudeRequest(const std::string& prompt,
                                             int maxTokens,
                                             double temperature)
{
    nlohmann::json payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;

    nlohmann::json message;
    message["role"] = "user";
    message["content"] = prompt;
    payload["messages"] = nlohmann::json::array({message});

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["x-api-key"] = m_apiKey;
    headers["anthropic-version"] = "2023-06-01";

    return sendHttpRequest("POST", m_endpoint + "/v1/messages", headers, payload.dump(), 60000);
}

/**
 * @brief Send request to OpenAI API
 */
nlohmann::json ModelInvoker::sendOpenAIRequest(const std::string& prompt,
                                            int maxTokens,
                                            double temperature)
{
    nlohmann::json payload;
    payload["model"] = m_model;
    payload["max_tokens"] = maxTokens;
    payload["temperature"] = temperature;

    nlohmann::json message;
    message["role"] = "user";
    message["content"] = prompt;
    payload["messages"] = nlohmann::json::array({message});

    std::map<std::string, std::string> headers;
    headers["Content-Type"] = "application/json";
    headers["Authorization"] = "Bearer " + m_apiKey;

    std::string url = (m_endpoint.find("/v1") != std::string::npos) ? m_endpoint + "/chat/completions" : m_endpoint + "/v1/chat/completions";
    return sendHttpRequest("POST", url, headers, payload.dump(), 60000);
}

/**
 * @brief Internal helper to send HTTP request via WinHTTP
 */
nlohmann::json ModelInvoker::sendHttpRequest(const std::string& method,
                                           const std::string& url,
                                           const std::map<std::string, std::string>& headers,
                                           const std::string& payload,
                                           int timeoutMs)
{
    nlohmann::json result;
    
    // Parse URL
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    std::wstring wUrl(url.begin(), url.end());
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &urlComp)) {
        return result;
    }

    std::wstring wHostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring wUrlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(L"RawrXD-Agent/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return result;

    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, wHostName.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return result;
    }

    std::wstring wMethod(method.begin(), method.end());
    DWORD dwFlags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(), wUrlPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, dwFlags);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return result;
    }

    // Add headers
    for (const auto& [name, value] : headers) {
        std::wstring wHeader = std::wstring(name.begin(), name.end()) + L": " + std::wstring(value.begin(), value.end());
        WinHttpAddRequestHeaders(hRequest, wHeader.c_str(), (ULONG)-1L, WINHTTP_ADDREQ_FLAG_ADD);
    }

    // Send request
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)payload.c_str(), (DWORD)payload.length(), (DWORD)payload.length(), 0)) {
        if (WinHttpReceiveResponse(hRequest, NULL)) {
            DWORD dwSize = 0;
            std::string responseData;
            do {
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (dwSize == 0) break;

                std::vector<char> buffer(dwSize);
                DWORD dwRead = 0;
                if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwRead)) {
                    responseData.append(buffer.data(), dwRead);
                }
            } while (dwSize > 0);

            try {
                result = nlohmann::json::parse(responseData);
            } catch (...) {
                result["_raw_response"] = responseData;
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return result;
}

/**
 * @brief Parse LLM response into structured plan
 */
nlohmann::json ModelInvoker::parsePlan(const std::string& llmOutput)
{
    // Strategy 1: Direct JSON extraction (```json ... ```)
    std::regex jsonBlockRegex(R"(```(?:json)?\s*\n?([\s\S]*?)\n?```)");
    std::smatch match;

    if (std::regex_search(llmOutput, match, jsonBlockRegex)) {
        if (match.size() > 1) {
            std::string jsonStr = match.str(1);
            try {
                auto doc = nlohmann::json::parse(jsonStr);
                if (doc.is_array()) {
                    return doc;
                }
            } catch (...) {}
        }
    }

    // Strategy 2: Try parsing entire output as JSON
    try {
        auto doc = nlohmann::json::parse(llmOutput);
        if (doc.is_array()) {
            return doc;
        }
    } catch (...) {}

    // Strategy 3: Fallback - create generic action
    std::cerr << "[ModelInvoker] Failed to parse plan from LLM output" << std::endl;
    nlohmann::json fallback = nlohmann::json::array();
    nlohmann::json action;
    action["type"] = "user_input";
    action["description"] = llmOutput.length() > 500 ? llmOutput.substr(0, 500) : llmOutput;
    fallback.push_back(action);
    return fallback;
}

/**
 * @brief Validate plan sanity
 */
bool ModelInvoker::validatePlanSanity(const nlohmann::json& plan)
{
    if (plan.empty() || !plan.is_array()) {
        std::cerr << "[ModelInvoker] Empty or invalid plan detected" << std::endl;
        return false;
    }

    int actionCount = 0;
    std::vector<std::string> seenTargets;

    for (const auto& val : plan) {
        if (!val.is_object()) {
            std::cerr << "[ModelInvoker] Non-object in plan" << std::endl;
            return false;
        }

        std::string type = val.contains("type") ? val["type"].get<std::string>() : "";

        // Check for dangerous operations
        if (type == "file_delete" || type == "format_drive" || type == "system_reboot") {
            std::cerr << "[ModelInvoker] Dangerous operation detected: " << type << std::endl;
            return false;
        }

        // Check for circular dependencies
        std::string target = val.contains("target") ? val["target"].get<std::string>() : "";
        if (!target.empty() && std::find(seenTargets.begin(), seenTargets.end(), target) != seenTargets.end()) {
            std::cerr << "[ModelInvoker] Circular dependency on target: " << target << std::endl;
            return false;
        }
        seenTargets.push_back(target);

        actionCount++;
        if (actionCount > 100) {
            std::cerr << "[ModelInvoker] Plan too large (>100 actions)" << std::endl;
            return false;
        }
    }

    return true;
}

/**
 * @brief Get cache key for request
 */
std::string ModelInvoker::getCacheKey(const InvocationParams& params) const
{
    return params.wish.length() > 100 ? params.wish.substr(0, 100) : params.wish;
}

/**
 * @brief Load cached response
 */
LLMResponse ModelInvoker::getCachedResponse(const std::string& key) const
{
    auto it = m_responseCache.find(key);
    if (it != m_responseCache.end()) {
        return it->second;
    }
    return LLMResponse();
}

/**
 * @brief Store response in cache
 */
void ModelInvoker::cacheResponse(const std::string& key, const LLMResponse& response)
{
    const_cast<ModelInvoker*>(this)->m_responseCache[key] = response;
}

// ============================================================================
// Vision/Multimodal Support
// ============================================================================

/**
 * @brief Analyze an image using vision-capable model
 */
LLMResponse ModelInvoker::analyzeImage(const std::vector<uint8_t>& imageData,
                                       const std::string& mediaType,
                                       const std::string& prompt)
{
    LLMResponse response;
    
    if (imageData.empty()) {
        response.error = "No image data provided";
        return response;
    }

    std::cout << "[ModelInvoker] Analyzing image (" << imageData.size() << " bytes)" << std::endl;
    
    // Simple Base64 encoding
    static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/
";
    std::string base64Data;
    int i = 0, j = 0;
    unsigned char char_array_3[3], char_array_4[4];

    for (const auto& byte : imageData) {
        char_array_3[i++] = byte;
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
            for (i = 0; i < 4; i++) base64Data += base64_chars[char_array_4[i]];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;
        for (j = 0; j < i + 1; j++) base64Data += base64_chars[char_array_4[j]];
        while (i++ < 3) base64Data += '=';
    }
    
    if (m_backend == "claude") {
        nlohmann::json payload;
        payload["model"] = m_model.empty() ? "claude-3-sonnet-20240229" : m_model;
        payload["max_tokens"] = 4096;
        
        nlohmann::json imageBlock;
        imageBlock["type"] = "image";
        imageBlock["source"]["type"] = "base64";
        imageBlock["source"]["media_type"] = mediaType.empty() ? "image/png" : mediaType;
        imageBlock["source"]["data"] = base64Data;
        
        nlohmann::json textBlock;
        textBlock["type"] = "text";
        textBlock["text"] = prompt.empty() ? "Describe this image in detail." : prompt;
        
        nlohmann::json messages;
        nlohmann::json message;
        message["role"] = "user";
        message["content"] = nlohmann::json::array({imageBlock, textBlock});
        messages.push_back(message);
        payload["messages"] = messages;
        
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        headers["x-api-key"] = m_apiKey;
        headers["anthropic-version"] = "2023-06-01";

        auto resp = sendHttpRequest("POST", m_endpoint + "/v1/messages", headers, payload.dump(), 60000);
        if (resp.contains("content") && resp["content"].is_array() && !resp["content"].empty()) {
            response.rawOutput = resp["content"][0]["text"].get<std::string>();
            response.success = true;
        }
    } else if (m_backend == "openai") {
        nlohmann::json payload;
        payload["model"] = m_model.empty() ? "gpt-4-vision-preview" : m_model;
        payload["max_tokens"] = 4096;
        
        nlohmann::json textContent;
        textContent["type"] = "text";
        textContent["text"] = prompt.empty() ? "Describe this image in detail." : prompt;
        
        nlohmann::json imageContent;
        imageContent["type"] = "image_url";
        std::string mimeType = mediaType.empty() ? "image/png" : mediaType;
        imageContent["image_url"]["url"] = "data:" + mimeType + ";base64," + base64Data;
        
        nlohmann::json messages;
        nlohmann::json message;
        message["role"] = "user";
        message["content"] = nlohmann::json::array({textContent, imageContent});
        messages.push_back(message);
        payload["messages"] = messages;
        
        std::map<std::string, std::string> headers;
        headers["Content-Type"] = "application/json";
        headers["Authorization"] = "Bearer " + m_apiKey;

        auto resp = sendHttpRequest("POST", m_endpoint + "/v1/chat/completions", headers, payload.dump(), 60000);
        if (resp.contains("choices") && resp["choices"].is_array() && !resp["choices"].empty()) {
            response.rawOutput = resp["choices"][0]["message"]["content"].get<std::string>();
            response.success = true;
        }
    } else {
        response.error = "Vision not supported for backend: " + m_backend;
    }
    
    return response;
}

/**
 * @brief Capture and analyze screenshot
 */
LLMResponse ModelInvoker::analyzeScreenshot(const std::string& prompt)
{
    LLMResponse response;
    
    // Capture primary screen on Windows
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    
    response.error = "Screenshot analysis requires a PNG encoder (not implemented)";
    return response;
}