#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * @brief Request structure for unified inference backend
 */
struct UnifiedRequest {
    QString prompt;
    qint64  reqId;
    QString backend;   // "local" | "llama" | "openai" | "claude" | "gemini"
    QString apiKey;
};

/**
 * @brief Unified backend supporting local GGUF and remote API inference
 * 
 * Handles streaming responses from:
 * - Local GGUF models (via InferenceEngine worker thread)
 * - llama.cpp HTTP server (self-hosted)
 * - OpenAI API (gpt-3.5-turbo, gpt-4)
 * - Anthropic Claude API (claude-3-sonnet)
 * - Google Gemini API (gemini-pro)
 */
class UnifiedBackend : public QObject {
    Q_OBJECT
public:
    explicit UnifiedBackend(QObject* parent = nullptr);
    
    /**
     * @brief Submit inference request to configured backend
     * @param req Request with prompt, backend ID, and optional API key
     */
    void submit(const UnifiedRequest& req);
    
    /**
     * @brief Set the local inference engine (for "local" backend)
     */
    void setLocalEngine(QObject* engine) { m_localEngine = engine; }

signals:
    /**
     * @brief Emitted for each token during streaming inference
     * @param reqId Request identifier
     * @param token Single token or character from model
     */
    void streamToken(qint64 reqId, const QString& token);
    
    /**
     * @brief Emitted when streaming inference completes
     * @param reqId Request identifier
     */
    void streamFinished(qint64 reqId);
    
    /**
     * @brief Emitted on inference error
     * @param reqId Request identifier
     * @param error Error message
     */
    void error(qint64 reqId, const QString& error);

private slots:
    void onLocalDone(qint64 id, const QString& answer);

private:
    void submitLlamaCpp(const UnifiedRequest& req);
    void submitOpenAI(const UnifiedRequest& req);
    void submitClaude(const UnifiedRequest& req);
    void submitGemini(const UnifiedRequest& req);
    
    QNetworkAccessManager* m_nam{nullptr};
    QObject* m_localEngine{nullptr};
};
