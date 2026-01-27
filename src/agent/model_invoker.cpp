/**
 * @file model_invoker.cpp
 * @brief Implementation of LLM invocation layer
 *
 * Provides synchronous and asynchronous wish→plan transformation
 * with support for Ollama (local) and cloud LLMs.
 */

#include "model_invoker.hpp"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QTimer>
#include <QEventLoop>
#include <QDebug>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QtConcurrent>

/**
 * @brief Constructor - initializes network manager and default settings
 */
ModelInvoker::ModelInvoker(QObject* parent)
    : QObject(parent)
    , m_networkManager(std::make_unique<QNetworkAccessManager>(this))
{
    m_backend = "ollama";
    m_endpoint = "http://localhost:11434";

    // Load cached responses from disk if available
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (!cacheDir.isEmpty()) {
        QDir dir(cacheDir);
        if (!dir.exists("agent_cache")) {
            dir.mkdir("agent_cache");
        }
    }
}

/**
 * @brief Destructor
 */
ModelInvoker::~ModelInvoker() = default;

/**
 * @brief Set the LLM backend and endpoint
 */
void ModelInvoker::setLLMBackend(const QString& backend,
                                  const QString& endpoint,
                                  const QString& apiKey)
{
    m_backend = backend.toLower();
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

    qInfo() << "[ModelInvoker] Backend set to" << m_backend << "at" << m_endpoint;
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
        QString cacheKey = getCacheKey(params);
        LLMResponse cached = getCachedResponse(cacheKey);
        if (cached.success) {
            qDebug() << "[ModelInvoker] Cache hit for:" << params.wish;
            return cached;
        }
    }

    qDebug() << "[ModelInvoker] Invoking LLM with wish:" << params.wish;
    m_isInvoking = true;
    emit planGenerationStarted(params.wish);

    LLMResponse response;

    try {
        // Build prompts
        QString systemPrompt = m_customSystemPrompt.isEmpty()
                                   ? buildSystemPrompt(params.availableTools)
                                   : m_customSystemPrompt;
        QString userMessage = buildUserMessage(params);

        QJsonObject llmResponse;

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
        if (llmResponse.isEmpty()) {
            response.error = "Empty response from LLM";
            m_isInvoking = false;
            emit invocationError(response.error, true);
            return response;
        }

        // Parse backend-specific response format
        if (m_backend == "ollama") {
            response.rawOutput = llmResponse.value("response").toString();
            response.tokensUsed = llmResponse.value("eval_count").toInt() + 
                                  llmResponse.value("prompt_eval_count").toInt();
        } else if (m_backend == "claude") {
            auto content = llmResponse.value("content").toArray();
            if (!content.isEmpty()) {
                response.rawOutput = content[0].toObject().value("text").toString();
            }
            response.tokensUsed = llmResponse.value("usage").toObject().value("output_tokens").toInt();
        } else if (m_backend == "openai") {
            auto choices = llmResponse.value("choices").toArray();
            if (!choices.isEmpty()) {
                response.rawOutput = choices[0].toObject().value("message").toObject().value("content").toString();
            }
            response.tokensUsed = llmResponse.value("usage").toObject().value("completion_tokens").toInt();
        }

        qDebug() << "[ModelInvoker] LLM response:" << response.rawOutput.left(200);

        // Parse into structured plan
        response.parsedPlan = parsePlan(response.rawOutput);

        // Validate sanity
        if (!validatePlanSanity(response.parsedPlan)) {
            response.error = "Plan failed sanity checks";
            response.success = false;
            m_isInvoking = false;
            emit invocationError(response.error, true);
            return response;
        }

        response.success = true;
        response.reasoning = llmResponse.value("reasoning").toString();

        // Cache successful response
        if (m_cachingEnabled) {
            cacheResponse(getCacheKey(params), response);
        }

        qInfo() << "[ModelInvoker] Generated plan with" << response.parsedPlan.size() << "actions";

    } catch (const std::exception& e) {
        response.error = "Exception: " + QString::fromStdString(e.what());
        response.success = false;
        emit invocationError(response.error, false);
    }

    m_isInvoking = false;
    return response;
}

/**
 * @brief Asynchronous invocation (non-blocking)
 */
void ModelInvoker::invokeAsync(const InvocationParams& params)
{
    QtConcurrent::run([this, params]() {
        LLMResponse response = invoke(params);
        emit planGenerated(response);
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
QString ModelInvoker::buildSystemPrompt(const QStringList& tools)
{
    QString prompt = R"(You are an intelligent IDE agent for the RawrXD code generation framework.

Your role is to transform natural language wishes into structured action plans that can be executed by an automated system.

# Available Tools
You can use the following tools:
)";

    for (const QString& tool : tools) {
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
QString ModelInvoker::buildUserMessage(const InvocationParams& params)
{
    QString message = "User Wish: " + params.wish + "\n\n";

    if (!params.context.isEmpty()) {
        message += "Context: " + params.context + "\n\n";
    }

    if (!params.codebaseContext.isEmpty()) {
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
 * @brief Send request to OpenAI API
 */
QJsonObject ModelInvoker::sendOpenAIRequest(const QString& prompt,
                                            int maxTokens,
                                            double temperature)
{
    QUrl url("https://api.openai.com/v1/chat/completions");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

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
        qWarning() << "[ModelInvoker] OpenAI API error:" << reply->errorString();
        reply->deleteLater();
        return QJsonObject();
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    return responseDoc.object();
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
