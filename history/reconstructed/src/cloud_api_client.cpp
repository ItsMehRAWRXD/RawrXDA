// cloud_api_client.cpp - Implementation of Cloud API Client
#include "cloud_api_client.h"
#include "universal_model_router.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>

CloudApiClient::CloudApiClient(QObject* parent)
    : QObject(parent),
      network_manager(std::make_unique<QNetworkAccessManager>(this))
{
    initializeApiEndpoints();
    
    connect(network_manager.get(), &QNetworkAccessManager::finished,
            this, &CloudApiClient::onNetworkReplyFinished);
}

CloudApiClient::~CloudApiClient() = default;

void CloudApiClient::initializeApiEndpoints()
{
    // ANTHROPIC API
    api_endpoints[static_cast<int>(ModelBackend::ANTHROPIC)] = {
        "https://api.anthropic.com",
        "/v1/messages",
        "/v1/models",
        [this](const QString& prompt, const ModelConfig& config) {
            return buildAnthropicRequest(prompt, config);
        },
        [this](const QJsonObject& response) {
            return parseAnthropicResponse(response);
        }
    };
    
    // OPENAI API
    api_endpoints[static_cast<int>(ModelBackend::OPENAI)] = {
        "https://api.openai.com",
        "/v1/chat/completions",
        "/v1/models",
        [this](const QString& prompt, const ModelConfig& config) {
            return buildOpenAIRequest(prompt, config);
        },
        [this](const QJsonObject& response) {
            return parseOpenAIResponse(response);
        }
    };
    
    // GOOGLE GEMINI API
    api_endpoints[static_cast<int>(ModelBackend::GOOGLE)] = {
        "https://generativelanguage.googleapis.com",
        "/v1beta/models/{model}:generateContent",
        "/v1beta/models",
        [this](const QString& prompt, const ModelConfig& config) {
            return buildGoogleRequest(prompt, config);
        },
        [this](const QJsonObject& response) {
            return parseGoogleResponse(response);
        }
    };
    
    // MOONSHOT (KIMI) API
    api_endpoints[static_cast<int>(ModelBackend::MOONSHOT)] = {
        "https://api.moonshot.cn",
        "/v1/chat/completions",
        "/v1/models",
        [this](const QString& prompt, const ModelConfig& config) {
            return buildMoonshotRequest(prompt, config);
        },
        [this](const QJsonObject& response) {
            return parseMoonshotResponse(response);
        }
    };
    
    // AZURE OPENAI API
    api_endpoints[static_cast<int>(ModelBackend::AZURE_OPENAI)] = {
        "", // Azure uses custom endpoint
        "/chat/completions",
        "/models",
        [this](const QString& prompt, const ModelConfig& config) {
            return buildAzureOpenAIRequest(prompt, config);
        },
        [this](const QJsonObject& response) {
            return parseAzureOpenAIResponse(response);
        }
    };
    
    // AWS BEDROCK API
    api_endpoints[static_cast<int>(ModelBackend::AWS_BEDROCK)] = {
        "", // Bedrock uses regional endpoints
        "/model/invoke",
        "/foundation-models",
        [this](const QString& prompt, const ModelConfig& config) {
            return buildAwsBedrockRequest(prompt, config);
        },
        [this](const QJsonObject& response) {
            return parseAwsBedrockResponse(response);
        }
    };
}

QString CloudApiClient::generate(const QString& prompt, const ModelConfig& config)
{
    if (!config.isValid()) {
        emit generationFailed("Invalid model configuration");
        return "";
    }
    
    ApiResponse response = executeRequest(
        api_endpoints[static_cast<int>(config.backend)].chat_endpoint,
        "POST",
        buildRequestBody(prompt, config),
        config.api_key
    );
    
    if (!response.success) {
        emit generationFailed(response.error_message);
        return "";
    }
    
    emit generationCompleted(response);
    return response.content;
}

void CloudApiClient::generateAsync(const QString& prompt,
                                   const ModelConfig& config,
                                   std::function<void(const ApiResponse&)> callback)
{
    // Implementation for async generation
    // In a real implementation, this would queue requests and use signals/slots
    auto result = generate(prompt, config);
    ApiResponse response;
    response.content = result;
    response.success = !result.isEmpty();
    callback(response);
}

void CloudApiClient::generateStream(const QString& prompt,
                                   const ModelConfig& config,
                                   std::function<void(const QString&)> chunk_callback,
                                   std::function<void(const QString&)> error_callback)
{
    // For now, implement as non-streaming (collect all then emit chunks)
    // In production, use SSE (Server-Sent Events) support
    QString result = generate(prompt, config);
    
    if (!result.isEmpty()) {
        // Split result into chunks (simulate streaming)
        int chunk_size = 50;
        for (int i = 0; i < result.length(); i += chunk_size) {
            chunk_callback(result.mid(i, chunk_size));
        }
        emit streamingCompleted();
    } else {
        if (error_callback) {
            error_callback("Generation failed");
        }
        emit streamingFailed("Generation failed");
    }
}

bool CloudApiClient::checkProviderHealth(const ModelConfig& config)
{
    // Simple health check by attempting to list models
    QStringList models = listModels(config);
    bool healthy = !models.isEmpty();
    
    ApiCallLog log;
    log.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    log.provider = QString::number(static_cast<int>(config.backend));
    log.endpoint = api_endpoints[static_cast<int>(config.backend)].model_list_endpoint;
    log.success = healthy;
    logApiCall(log);
    
    emit healthCheckCompleted(healthy);
    return healthy;
}

void CloudApiClient::checkProviderHealthAsync(const ModelConfig& config,
                                              std::function<void(bool)> callback)
{
    bool result = checkProviderHealth(config);
    callback(result);
}

QStringList CloudApiClient::listModels(const ModelConfig& config)
{
    // This would need implementation for each provider
    // For now, return empty list
    return QStringList();
}

void CloudApiClient::listModelsAsync(const ModelConfig& config,
                                     std::function<void(const QStringList&)> callback)
{
    auto models = listModels(config);
    callback(models);
}

QJsonObject CloudApiClient::buildRequestBody(const QString& prompt, const ModelConfig& config)
{
    return api_endpoints[static_cast<int>(config.backend)].request_builder(prompt, config);
}

// ============ REQUEST BUILDERS ============

QJsonObject CloudApiClient::buildAnthropicRequest(const QString& prompt, const ModelConfig& config)
{
    QJsonObject request;
    request["model"] = config.model_id;
    request["max_tokens"] = config.parameters.value("max_tokens", "4096").toInt();
    
    if (config.parameters.contains("temperature")) {
        request["temperature"] = config.parameters.value("temperature").toDouble();
    }
    
    QJsonArray messages;
    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);
    
    request["messages"] = messages;
    
    return request;
}

QJsonObject CloudApiClient::buildOpenAIRequest(const QString& prompt, const ModelConfig& config)
{
    QJsonObject request;
    request["model"] = config.model_id;
    
    if (config.parameters.contains("max_tokens")) {
        request["max_tokens"] = config.parameters.value("max_tokens").toInt();
    }
    
    if (config.parameters.contains("temperature")) {
        request["temperature"] = config.parameters.value("temperature").toDouble();
    }
    
    QJsonArray messages;
    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);
    
    request["messages"] = messages;
    
    return request;
}

QJsonObject CloudApiClient::buildGoogleRequest(const QString& prompt, const ModelConfig& config)
{
    QJsonObject request;
    
    QJsonArray contents;
    QJsonObject content;
    content["role"] = "user";
    
    QJsonArray parts;
    QJsonObject part;
    part["text"] = prompt;
    parts.append(part);
    
    content["parts"] = parts;
    contents.append(content);
    
    request["contents"] = contents;
    
    return request;
}

QJsonObject CloudApiClient::buildMoonshotRequest(const QString& prompt, const ModelConfig& config)
{
    QJsonObject request;
    request["model"] = config.model_id;
    
    if (config.parameters.contains("max_tokens")) {
        request["max_tokens"] = config.parameters.value("max_tokens").toInt();
    }
    
    QJsonArray messages;
    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);
    
    request["messages"] = messages;
    
    return request;
}

QJsonObject CloudApiClient::buildAzureOpenAIRequest(const QString& prompt, const ModelConfig& config)
{
    // Azure uses OpenAI format but different endpoint
    return buildOpenAIRequest(prompt, config);
}

QJsonObject CloudApiClient::buildAwsBedrockRequest(const QString& prompt, const ModelConfig& config)
{
    QJsonObject request;
    
    // Bedrock uses provider-specific formats
    // This is a generic wrapper
    QJsonObject messages;
    messages["prompt"] = prompt;
    
    if (config.parameters.contains("max_tokens")) {
        messages["max_tokens_to_sample"] = config.parameters.value("max_tokens").toInt();
    }
    
    request["body"] = messages;
    
    return request;
}

// ============ RESPONSE PARSERS ============

QString CloudApiClient::parseAnthropicResponse(const QJsonObject& response)
{
    if (!response.contains("content") || response["content"].toArray().isEmpty()) {
        return "";
    }
    
    QJsonArray content = response["content"].toArray();
    if (content[0].toObject().contains("text")) {
        return content[0].toObject()["text"].toString();
    }
    
    return "";
}

QString CloudApiClient::parseOpenAIResponse(const QJsonObject& response)
{
    if (!response.contains("choices") || response["choices"].toArray().isEmpty()) {
        return "";
    }
    
    QJsonArray choices = response["choices"].toArray();
    QJsonObject choice = choices[0].toObject();
    
    if (choice.contains("message")) {
        return choice["message"].toObject()["content"].toString();
    }
    
    return "";
}

QString CloudApiClient::parseGoogleResponse(const QJsonObject& response)
{
    if (!response.contains("candidates") || response["candidates"].toArray().isEmpty()) {
        return "";
    }
    
    QJsonArray candidates = response["candidates"].toArray();
    QJsonObject candidate = candidates[0].toObject();
    
    if (candidate.contains("content") && candidate["content"].toObject().contains("parts")) {
        QJsonArray parts = candidate["content"].toObject()["parts"].toArray();
        if (!parts.isEmpty()) {
            return parts[0].toObject()["text"].toString();
        }
    }
    
    return "";
}

QString CloudApiClient::parseMoonshotResponse(const QJsonObject& response)
{
    // Moonshot uses same format as OpenAI
    return parseOpenAIResponse(response);
}

QString CloudApiClient::parseAzureOpenAIResponse(const QJsonObject& response)
{
    // Azure uses same format as OpenAI
    return parseOpenAIResponse(response);
}

QString CloudApiClient::parseAwsBedrockResponse(const QJsonObject& response)
{
    if (response.contains("body")) {
        QString body = response["body"].toString();
        // Parse Bedrock-specific response format
        return body;
    }
    
    return "";
}

// ============ UTILITY METHODS ============

ApiResponse CloudApiClient::executeRequest(const QString& endpoint,
                                          const QString& method,
                                          const QJsonObject& body,
                                          const QString& api_key,
                                          const QMap<QString, QString>& headers)
{
    ApiResponse response;
    response.success = false;
    response.status_code = 0;

    QNetworkRequest request(QUrl(endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    if (!api_key.isEmpty()) {
        request.setRawHeader("Authorization", ("Bearer " + api_key).toUtf8());
    }
    
    // Apply custom headers
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }
    
    // Set timeouts
    request.setTransferTimeout(30000); // 30 second timeout
    
    QJsonDocument doc(body);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    
    // Execute synchronous request using QEventLoop
    QNetworkReply* reply = nullptr;
    if (method.toUpper() == "POST") {
        reply = network_manager->post(request, jsonData);
    } else if (method.toUpper() == "GET") {
        reply = network_manager->get(request);
    } else if (method.toUpper() == "PUT") {
        reply = network_manager->put(request, jsonData);
    } else if (method.toUpper() == "DELETE") {
        reply = network_manager->deleteResource(request);
    } else {
        response.error_message = "Unsupported HTTP method: " + method;
        return response;
    }
    
    // Block until response arrives
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    
    // Timeout guard
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(30000);
    
    loop.exec();
    
    if (!reply->isFinished()) {
        reply->abort();
        response.error_message = "Request timed out after 30 seconds";
        response.status_code = 408;
        reply->deleteLater();
        return response;
    }
    
    response.status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QByteArray responseData = reply->readAll();
    
    if (reply->error() == QNetworkReply::NoError) {
        response.success = true;
        
        // Parse JSON response
        QJsonParseError parseError;
        QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            response.body = responseDoc.object();
        } else {
            response.raw_body = QString::fromUtf8(responseData);
        }
    } else {
        response.success = false;
        response.error_message = formatErrorResponse(response.status_code, QString::fromUtf8(responseData));
    }
    
    // Log the API call
    ApiCallLog log;
    log.timestamp = QDateTime::currentDateTime();
    log.endpoint = endpoint;
    log.method = method;
    log.status_code = response.status_code;
    log.success = response.success;
    log.latency_ms = reply->property("downloadTime").toInt();
    logApiCall(log);
    
    reply->deleteLater();
    return response;
}

QString CloudApiClient::formatErrorResponse(int status_code, const QString& body)
{
    return QString("HTTP %1: %2").arg(status_code).arg(body);
}

void CloudApiClient::logApiCall(const ApiCallLog& log)
{
    call_history.append(log);
    
    // Keep history size bounded
    if (call_history.size() > MAX_HISTORY_SIZE) {
        call_history.removeFirst();
    }
}

void CloudApiClient::onNetworkReplyFinished(QNetworkReply* reply)
{
    // Handle completed network requests
    if (reply->error() != QNetworkReply::NoError) {
        emit generationFailed(reply->errorString());
    }
    
    reply->deleteLater();
}

QVector<ApiCallLog> CloudApiClient::getCallHistory() const
{
    return call_history;
}

void CloudApiClient::clearCallHistory()
{
    call_history.clear();
}

ApiCallLog CloudApiClient::getLastCall() const
{
    if (call_history.isEmpty()) {
        return ApiCallLog();
    }
    return call_history.last();
}

double CloudApiClient::getAverageLatency() const
{
    if (call_history.isEmpty()) {
        return 0.0;
    }
    
    double total = 0.0;
    for (const auto& log : call_history) {
        total += log.latency_ms;
    }
    
    return total / call_history.size();
}

int CloudApiClient::getSuccessRate() const
{
    if (call_history.isEmpty()) {
        return 0;
    }
    
    int successful = 0;
    for (const auto& log : call_history) {
        if (log.success) {
            successful++;
        }
    }
    
    return (successful * 100) / call_history.size();
}
