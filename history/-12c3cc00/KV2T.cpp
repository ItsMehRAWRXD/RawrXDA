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
QJsonArray ModelInvoker::parsePlan(const QString& llmOutput)
{
    // Strategy 1: Direct JSON extraction (```json ... ```)
    QRegularExpression jsonBlockRegex(R"(```(?:json)?\s*\n?([\s\S]*?)\n?```)", 
                                       QRegularExpression::MultilineOption);
    QRegularExpressionMatch match = jsonBlockRegex.match(llmOutput);

    if (match.hasMatch()) {
        QString jsonStr = match.captured(1);
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isArray()) {
            return doc.array();
        }
    }

    // Strategy 2: Try parsing entire output as JSON
    QJsonDocument doc = QJsonDocument::fromJson(llmOutput.toUtf8());
    if (doc.isArray()) {
        return doc.array();
    }

    // Strategy 3: Fallback - create generic action
    qWarning() << "[ModelInvoker] Failed to parse plan from LLM output";
    QJsonArray fallback;
    QJsonObject action;
    action["type"] = "user_input";
    action["description"] = llmOutput.left(500);
    fallback.append(action);
    return fallback;
}

/**
 * @brief Validate plan sanity
 */
bool ModelInvoker::validatePlanSanity(const QJsonArray& plan)
{
    if (plan.isEmpty()) {
        qWarning() << "[ModelInvoker] Empty plan detected";
        return false;
    }

    int actionCount = 0;
    QStringList seenTargets;

    for (const QJsonValue& val : plan) {
        if (!val.isObject()) {
            qWarning() << "[ModelInvoker] Non-object in plan";
            return false;
        }

        QJsonObject action = val.toObject();
        QString type = action.value("type").toString();

        // Check for dangerous operations
        if (type == "file_delete" || type == "format_drive" || type == "system_reboot") {
            qWarning() << "[ModelInvoker] Dangerous operation detected:" << type;
            return false;
        }

        // Check for circular dependencies
        QString target = action.value("target").toString();
        if (seenTargets.contains(target)) {
            qWarning() << "[ModelInvoker] Circular dependency on target:" << target;
            return false;
        }
        seenTargets.append(target);

        actionCount++;
        if (actionCount > 100) {
            qWarning() << "[ModelInvoker] Plan too large (>100 actions)";
            return false;
        }
    }

    return true;
}

/**
 * @brief Get cache key for request
 */
QString ModelInvoker::getCacheKey(const InvocationParams& params) const
{
    return params.wish.mid(0, 100);
}

/**
 * @brief Load cached response
 */
LLMResponse ModelInvoker::getCachedResponse(const QString& key) const
{
    if (m_responseCache.contains(key)) {
        return m_responseCache[key];
    }
    return LLMResponse();
}

/**
 * @brief Store response in cache
 */
void ModelInvoker::cacheResponse(const QString& key, const LLMResponse& response)
{
    m_responseCache[key] = response;
}

/**
 * @brief Handle LLM response received from network
 */
void ModelInvoker::onLLMResponseReceived(const QByteArray& data)
{
    qDebug() << "[ModelInvoker] Received LLM response:" << data.size() << "bytes";
    
    // Parse JSON response
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "[ModelInvoker] JSON parse error:" << parseError.errorString();
        emit invocationError("Failed to parse LLM response: " + parseError.errorString(), true);
        return;
    }
    
    // Extract plan from response
    LLMResponse response;
    response.success = true;
    response.rawOutput = QString::fromUtf8(data);
    
    // Try to extract JSON plan from response
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("plan") && obj["plan"].isArray()) {
            response.parsedPlan = obj["plan"].toArray();
        }
        if (obj.contains("reasoning")) {
            response.reasoning = obj["reasoning"].toString();
        }
    }
    
    emit planGenerated(response);
}

/**
 * @brief Handle network error
 */
void ModelInvoker::onNetworkError(const QString& error)
{
    qWarning() << "[ModelInvoker] Network error:" << error;
    emit invocationError("Network error: " + error, true);
}

/**
 * @brief Handle request timeout
 */
void ModelInvoker::onRequestTimeout()
{
    qWarning() << "[ModelInvoker] Request timeout";
    emit invocationError("Request timed out - LLM did not respond", true);
}

// ============================================================================
// Vision/Multimodal Support
// ============================================================================

/**
 * @brief Analyze an image using vision-capable model
 */
LLMResponse ModelInvoker::analyzeImage(const QByteArray& imageData,
                                       const QString& mediaType,
                                       const QString& prompt)
{
    LLMResponse response;
    
    if (imageData.isEmpty()) {
        response.error = "No image data provided";
        return response;
    }

    qInfo() << "[ModelInvoker] Analyzing image (" << imageData.size() << " bytes)";
    
    // Convert to base64 if not already
    QString base64Data = QString::fromLatin1(imageData.toBase64());
    
    if (m_backend == "claude") {
        // Claude Vision API (claude-3-sonnet, claude-3-opus)
        QUrl url("https://api.anthropic.com/v1/messages");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("x-api-key", m_apiKey.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
        
        QJsonObject payload;
        payload["model"] = m_model.isEmpty() ? "claude-3-sonnet-20240229" : m_model;
        payload["max_tokens"] = 4096;
        
        // Build multimodal content
        QJsonArray contentArray;
        
        // Image content block
        QJsonObject imageBlock;
        imageBlock["type"] = "image";
        QJsonObject source;
        source["type"] = "base64";
        source["media_type"] = mediaType.isEmpty() ? "image/png" : mediaType;
        source["data"] = base64Data;
        imageBlock["source"] = source;
        contentArray.append(imageBlock);
        
        // Text prompt block
        QJsonObject textBlock;
        textBlock["type"] = "text";
        textBlock["text"] = prompt.isEmpty() ? "Describe this image in detail." : prompt;
        contentArray.append(textBlock);
        
        QJsonArray messages;
        QJsonObject message;
        message["role"] = "user";
        message["content"] = contentArray;
        messages.append(message);
        payload["messages"] = messages;
        
        QJsonDocument doc(payload);
        QByteArray data = doc.toJson();
        
        QEventLoop loop;
        QNetworkReply* reply = m_networkManager->post(request, data);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(60000, &loop, &QEventLoop::quit); // 60s for vision
        loop.exec();
        
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument respDoc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject respObj = respDoc.object();
            auto content = respObj.value("content").toArray();
            if (!content.isEmpty()) {
                response.rawOutput = content[0].toObject().value("text").toString();
                response.success = true;
            }
        } else {
            response.error = reply->errorString();
        }
        reply->deleteLater();
        
    } else if (m_backend == "openai") {
        // OpenAI GPT-4 Vision API
        QUrl url("https://api.openai.com/v1/chat/completions");
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
        
        QJsonObject payload;
        payload["model"] = m_model.isEmpty() ? "gpt-4-vision-preview" : m_model;
        payload["max_tokens"] = 4096;
        
        // Build multimodal content
        QJsonArray contentArray;
        
        // Text content
        QJsonObject textContent;
        textContent["type"] = "text";
        textContent["text"] = prompt.isEmpty() ? "Describe this image in detail." : prompt;
        contentArray.append(textContent);
        
        // Image URL (data URL format)
        QJsonObject imageContent;
        imageContent["type"] = "image_url";
        QJsonObject imageUrl;
        QString mimeType = mediaType.isEmpty() ? "image/png" : mediaType;
        imageUrl["url"] = QString("data:%1;base64,%2").arg(mimeType, base64Data);
        imageContent["image_url"] = imageUrl;
        contentArray.append(imageContent);
        
        QJsonArray messages;
        QJsonObject message;
        message["role"] = "user";
        message["content"] = contentArray;
        messages.append(message);
        payload["messages"] = messages;
        
        QJsonDocument doc(payload);
        QByteArray data = doc.toJson();
        
        QEventLoop loop;
        QNetworkReply* reply = m_networkManager->post(request, data);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(60000, &loop, &QEventLoop::quit);
        loop.exec();
        
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument respDoc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject respObj = respDoc.object();
            auto choices = respObj.value("choices").toArray();
            if (!choices.isEmpty()) {
                response.rawOutput = choices[0].toObject()
                    .value("message").toObject()
                    .value("content").toString();
                response.success = true;
            }
        } else {
            response.error = reply->errorString();
        }
        reply->deleteLater();
        
    } else {
        response.error = "Vision not supported for backend: " + m_backend + 
                        ". Use 'claude' or 'openai'.";
    }
    
    return response;
}

/**
 * @brief Capture and analyze screenshot
 */
LLMResponse ModelInvoker::analyzeScreenshot(const QString& prompt)
{
    LLMResponse response;
    
#ifdef Q_OS_WIN
    // Capture primary screen on Windows
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);
    
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, width, height, hdcScreen, 0, 0, SRCCOPY);
    
    // Convert to QImage
    QImage screenshot(width, height, QImage::Format_ARGB32);
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative for top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    GetDIBits(hdcMem, hBitmap, 0, height, screenshot.bits(), 
              (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    
    // Convert to PNG bytes
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    screenshot.save(&buffer, "PNG");
    
    qInfo() << "[ModelInvoker] Screenshot captured:" << width << "x" << height;
    
    return analyzeImage(imageData, "image/png", prompt);
#else
    response.error = "Screenshot capture not implemented for this platform";
    return response;
#endif
}