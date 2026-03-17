// universal_model_router.cpp - Implementation of Universal Model Router
#include "universal_model_router.h"
#include "cloud_api_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>

UniversalModelRouter::UniversalModelRouter(QObject* parent)
    : QObject(parent),
      local_engine_ready(false),
      cloud_client_ready(false)
{
    cloud_client = std::make_unique<CloudApiClient>(this);
}

UniversalModelRouter::~UniversalModelRouter() = default;

void UniversalModelRouter::registerModel(const QString& model_name, const ModelConfig& config)
{
    if (!config.isValid()) {
        emit error(QString("Invalid configuration for model: %1").arg(model_name));
        return;
    }
    
    model_registry[model_name] = config;
    emit modelRegistered(model_name, config.backend);
}

void UniversalModelRouter::unregisterModel(const QString& model_name)
{
    if (model_registry.remove(model_name) > 0) {
        emit modelUnregistered(model_name);
    }
}

ModelConfig UniversalModelRouter::getModelConfig(const QString& model_name) const
{
    if (model_registry.contains(model_name)) {
        return model_registry[model_name];
    }
    
    ModelConfig empty;
    empty.model_id = "";
    return empty;
}

QStringList UniversalModelRouter::getAvailableModels() const
{
    return model_registry.keys();
}

QStringList UniversalModelRouter::getModelsForBackend(ModelBackend backend) const
{
    QStringList models;
    
    for (const auto& name : model_registry.keys()) {
        if (model_registry[name].backend == backend) {
            models.append(name);
        }
    }
    
    return models;
}

bool UniversalModelRouter::loadConfigFromFile(const QString& config_file_path)
{
    QFile file(config_file_path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error(QString("Cannot open config file: %1").arg(config_file_path));
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        emit error("Config file is not valid JSON");
        return false;
    }
    
    return loadConfigFromJson(doc.object());
}

bool UniversalModelRouter::loadConfigFromJson(const QJsonObject& config_json)
{
    model_registry.clear();
    
    if (!config_json.contains("models")) {
        emit error("Config missing 'models' section");
        return false;
    }
    
    QJsonObject models_obj = config_json["models"].toObject();
    
    for (const auto& model_name : models_obj.keys()) {
        QJsonObject model_json = models_obj[model_name].toObject();
        
        ModelConfig config;
        config.full_config = model_json;
        config.model_id = model_json["model_id"].toString();
        config.description = model_json.value("description", "").toString();
        config.api_key = model_json["api_key"].toString();
        config.endpoint = model_json.value("endpoint", "").toString();
        
        // Parse backend
        QString backend_str = model_json["backend"].toString().toUpper();
        if (backend_str == "LOCAL_GGUF") {
            config.backend = ModelBackend::LOCAL_GGUF;
        } else if (backend_str == "OLLAMA_LOCAL") {
            config.backend = ModelBackend::OLLAMA_LOCAL;
        } else if (backend_str == "ANTHROPIC") {
            config.backend = ModelBackend::ANTHROPIC;
        } else if (backend_str == "OPENAI") {
            config.backend = ModelBackend::OPENAI;
        } else if (backend_str == "GOOGLE") {
            config.backend = ModelBackend::GOOGLE;
        } else if (backend_str == "MOONSHOT") {
            config.backend = ModelBackend::MOONSHOT;
        } else if (backend_str == "AZURE_OPENAI") {
            config.backend = ModelBackend::AZURE_OPENAI;
        } else if (backend_str == "AWS_BEDROCK") {
            config.backend = ModelBackend::AWS_BEDROCK;
        } else {
            emit error(QString("Unknown backend: %1").arg(backend_str));
            continue;
        }
        
        // Load parameters
        if (model_json.contains("parameters")) {
            QJsonObject params_obj = model_json["parameters"].toObject();
            for (const auto& key : params_obj.keys()) {
                config.parameters[key] = params_obj[key].toString();
            }
        }
        
        // Handle environment variable substitution
        if (config.api_key.startsWith("${") && config.api_key.endsWith("}")) {
            QString env_var = config.api_key.mid(2, config.api_key.length() - 3);
            QString env_value = QString::fromStdString(std::getenv(env_var.toStdString().c_str()));
            if (!env_value.isEmpty()) {
                config.api_key = env_value;
            }
        }
        
        registerModel(model_name, config);
    }
    
    emit configLoaded(model_registry.size());
    return true;
}

bool UniversalModelRouter::saveConfigToFile(const QString& config_file_path) const
{
    QJsonObject root;
    QJsonObject models_obj;
    
    for (const auto& name : model_registry.keys()) {
        const auto& config = model_registry[name];
        models_obj[name] = config.full_config;
    }
    
    root["models"] = models_obj;
    
    QJsonDocument doc(root);
    QFile file(config_file_path);
    
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    emit configSaved();
    return true;
}

void UniversalModelRouter::initializeLocalEngine(const QString& engine_config_path)
{
    // Initialize local GGUF engine
    // This would integrate with your existing QuantizationAwareInferenceEngine
    local_engine_ready = true;
    emit modelRegistered("local_engine_ready", ModelBackend::LOCAL_GGUF);
}

void UniversalModelRouter::initializeCloudClient()
{
    // Cloud client is already initialized in constructor
    cloud_client_ready = true;
}

ModelConfig UniversalModelRouter::getOrLoadModel(const QString& model_name)
{
    return getModelConfig(model_name);
}

bool UniversalModelRouter::isModelAvailable(const QString& model_name) const
{
    return model_registry.contains(model_name);
}

ModelBackend UniversalModelRouter::getModelBackend(const QString& model_name) const
{
    if (model_registry.contains(model_name)) {
        return model_registry[model_name].backend;
    }
    
    return ModelBackend::LOCAL_GGUF;  // Default
}

QString UniversalModelRouter::getModelDescription(const QString& model_name) const
{
    if (model_registry.contains(model_name)) {
        return model_registry[model_name].description;
    }
    
    return "";
}

QJsonObject UniversalModelRouter::getModelInfo(const QString& model_name) const
{
    if (model_registry.contains(model_name)) {
        return model_registry[model_name].full_config;
    }
    
    return QJsonObject();
}

void UniversalModelRouter::onLocalEngineInitialized()
{
    local_engine_ready = true;
}

void UniversalModelRouter::onCloudClientInitialized()
{
    cloud_client_ready = true;
}

void UniversalModelRouter::onEngineError(const QString& error)
{
    emit error(error);
}
