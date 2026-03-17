/**
 * @file ai_completion_provider.cpp
 * @brief AI-powered code completion provider (Ollama/local LLM)
 * 
 * @author RawrXD Team
 * @date 2026-01-07
 */

#include "ai_completion_provider.h"
#include "logging/logger.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>
#include <QCoreApplication>

namespace RawrXD {

AICompletionProvider::AICompletionProvider(QObject* parent)
    : QObject(parent)
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
    const QString& prefix,
    const QString& suffix,
    const QString& filePath,
    const QString& fileType,
    const QStringList& contextLines)
{
    if (m_isPending) {
        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::requestCompletions",
            "Request already pending, ignoring new request"
        );
        return;
    }

    if (prefix.length() < 2 && prefix.trimmed().isEmpty()) {
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
        " (file type: " + fileType + ", prefix length: " + QString::number(prefix.length()) + ")"
    );

    // Process asynchronously to not block UI
    QTimer::singleShot(0, this, &AICompletionProvider::onCompletionRequest);
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

void AICompletionProvider::setModelEndpoint(const QString& endpoint)
{
    m_modelEndpoint = endpoint;
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Model endpoint set to: " + endpoint
    );
}

void AICompletionProvider::setModel(const QString& modelName)
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
        "Request timeout set to: " + QString::number(timeoutMs) + "ms"
    );
}

void AICompletionProvider::setTimeoutFallback(bool enabled)
{
    m_useTimeoutFallback = enabled;
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Timeout fallback: " + QString(enabled ? "enabled" : "disabled")
    );
}

void AICompletionProvider::setMinConfidence(float threshold)
{
    m_minConfidence = qBound(0.0f, threshold, 1.0f);
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Min confidence threshold set to: " + QString::number(m_minConfidence)
    );
}

void AICompletionProvider::setMaxSuggestions(int count)
{
    m_maxSuggestions = qMax(1, count);
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider",
        "Max suggestions set to: " + QString::number(count)
    );
}

void AICompletionProvider::onCompletionRequest()
{
    if (!m_isPending) {
        return;
    }

    QElapsedTimer timer;
    timer.start();

    try {
        // Format the prompt for the model
        QStringList contextLines; // TODO: Pass from caller
        QString prompt = formatPrompt(m_lastPrefix, m_lastSuffix, m_lastFileType, contextLines);

        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::onCompletionRequest",
            "Calling model: " + m_modelName + " at " + m_modelEndpoint
        );

        // Call the model (this will block in background thread)
        QString response = callModel(prompt);

        qint64 elapsed = timer.elapsed();
        m_lastLatencyMs = elapsed;

        Logger::instance()->log(
            Logger::DEBUG,
            "AICompletionProvider::onCompletionRequest",
            "Model responded in " + QString::number(elapsed) + "ms"
        );

        // Parse the response into structured completions
        QVector<AICompletion> completions = parseCompletions(response);

        // Filter by confidence
        QVector<AICompletion> filtered;
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
            "Emitting " + QString::number(filtered.size()) + " completions"
        );

        m_isPending = false;
        emit latencyReported(m_lastLatencyMs);
        emit completionsReady(filtered);

    } catch (const std::exception& ex) {
        m_isPending = false;
        QString errorMsg = QString::fromStdString(ex.what());
        Logger::instance()->log(
            Logger::ERROR,
            "AICompletionProvider::onCompletionRequest",
            "Exception: " + errorMsg
        );
        emit error(errorMsg);
    }
}

QString AICompletionProvider::formatPrompt(
    const QString& prefix,
    const QString& suffix,
    const QString& fileType,
    const QStringList& contextLines)
{
    // Simple prompt format for code completion
    // Can be extended with more sophisticated prompt engineering
    
    QString prompt;
    prompt += "Language: " + fileType + "\n";
    
    if (!contextLines.isEmpty()) {
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

QVector<AICompletion> AICompletionProvider::parseCompletions(const QString& response)
{
    QVector<AICompletion> completions;
    
    // Simple parsing: split by newlines and create completions
    // In production, this would parse JSON or structured format from the model
    
    QStringList lines = response.split('\n', Qt::SkipEmptyParts);
    
    int ranking = 0;
    for (const auto& line : lines) {
        if (line.trimmed().isEmpty()) {
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

float AICompletionProvider::extractConfidence(const QString& suggestion)
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

QString AICompletionProvider::callModel(const QString& prompt)
{
    QNetworkAccessManager manager;
    
    // Build Ollama API request
    QUrl url(m_modelEndpoint + "/api/generate");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject jsonBody;
    jsonBody["model"] = m_modelName;
    jsonBody["prompt"] = prompt;
    jsonBody["stream"] = false; // Get complete response at once
    jsonBody["temperature"] = 0.5; // Balanced between creativity and precision
    
    QJsonDocument doc(jsonBody);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    // Create event loop for synchronous-style wait
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    
    QString response;
    bool requestFailed = false;
    
    // NOTE: In production, this should use async networking with proper threading
    // For now, this is a blocking implementation for simplicity
    
    Logger::instance()->log(
        Logger::DEBUG,
        "AICompletionProvider::callModel",
        "Sending request to: " + url.toString()
    );
    
    // TODO: Implement actual network call using QNetworkAccessManager::post()
    // This is a placeholder that simulates the API call
    response = "// TODO: Model completion suggestion";
    
    return response;
}

} // namespace RawrXD
