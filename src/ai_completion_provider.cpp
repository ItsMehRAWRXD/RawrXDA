/**
 * @file ai_completion_provider.cpp
 * @brief AI-powered code completion provider (Ollama/local LLM)
 * 
 * @author RawrXD Team
 * @date 2026-01-07
 */

#include "ai_completion_provider.h"

namespace RawrXD {

AICompletionProvider::AICompletionProvider()
    
    , m_isPending(false)
    , m_lastLatencyMs(0.0)
{
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Initialized with endpoint: " + m_modelEndpoint + ", model: " + m_modelName
    );
}

AICompletionProvider::~AICompletionProvider()
{
    if (m_isPending) {
        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider",
            "Destroying while request pending - cancelling"
        );
        cancelPendingRequest();
    }
}

void AICompletionProvider::requestCompletions(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filePath,
    const std::string& fileType,
    const std::stringList& contextLines)
{
    if (m_isPending) {
        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::requestCompletions",
            "Request already pending, ignoring new request"
        );
        return;
    }

    if (prefix.length() < 2 && prefix.trimmed().empty()) {
        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::requestCompletions",
            "Prefix too short, skipping"
        );
        return;
    }

    m_isPending = true;
    m_lastPrefix = prefix;
    m_lastSuffix = suffix;
    m_lastFileType = fileType;

    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider::requestCompletions",
        "Requesting completions for: " + filePath +
        " (file type: " + fileType + ", prefix length: " + std::string::number(prefix.length()) + ")"
    );

    // Process asynchronously to not block UI
    // Timer operation removed
}

void AICompletionProvider::cancelPendingRequest()
{
    if (m_isPending) {
        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider",
            "Cancelled pending completion request"
        );
        m_isPending = false;
    }
}

void AICompletionProvider::setModelEndpoint(const std::string& endpoint)
{
    m_modelEndpoint = endpoint;
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Model endpoint set to: " + endpoint
    );
}

void AICompletionProvider::setModel(const std::string& modelName)
{
    m_modelName = modelName;
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Model set to: " + modelName
    );
}

void AICompletionProvider::setRequestTimeout(int timeoutMs)
{
    m_requestTimeoutMs = timeoutMs;
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Request timeout set to: " + std::string::number(timeoutMs) + "ms"
    );
}

void AICompletionProvider::setTimeoutFallback(bool enabled)
{
    m_useTimeoutFallback = enabled;
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Timeout fallback: " + std::string(enabled ? "enabled" : "disabled")
    );
}

void AICompletionProvider::setMinConfidence(float threshold)
{
    m_minConfidence = qBound(0.0f, threshold, 1.0f);
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Min confidence threshold set to: " + std::string::number(m_minConfidence)
    );
}

void AICompletionProvider::setMaxSuggestions(int count)
{
    m_maxSuggestions = qMax(1, count);
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Max suggestions set to: " + std::string::number(count)
    );
}

void AICompletionProvider::onCompletionRequest()
{
    if (!m_isPending) {
        return;
    }

    std::chrono::steady_clock::time_point timer;
    timer.start();

    try {
        // Format the prompt for the model
        std::stringList contextLines; // TODO: Pass from caller
        std::string prompt = formatPrompt(m_lastPrefix, m_lastSuffix, m_lastFileType, contextLines);

        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::onCompletionRequest",
            "Calling model: " + m_modelName + " at " + m_modelEndpoint
        );

        // Call the model (this will block in background thread)
        std::string response = callModel(prompt);

        int64_t elapsed = timer.elapsed();
        m_lastLatencyMs = elapsed;

        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::onCompletionRequest",
            "Model responded in " + std::string::number(elapsed) + "ms"
        );

        // Parse the response into structured completions
        std::vector<AICompletion> completions = parseCompletions(response);

        // Filter by confidence
        std::vector<AICompletion> filtered;
        for (const auto& completion : completions) {
            if (completion.confidence >= m_minConfidence) {
                filtered.push_back(completion);
            }
        }

        // Limit to max suggestions
        if (filtered.size() > m_maxSuggestions) {
            filtered.resize(m_maxSuggestions);
        }

        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::onCompletionRequest",
            "Emitting " + std::string::number(filtered.size()) + " completions"
        );

        m_isPending = false;
        latencyReported(m_lastLatencyMs);
        completionsReady(filtered);

    } catch (const std::exception& ex) {
        m_isPending = false;
        std::string errorMsg = std::string::fromStdString(ex.what());
        Logger::instance()->log(
            Logger::ERROR,
            "AICompletionProvider::onCompletionRequest",
            "Exception: " + errorMsg
        );
        error(errorMsg);
    }
}

std::string AICompletionProvider::formatPrompt(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& fileType,
    const std::stringList& contextLines)
{
    // Simple prompt format for code completion
    // Can be extended with more sophisticated prompt engineering
    
    std::string prompt;
    prompt += "Language: " + fileType + "\n";
    
    if (!contextLines.empty()) {
        prompt += "Context:\n";
        for (const auto& line : contextLines) {
            prompt += line + "\n";
        }
    }
    
    prompt += "Current line before cursor: " + prefix + "\n";
    prompt += "Complete the code:\n";
    prompt += prefix;
    
    return prompt;
}

std::vector<AICompletion> AICompletionProvider::parseCompletions(const std::string& response)
{
    std::vector<AICompletion> completions;
    
    // Simple parsing: split by newlines and create completions
    // In production, this would parse JSON or structured format from the model
    
    std::stringList lines = response.split('\n', SkipEmptyParts);
    
    int ranking = 0;
    for (const auto& line : lines) {
        if (line.trimmed().empty()) {
            continue;
        }
        
        AICompletion completion;
        completion.text = line.trimmed();
        completion.type = "keyword"; // TODO: Detect type
        completion.confidence = extractConfidence(line);
        completion.ranking = ranking++;
        
        completions.push_back(completion);
        
        if (ranking >= m_maxSuggestions) {
            break;
        }
    }
    
    return completions;
}

float AICompletionProvider::extractConfidence(const std::string& suggestion)
{
    // TODO: Extract confidence from model output if available
    // For now, use a simple heuristic based on suggestion characteristics
    
    if (suggestion.length() < 3) {
        return 0.4f;
    } else if (suggestion.length() < 10) {
        return 0.7f;
    } else {
        return 0.5f;
    }
}

std::string AICompletionProvider::callModel(const std::string& prompt)
{
    // Build Ollama API request
    std::string url(m_modelEndpoint + "/api/generate");
    void* request(url);
    request.setHeader(void*::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "RawrXD-IDE/1.0");
    
    // Build request body
    void* jsonBody;
    jsonBody["model"] = m_modelName;
    jsonBody["prompt"] = prompt;
    jsonBody["stream"] = false; // Get complete response at once
    jsonBody["temperature"] = 0.5; // Balanced between creativity and precision
    jsonBody["top_p"] = 0.9;
    jsonBody["top_k"] = 40;
    jsonBody["num_predict"] = 100; // Limit response length for completion context
    
    void* doc(jsonBody);
    std::vector<uint8_t> postData = doc.toJson(void*::Compact);
    
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider::callModel",
        "Sending request to: " + url.toString() + ", model: " + m_modelName +
        ", prompt length: " + std::string::number(prompt.length())
    );
    
    // Create network manager for this request
    void* manager;
    
    // Create event loop to wait for response synchronously
    voidLoop loop;
    // Timer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(m_requestTimeoutMs);
    
    std::string responseText;
    bool success = false;  // Signal connection removed\n// Make the HTTP POST request
    void** reply = manager.post(request, postData);  // Signal connection removed\nloop.quit();
    });  // Signal connection removed\n// Parse JSON response
            void* jsonDoc = void*::fromJson(responseData);
            if (jsonDoc.isObject()) {
                void* jsonObj = jsonDoc.object();
                responseText = jsonObj["response"].toString();
                success = true;
                
                Logger::instance()->log(
                    Logger::DEBUG,
                    "AICompletionProvider::callModel",
                    "Response received: " + responseText.left(100)
                );
            } else {
                Logger::instance()->log(
                    Logger::ERROR,
                    "AICompletionProvider::callModel",
                    "Invalid JSON response"
                );
            }
        } else {
            Logger::instance()->log(
                Logger::ERROR,
                "AICompletionProvider::callModel",
                "HTTP error: " + std::string::number(reply->attribute(void*::HttpStatusCodeAttribute))
            );
        }
        loop.quit();
    });
    
    timeoutTimer.start();
    loop.exec();
    reply->deleteLater();
    
    if (!success && m_useTimeoutFallback) {
        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::callModel",
            "Using fallback completion (model request failed/timeout)"
        );
        responseText = generateFallbackCompletion(prompt);
    }
    
    return responseText;
}

std::string AICompletionProvider::generateFallbackCompletion(const std::string& prompt)
{
    // Generate simple keyword-based completion when model is unavailable
    // This ensures IDE still provides some help when LLM is down
    
    if (prompt.contains("for") || prompt.contains("while")) {
        return "(i = 0; i < n; i++)";
    } else if (prompt.contains("if")) {
        return "(condition) {\n    // code here\n}";
    } else if (prompt.contains("function") || prompt.contains("def")) {
        return "() {\n    return 0;\n}";
    } else if (prompt.contains("class")) {
        return " {\npublic:\n    // members\n};";
    } else if (prompt.endsWith(".")) {
        return " "; // Likely method call context
    } else if (prompt.endsWith("(")) {
        return ");";
    } else if (prompt.endsWith("{")) {
        return "\n    // code\n}";
    }
    
    // Default fallback
    return " ";
}

} // namespace RawrXD






