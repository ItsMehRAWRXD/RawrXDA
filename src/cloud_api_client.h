// cloud_api_client.h - Universal Cloud API Client for Multiple Providers
#ifndef CLOUD_API_CLIENT_H
#define CLOUD_API_CLIENT_H

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>
#include <functional>
#include <map>

class UniversalModelRouter;
struct ModelConfig;

// Structured logging
struct ApiCallLog {
    QString timestamp;
    QString provider;
    QString model;
    QString endpoint;
    QString request_body;
    QString response_body;
    int status_code;
    qint64 latency_ms;
    bool success;
    QString error_message;
};

// API response structure
struct ApiResponse {
    bool success;
    QString content;
    int status_code;
    QString raw_body;
    qint64 latency_ms;
    QString error_message;
    QJsonObject metadata;
};

// Streaming response structure
struct StreamingResponse {
    QString chunk;
    bool is_final;
    QString error;
};

// Cloud API Client
class CloudApiClient : public QObject {
    Q_OBJECT

public:
    explicit CloudApiClient(QObject* parent = nullptr);
    ~CloudApiClient();

    // Synchronous generation (blocking)
    QString generate(const QString& prompt, const ModelConfig& config);
    
    // Asynchronous generation
    void generateAsync(const QString& prompt, 
                      const ModelConfig& config,
                      std::function<void(const ApiResponse&)> callback);
    
    // Streaming generation
    void generateStream(const QString& prompt,
                       const ModelConfig& config,
                       std::function<void(const QString&)> chunk_callback,
                       std::function<void(const QString&)> error_callback = nullptr);
    
    // Provider health check
    bool checkProviderHealth(const ModelConfig& config);
    void checkProviderHealthAsync(const ModelConfig& config,
                                 std::function<void(bool)> callback);
    
    // Model listing
    QStringList listModels(const ModelConfig& config);
    void listModelsAsync(const ModelConfig& config,
                        std::function<void(const QStringList&)> callback);
    
    // Request building (for debugging/logging)
    QJsonObject buildRequestBody(const QString& prompt, const ModelConfig& config);
    
    // Logging and metrics
    QVector<ApiCallLog> getCallHistory() const;
    void clearCallHistory();
    ApiCallLog getLastCall() const;
    double getAverageLatency() const;
    int getSuccessRate() const;  // 0-100

signals:
    void generationCompleted(const ApiResponse& response);
    void generationFailed(const QString& error);
    void streamChunkReceived(const QString& chunk);
    void streamingCompleted();
    void streamingFailed(const QString& error);
    void healthCheckCompleted(bool healthy);
    void modelListReceived(const QStringList& models);

private slots:
    void onNetworkReplyFinished(QNetworkReply* reply);

private:
    // API request execution
    ApiResponse executeRequest(const QString& endpoint,
                             const QString& method,
                             const QJsonObject& body,
                             const QString& api_key,
                             const QMap<QString, QString>& headers = {});
    
    // Response parsing by backend
    QString parseAnthropicResponse(const QJsonObject& response);
    QString parseOpenAIResponse(const QJsonObject& response);
    QString parseGoogleResponse(const QJsonObject& response);
    QString parseMoonshotResponse(const QJsonObject& response);
    QString parseAzureOpenAIResponse(const QJsonObject& response);
    QString parseAwsBedrockResponse(const QJsonObject& response);
    
    // Request building by backend
    QJsonObject buildAnthropicRequest(const QString& prompt, const ModelConfig& config);
    QJsonObject buildOpenAIRequest(const QString& prompt, const ModelConfig& config);
    QJsonObject buildGoogleRequest(const QString& prompt, const ModelConfig& config);
    QJsonObject buildMoonshotRequest(const QString& prompt, const ModelConfig& config);
    QJsonObject buildAzureOpenAIRequest(const QString& prompt, const ModelConfig& config);
    QJsonObject buildAwsBedrockRequest(const QString& prompt, const ModelConfig& config);
    
    // Endpoint mapping
    struct ApiEndpoint {
        QString base_url;
        QString chat_endpoint;
        QString model_list_endpoint;
        std::function<QJsonObject(const QString&, const ModelConfig&)> request_builder;
        std::function<QString(const QJsonObject&)> response_parser;
    };
    
    // API endpoint definitions
    void initializeApiEndpoints();
    std::map<int, ApiEndpoint> api_endpoints;  // keyed by ModelBackend enum
    
    // Network management
    std::unique_ptr<QNetworkAccessManager> network_manager;
    QMap<QNetworkReply*, std::function<void(const QString&)>> pending_callbacks;
    
    // Call history and metrics
    QVector<ApiCallLog> call_history;
    static const int MAX_HISTORY_SIZE = 1000;
    
    // Error handling
    QString formatErrorResponse(int status_code, const QString& body);
    void logApiCall(const ApiCallLog& log);
};

#endif // CLOUD_API_CLIENT_H
