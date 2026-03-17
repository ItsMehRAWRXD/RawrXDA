// universal_model_router.h - Unified Interface for Local GGUF + Cloud Models
//
// STABLE API — frozen as of v1.0.0 (January 22, 2026)
// Breaking changes require MAJOR version bump (v2.0.0+)
// See FEATURE_FLAGS.md for API stability guarantees
//
#ifndef UNIVERSAL_MODEL_ROUTER_H
#define UNIVERSAL_MODEL_ROUTER_H

#include <QString>
#include <QObject>
#include <QHash>
#include <QMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <memory>
#include <functional>
#include <stdexcept>

class QuantizationAwareInferenceEngine;
struct QuantizationEngineNoopDeleter {
    void operator()(QuantizationAwareInferenceEngine*) const noexcept {}
};
class CloudApiClient;

// Model backend enumeration
enum class ModelBackend {
    LOCAL_GGUF,        // Local GGUF quantized models
    OLLAMA_LOCAL,      // Ollama API (local HTTP server)
    ANTHROPIC,         // Claude API (Anthropic)
    OPENAI,            // GPT-4, GPT-3.5 (OpenAI)
    GOOGLE,            // Gemini (Google)
    MOONSHOT,          // Kimi (Moonshot)
    AZURE_OPENAI,      // OpenAI through Azure
    AWS_BEDROCK        // Claude/Mistral through AWS
};

// Model configuration structure
struct ModelConfig {
    ModelBackend backend;
    QString model_id;                               // Model identifier (e.g., "gpt-4", "claude-3-opus")
    QString api_key;                                // API key (may be empty for local models)
    QString endpoint;                               // Custom endpoint URL
    QMap<QString, QString> parameters;              // Additional parameters
    QString description;                            // Human-readable description
    QJsonObject full_config;                        // Full JSON configuration
    
    // Validation
    bool isValid() const {
        if (model_id.isEmpty()) return false;
        
        // Cloud models require API key
        if (backend != ModelBackend::LOCAL_GGUF && 
            backend != ModelBackend::OLLAMA_LOCAL && 
            api_key.isEmpty()) {
            return false;
        }
        
        return true;
    }
};

// Streaming callback type
using StreamCallback = std::function<void(const QString&)>;

// Error callback type
using ErrorCallback = std::function<void(const QString&)>;

// Universal Model Router - Routes requests to appropriate backend
class UniversalModelRouter : public QObject {
    Q_OBJECT

public:
    explicit UniversalModelRouter(QObject* parent = nullptr);
    ~UniversalModelRouter();

    // Model registration and management
    void registerModel(const QString& model_name, const ModelConfig& config);
    void unregisterModel(const QString& model_name);
    ModelConfig getModelConfig(const QString& model_name) const;
    QStringList getAvailableModels() const;
    QStringList getModelsForBackend(ModelBackend backend) const;
    
    // Model configuration loading
    bool loadConfigFromFile(const QString& config_file_path);
    bool loadConfigFromJson(const QJsonObject& config_json);
    bool saveConfigToFile(const QString& config_file_path);
    
    // Backend initialization
    void initializeLocalEngine(const QString& engine_config_path);
    void initializeCloudClient();
    
    // Direct model access
    ModelConfig getOrLoadModel(const QString& model_name);
    bool isModelAvailable(const QString& model_name) const;
    ModelBackend getModelBackend(const QString& model_name) const;
    
    // Model info retrieval
    QString getModelDescription(const QString& model_name) const;
    QJsonObject getModelInfo(const QString& model_name) const;

signals:
    void modelRegistered(const QString& model_name, ModelBackend backend);
    void modelUnregistered(const QString& model_name);
    void configLoaded(int model_count);
    void configSaved();
    void routerError(const QString& error_message);

private slots:
    void onLocalEngineInitialized();
    void onCloudClientInitialized();
    void onEngineError(const QString& error);

private:
    QMap<QString, ModelConfig> model_registry;
    std::unique_ptr<QuantizationAwareInferenceEngine, QuantizationEngineNoopDeleter> local_engine;
    std::unique_ptr<CloudApiClient> cloud_client;
    bool local_engine_ready;
    bool cloud_client_ready;
};

#endif // UNIVERSAL_MODEL_ROUTER_H
