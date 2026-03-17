#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

/**
 * @class OllamaProxy
 * @brief Lightweight fallback proxy to Ollama REST API
 * 
 * Only used when:
 * 1. Model exists in Ollama registry but not as plain .gguf in OllamaModels
 * 2. GGUF uses quantization our engine doesn't support yet
 * 
 * Preserves all custom optimizations 95% of the time by using direct GGUF loading.
 * Falls back to Ollama only for edge cases (new quants, unsupported architectures).
 */
class OllamaProxy : public QObject {
    Q_OBJECT
public:
    explicit OllamaProxy(QObject* parent = nullptr);
    ~OllamaProxy();
    
    // Set which Ollama model to use (e.g., "llama3.2:3b", "unlocked-350M:latest")
    void setModel(const QString& modelName);
    QString currentModel() const { return m_modelName; }
    
    // Detect blobs in a directory (e.g., D:/OllamaModels)
    void detectBlobs(const QString& modelDir);
    
    // Get list of detected models from blobs/manifests
    QStringList detectedModels() const { return m_detectedModels.keys(); }
    
    // Check if a path is an Ollama blob
    bool isBlobPath(const QString& path) const;
    
    // Resolve a blob path to a model name
    QString resolveBlobToModel(const QString& blobPath) const;

    // Check if Ollama is running and model is available
    bool isOllamaAvailable();
    bool isModelAvailable(const QString& modelName);
    
    // Generate response using Ollama API with streaming
    void generateResponse(const QString& prompt, 
                         float temperature = 0.8f,
                         int maxTokens = 512);
    
    // Stop current generation
    void stopGeneration();
    
signals:
    // Emitted for each token during streaming (compatible with AgenticEngine)
    void tokenArrived(const QString& token);
    
    // Emitted when generation completes
    void generationComplete();
    
    // Emitted on errors
    void error(const QString& message);
    
private slots:
    void onNetworkReply();
    void onNetworkError(QNetworkReply::NetworkError code);
    
private:
    QString m_modelName;
    QString m_ollamaUrl;  // Default: http://localhost:11434
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    
    QByteArray m_buffer;  // Buffer for partial SSE events
    
    // Map of model name to blob path
    QMap<QString, QString> m_detectedModels;
    // Map of blob hash to model name
    QMap<QString, QString> m_blobToModel;
