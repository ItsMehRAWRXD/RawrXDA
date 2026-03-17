#ifndef OLLAMAINTERFACE_H
#define OLLAMAINTERFACE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

/**
 * @brief Native interface to Ollama local LLM server
 * 
 * Connects to your existing Ollama setup at localhost:11434
 * Supports streaming, embeddings, and all Ollama model features
 * 
 * Usage:
 *   OllamaInterface* ollama = new OllamaInterface();
 *   ollama->setModel("ministral-3");
 *   ollama->sendCompletionRequest("Write a function to...");
 */
class OllamaInterface : public QObject
{
    Q_OBJECT

public:
    struct OllamaConfig {
        QString host = "http://localhost:11434";
        QString defaultModel = "ministral-3";
        int timeout = 60000; // 60 seconds
        bool streamingEnabled = true;
        
        QJsonObject toJson() const;
        static OllamaConfig fromJson(const QJsonObject& obj);
    };

    struct GenerateOptions {
        int numPredict = -1;        // Max tokens (-1 = unlimited)
        double temperature = 0.7;
        int topK = 40;
        double topP = 0.9;
        int repeatLastN = 64;
        double repeatPenalty = 1.1;
        int seed = -1;              // -1 = random
        QStringList stop;           // Stop sequences
        
        QJsonObject toJson() const;
    };

    explicit OllamaInterface(QObject *parent = nullptr);
    ~OllamaInterface();

    // Configuration
    void setConfig(const OllamaConfig& config);
    OllamaConfig getConfig() const;
    void setModel(const QString& modelName);
    QString getCurrentModel() const;

    // Core API methods
    void sendCompletionRequest(const QString& prompt, const GenerateOptions& options = GenerateOptions());
    void sendChatRequest(const QJsonArray& messages, const GenerateOptions& options = GenerateOptions());
    void sendEmbeddingRequest(const QString& text);
    void sendEmbeddingRequest(const QStringList& texts);
    
    // Model management
    void listModels();
    void showModelInfo(const QString& modelName);
    void pullModel(const QString& modelName);
    void deleteModel(const QString& modelName);
    
    // Streaming control
    void enableStreaming(bool enable);
    bool isStreamingEnabled() const;
    void cancelCurrentRequest();

    // Health check
    void ping();
    bool isServerAvailable() const;

signals:
    // Response signals
    void completionReceived(const QString& completion, const QJsonObject& metadata);
    void chatResponseReceived(const QString& response, const QJsonObject& metadata);
    void embeddingReceived(const QJsonArray& embedding);
    void streamingChunkReceived(const QString& chunk, bool isFinal);
    
    // Model management signals
    void modelsListed(const QJsonArray& models);
    void modelInfoReceived(const QJsonObject& info);
    void modelPullProgress(int percentage, const QString& status);
    void modelPullCompleted(const QString& modelName);
    void modelDeleted(const QString& modelName);
    
    // Status signals
    void requestStarted();
    void requestCompleted();
    void serverAvailabilityChanged(bool available);
    void errorOccurred(const QString& error, const QJsonObject& context);

private slots:
    void onNetworkReply();
    void onStreamingData();
    void onTimeout();
    void onPingReply();

private:
    QNetworkAccessManager* m_networkManager;
    OllamaConfig m_config;
    QString m_currentModel;
    bool m_streamingEnabled;
    bool m_serverAvailable;
    
    QNetworkReply* m_currentReply;
    QTimer* m_timeoutTimer;
    QString m_accumulatedStreamData;
    
    // Request builders
    QNetworkRequest createRequest(const QString& endpoint);
    QJsonObject buildGeneratePayload(const QString& prompt, const GenerateOptions& options);
    QJsonObject buildChatPayload(const QJsonArray& messages, const GenerateOptions& options);
    QJsonObject buildEmbeddingPayload(const QString& text);
    
    // Response parsers
    void parseGenerateResponse(const QByteArray& data, bool isStreaming = false);
    void parseChatResponse(const QByteArray& data, bool isStreaming = false);
    void parseEmbeddingResponse(const QByteArray& data);
    void parseStreamingChunk(const QString& chunk);
    
    // Helper methods
    void sendRequest(const QString& endpoint, const QJsonObject& payload);
    void handleError(const QString& error, const QJsonObject& context = QJsonObject());
    void logRequest(const QString& endpoint, const QJsonObject& payload);
    void logResponse(const QByteArray& data);
};

#endif // OLLAMAINTERFACE_H
