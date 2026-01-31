// cloud_api_client.cpp - Implementation of Cloud API Client
#include "cloud_api_client.h"
#include "universal_model_router.h"


CloudApiClient::CloudApiClient(void* parent)
    : void(parent),
      network_manager(std::make_unique<void*>(this))
{
    initializeApiEndpoints();
// Qt connect removed
}

CloudApiClient::~CloudApiClient() = default;

void CloudApiClient::initializeApiEndpoints()
{
    // ANTHROPIC API
    api_endpoints[static_cast<int>(ModelBackend::ANTHROPIC)] = {
        "https://api.anthropic.com",
        "/v1/messages",
        "/v1/models",
        [this](const std::string& prompt, const ModelConfig& config) {
            return buildAnthropicRequest(prompt, config);
        },
        [this](const void*& response) {
            return parseAnthropicResponse(response);
        }
    };
    
    // OPENAI API
    api_endpoints[static_cast<int>(ModelBackend::OPENAI)] = {
        "https://api.openai.com",
        "/v1/chat/completions",
        "/v1/models",
        [this](const std::string& prompt, const ModelConfig& config) {
            return buildOpenAIRequest(prompt, config);
        },
        [this](const void*& response) {
            return parseOpenAIResponse(response);
        }
    };
    
    // GOOGLE GEMINI API
    api_endpoints[static_cast<int>(ModelBackend::GOOGLE)] = {
        "https://generativelanguage.googleapis.com",
        "/v1beta/models/{model}:generateContent",
        "/v1beta/models",
        [this](const std::string& prompt, const ModelConfig& config) {
            return buildGoogleRequest(prompt, config);
        },
        [this](const void*& response) {
            return parseGoogleResponse(response);
        }
    };
    
    // MOONSHOT (KIMI) API
    api_endpoints[static_cast<int>(ModelBackend::MOONSHOT)] = {
        "https://api.moonshot.cn",
        "/v1/chat/completions",
        "/v1/models",
        [this](const std::string& prompt, const ModelConfig& config) {
            return buildMoonshotRequest(prompt, config);
        },
        [this](const void*& response) {
            return parseMoonshotResponse(response);
        }
    };
    
    // AZURE OPENAI API
    api_endpoints[static_cast<int>(ModelBackend::AZURE_OPENAI)] = {
        "", // Azure uses custom endpoint
        "/chat/completions",
        "/models",
        [this](const std::string& prompt, const ModelConfig& config) {
            return buildAzureOpenAIRequest(prompt, config);
        },
        [this](const void*& response) {
            return parseAzureOpenAIResponse(response);
        }
    };
    
    // AWS BEDROCK API
    api_endpoints[static_cast<int>(ModelBackend::AWS_BEDROCK)] = {
        "", // Bedrock uses regional endpoints
        "/model/invoke",
        "/foundation-models",
        [this](const std::string& prompt, const ModelConfig& config) {
            return buildAwsBedrockRequest(prompt, config);
        },
        [this](const void*& response) {
            return parseAwsBedrockResponse(response);
        }
    };
}

std::string CloudApiClient::generate(const std::string& prompt, const ModelConfig& config)
{
    if (!config.isValid()) {
        generationFailed("Invalid model configuration");
        return "";
    }
    
    ApiResponse response = executeRequest(
        api_endpoints[static_cast<int>(config.backend)].chat_endpoint,
        "POST",
        buildRequestBody(prompt, config),
        config.api_key
    );
    
    if (!response.success) {
        generationFailed(response.error_message);
        return "";
    }
    
    generationCompleted(response);
    return response.content;
}

void CloudApiClient::generateAsync(const std::string& prompt,
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

void CloudApiClient::generateStream(const std::string& prompt,
                                   const ModelConfig& config,
                                   std::function<void(const std::string&)> chunk_callback,
                                   std::function<void(const std::string&)> error_callback)
{
    // For now, implement as non-streaming (collect all then chunks)
    // In production, use SSE (Server-Sent Events) support
    std::string result = generate(prompt, config);
    
    if (!result.isEmpty()) {
        // Split result into chunks (simulate streaming)
        int chunk_size = 50;
        for (int i = 0; i < result.length(); i += chunk_size) {
            chunk_callback(result.mid(i, chunk_size));
        }
        streamingCompleted();
    } else {
        if (error_callback) {
            error_callback("Generation failed");
        }
        streamingFailed("Generation failed");
    }
}

bool CloudApiClient::checkProviderHealth(const ModelConfig& config)
{
    // Simple health check by attempting to list models
    std::vector<std::string> models = listModels(config);
    bool healthy = !models.isEmpty();
    
    ApiCallLog log;
    log.timestamp = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    log.provider = std::string::number(static_cast<int>(config.backend));
    log.endpoint = api_endpoints[static_cast<int>(config.backend)].model_list_endpoint;
    log.success = healthy;
    logApiCall(log);
    
    healthCheckCompleted(healthy);
    return healthy;
}

void CloudApiClient::checkProviderHealthAsync(const ModelConfig& config,
                                              std::function<void(bool)> callback)
{
    bool result = checkProviderHealth(config);
    callback(result);
}

std::vector<std::string> CloudApiClient::listModels(const ModelConfig& config)
{
    // This would need implementation for each provider
    // For now, return empty list
    return std::vector<std::string>();
}

void CloudApiClient::listModelsAsync(const ModelConfig& config,
                                     std::function<void(const std::vector<std::string>&)> callback)
{
    auto models = listModels(config);
    callback(models);
}

void* CloudApiClient::buildRequestBody(const std::string& prompt, const ModelConfig& config)
{
    return api_endpoints[static_cast<int>(config.backend)].request_builder(prompt, config);
}

// ============ REQUEST BUILDERS ============

void* CloudApiClient::buildAnthropicRequest(const std::string& prompt, const ModelConfig& config)
{
    void* request;
    request["model"] = config.model_id;
    request["max_tokens"] = config.parameters.value("max_tokens", "4096").toInt();
    
    if (config.parameters.contains("temperature")) {
        request["temperature"] = config.parameters.value("temperature").toDouble();
    }
    
    void* messages;
    void* userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);
    
    request["messages"] = messages;
    
    return request;
}

void* CloudApiClient::buildOpenAIRequest(const std::string& prompt, const ModelConfig& config)
{
    void* request;
    request["model"] = config.model_id;
    
    if (config.parameters.contains("max_tokens")) {
        request["max_tokens"] = config.parameters.value("max_tokens").toInt();
    }
    
    if (config.parameters.contains("temperature")) {
        request["temperature"] = config.parameters.value("temperature").toDouble();
    }
    
    void* messages;
    void* userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);
    
    request["messages"] = messages;
    
    return request;
}

void* CloudApiClient::buildGoogleRequest(const std::string& prompt, const ModelConfig& config)
{
    void* request;
    
    void* contents;
    void* content;
    content["role"] = "user";
    
    void* parts;
    void* part;
    part["text"] = prompt;
    parts.append(part);
    
    content["parts"] = parts;
    contents.append(content);
    
    request["contents"] = contents;
    
    return request;
}

void* CloudApiClient::buildMoonshotRequest(const std::string& prompt, const ModelConfig& config)
{
    void* request;
    request["model"] = config.model_id;
    
    if (config.parameters.contains("max_tokens")) {
        request["max_tokens"] = config.parameters.value("max_tokens").toInt();
    }
    
    void* messages;
    void* userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = prompt;
    messages.append(userMessage);
    
    request["messages"] = messages;
    
    return request;
}

void* CloudApiClient::buildAzureOpenAIRequest(const std::string& prompt, const ModelConfig& config)
{
    // Azure uses OpenAI format but different endpoint
    return buildOpenAIRequest(prompt, config);
}

void* CloudApiClient::buildAwsBedrockRequest(const std::string& prompt, const ModelConfig& config)
{
    void* request;
    
    // Bedrock uses provider-specific formats
    // This is a generic wrapper
    void* messages;
    messages["prompt"] = prompt;
    
    if (config.parameters.contains("max_tokens")) {
        messages["max_tokens_to_sample"] = config.parameters.value("max_tokens").toInt();
    }
    
    request["body"] = messages;
    
    return request;
}

// ============ RESPONSE PARSERS ============

std::string CloudApiClient::parseAnthropicResponse(const void*& response)
{
    if (!response.contains("content") || response["content"].toArray().isEmpty()) {
        return "";
    }
    
    void* content = response["content"].toArray();
    if (content[0].toObject().contains("text")) {
        return content[0].toObject()["text"].toString();
    }
    
    return "";
}

std::string CloudApiClient::parseOpenAIResponse(const void*& response)
{
    if (!response.contains("choices") || response["choices"].toArray().isEmpty()) {
        return "";
    }
    
    void* choices = response["choices"].toArray();
    void* choice = choices[0].toObject();
    
    if (choice.contains("message")) {
        return choice["message"].toObject()["content"].toString();
    }
    
    return "";
}

std::string CloudApiClient::parseGoogleResponse(const void*& response)
{
    if (!response.contains("candidates") || response["candidates"].toArray().isEmpty()) {
        return "";
    }
    
    void* candidates = response["candidates"].toArray();
    void* candidate = candidates[0].toObject();
    
    if (candidate.contains("content") && candidate["content"].toObject().contains("parts")) {
        void* parts = candidate["content"].toObject()["parts"].toArray();
        if (!parts.isEmpty()) {
            return parts[0].toObject()["text"].toString();
        }
    }
    
    return "";
}

std::string CloudApiClient::parseMoonshotResponse(const void*& response)
{
    // Moonshot uses same format as OpenAI
    return parseOpenAIResponse(response);
}

std::string CloudApiClient::parseAzureOpenAIResponse(const void*& response)
{
    // Azure uses same format as OpenAI
    return parseOpenAIResponse(response);
}

std::string CloudApiClient::parseAwsBedrockResponse(const void*& response)
{
    if (response.contains("body")) {
        std::string body = response["body"].toString();
        // Parse Bedrock-specific response format
        return body;
    }
    
    return "";
}

// ============ UTILITY METHODS ============

ApiResponse CloudApiClient::executeRequest(const std::string& endpoint,
                                          const std::string& method,
                                          const void*& body,
                                          const std::string& api_key,
                                          const std::map<std::string, std::string>& headers)
{
    ApiResponse response;
    response.success = false;
    response.status_code = 0;
    response.error_message = "Not implemented in blocking mode";
    
    return response;
}

std::string CloudApiClient::formatErrorResponse(int status_code, const std::string& body)
{
    return std::string("HTTP %1: %2");
}

void CloudApiClient::logApiCall(const ApiCallLog& log)
{
    call_history.append(log);
    
    // Keep history size bounded
    if (call_history.size() > MAX_HISTORY_SIZE) {
        call_history.removeFirst();
    }
}

void CloudApiClient::onNetworkReplyFinished(void** reply)
{
    // Handle completed network requests
    if (reply->error() != void*::NoError) {
        generationFailed(reply->errorString());
    }
    
    reply->deleteLater();
}

std::vector<ApiCallLog> CloudApiClient::getCallHistory() const
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

