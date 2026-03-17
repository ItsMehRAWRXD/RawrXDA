// model_interface.cpp - Implementation of Unified Model Interface
#include "model_interface.h"
#include "universal_model_router.h"
#include "cloud_api_client.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QTimer>
#include <algorithm>

ModelInterface::ModelInterface(QObject* parent)
    : QObject(parent),
      default_model(""),
      initialized_flag(false)
{
    // Initialization deferred to initialize() method
}

ModelInterface::~ModelInterface() = default;

void ModelInterface::initialize(const QString& config_file_path)
{
    router = std::make_shared<UniversalModelRouter>(this);
    cloud_client = std::make_shared<CloudApiClient>(this);
    
    if (router->loadConfigFromFile(config_file_path)) {
        initialized_flag = true;
        emit initialized();
    }
}

void ModelInterface::initializeWithRouter(std::shared_ptr<UniversalModelRouter> provided_router)
{
    router = provided_router;
    cloud_client = std::make_shared<CloudApiClient>(this);
    initialized_flag = (router != nullptr);
    
    if (initialized_flag) {
        emit initialized();
    }
}

bool ModelInterface::isInitialized() const
{
    return initialized_flag;
}

GenerationResult ModelInterface::generate(const QString& prompt,
                                         const QString& model_name,
                                         const GenerationOptions& options)
{
    if (!initialized_flag || !router) {
        GenerationResult result;
        result.success = false;
        result.error = "ModelInterface not initialized";
        return result;
    }
    
    return generateInternal(prompt, model_name, options);
}

void ModelInterface::generateAsync(const QString& prompt,
                                  const QString& model_name,
                                  std::function<void(const GenerationResult&)> callback,
                                  const GenerationOptions& options)
{
    // In a full implementation, this would be truly async with threading
    // For now, we use a deferred callback
    auto result = generate(prompt, model_name, options);
    
    QTimer::singleShot(0, this, [this, result, callback]() {
        callback(result);
    });
}

void ModelInterface::generateStream(const QString& prompt,
                                   const QString& model_name,
                                   std::function<void(const QString&)> on_chunk,
                                   std::function<void(const QString&)> on_error,
                                   const GenerationOptions& options)
{
    if (!initialized_flag || !router) {
        if (on_error) {
            on_error("ModelInterface not initialized");
        }
        return;
    }
    
    generateStreamInternal(prompt, model_name, on_chunk, on_error, options);
}

QVector<GenerationResult> ModelInterface::generateBatch(const QStringList& prompts,
                                                        const QString& model_name,
                                                        const GenerationOptions& options)
{
    QVector<GenerationResult> results;
    
    for (const auto& prompt : prompts) {
        results.append(generate(prompt, model_name, options));
    }
    
    return results;
}

void ModelInterface::generateBatchAsync(const QStringList& prompts,
                                       const QString& model_name,
                                       std::function<void(const QVector<GenerationResult>&)> callback,
                                       const GenerationOptions& options)
{
    auto results = generateBatch(prompts, model_name, options);
    
    QTimer::singleShot(0, this, [this, results, callback]() {
        callback(results);
    });
}

QStringList ModelInterface::getAvailableModels() const
{
    if (!router) return QStringList();
    return router->getAvailableModels();
}

QStringList ModelInterface::getLocalModels() const
{
    if (!router) return QStringList();
    return router->getModelsForBackend(ModelBackend::LOCAL_GGUF)
         + router->getModelsForBackend(ModelBackend::OLLAMA_LOCAL);
}

QStringList ModelInterface::getCloudModels() const
{
    if (!router) return QStringList();
    
    QStringList cloud_models;
    cloud_models += router->getModelsForBackend(ModelBackend::ANTHROPIC);
    cloud_models += router->getModelsForBackend(ModelBackend::OPENAI);
    cloud_models += router->getModelsForBackend(ModelBackend::GOOGLE);
    cloud_models += router->getModelsForBackend(ModelBackend::MOONSHOT);
    cloud_models += router->getModelsForBackend(ModelBackend::AZURE_OPENAI);
    cloud_models += router->getModelsForBackend(ModelBackend::AWS_BEDROCK);
    
    return cloud_models;
}

QString ModelInterface::getModelDescription(const QString& model_name) const
{
    if (!router) return "";
    return router->getModelDescription(model_name);
}

QJsonObject ModelInterface::getModelInfo(const QString& model_name) const
{
    if (!router) return QJsonObject();
    return router->getModelInfo(model_name);
}

bool ModelInterface::modelExists(const QString& model_name) const
{
    if (!router) return false;
    return router->isModelAvailable(model_name);
}

void ModelInterface::registerModel(const QString& model_name, const ModelConfig& config)
{
    if (router) {
        router->registerModel(model_name, config);
        emit modelListUpdated(getAvailableModels());
    }
}

void ModelInterface::unregisterModel(const QString& model_name)
{
    if (router) {
        router->unregisterModel(model_name);
        emit modelListUpdated(getAvailableModels());
    }
}

QString ModelInterface::selectBestModel(const QString& task_type,
                                       const QString& language,
                                       bool prefer_local)
{
    // Smart model selection logic
    auto available = getAvailableModels();
    
    if (available.isEmpty()) {
        return "";
    }
    
    // Prefer local if requested
    if (prefer_local) {
        auto local_models = getLocalModels();
        if (!local_models.isEmpty()) {
            return local_models.first();
        }
    }
    
    // Default to first available
    return available.first();
}

QString ModelInterface::selectCostOptimalModel(const QString& prompt,
                                              double max_cost_usd)
{
    // Select model based on estimated cost
    auto models = getAvailableModels();
    
    for (const auto& model : models) {
        auto config = router->getModelConfig(model);
        // Estimate cost based on token count and backend
        // For now, just return first available
        return model;
    }
    
    return "";
}

QString ModelInterface::selectFastestModel(const QString& model_type)
{
    // Select model based on latency metrics
    auto models = getAvailableModels();
    
    if (models.isEmpty()) {
        return "";
    }
    
    // Return model with best latency stats
    QString fastest = models.first();
    double min_latency = getAverageLatency(fastest);
    
    for (const auto& model : models) {
        double latency = getAverageLatency(model);
        if (latency > 0 && latency < min_latency) {
            fastest = model;
            min_latency = latency;
        }
    }
    
    return fastest;
}

bool ModelInterface::loadConfig(const QString& config_file_path)
{
    if (!router) {
        router = std::make_shared<UniversalModelRouter>(this);
    }
    
    return router->loadConfigFromFile(config_file_path);
}

bool ModelInterface::saveConfig(const QString& config_file_path) const
{
    if (!router) {
        return false;
    }
    
    return router->saveConfigToFile(config_file_path);
}

void ModelInterface::setDefaultModel(const QString& model_name)
{
    if (modelExists(model_name)) {
        default_model = model_name;
    }
}

QString ModelInterface::getDefaultModel() const
{
    return default_model;
}

QJsonObject ModelInterface::getUsageStatistics() const
{
    QJsonObject stats;
    
    for (const auto& model_name : stats_map.keys()) {
        const auto& model_stats = stats_map[model_name];
        QJsonObject model_obj;
        model_obj["calls"] = model_stats.call_count;
        model_obj["successes"] = model_stats.success_count;
        model_obj["failures"] = model_stats.failure_count;
        model_obj["avg_latency_ms"] = model_stats.total_latency_ms / std::max(1, model_stats.call_count);
        model_obj["total_cost"] = model_stats.total_cost;
        model_obj["total_tokens"] = model_stats.total_tokens;
        
        stats[model_name] = model_obj;
    }
    
    return stats;
}

QJsonObject ModelInterface::getModelStats(const QString& model_name) const
{
    QJsonObject stats;
    
    if (stats_map.contains(model_name)) {
        const auto& model_stats = stats_map[model_name];
        stats["calls"] = model_stats.call_count;
        stats["successes"] = model_stats.success_count;
        stats["failures"] = model_stats.failure_count;
        stats["avg_latency_ms"] = model_stats.total_latency_ms / std::max(1, model_stats.call_count);
        stats["total_cost"] = model_stats.total_cost;
        stats["total_tokens"] = model_stats.total_tokens;
    }
    
    return stats;
}

double ModelInterface::getAverageLatency(const QString& model_name) const
{
    if (model_name.isEmpty()) {
        // Return average across all models
        double total_latency = 0.0;
        int total_calls = 0;
        
        for (const auto& model : stats_map.keys()) {
            const auto& stats = stats_map[model];
            total_latency += stats.total_latency_ms;
            total_calls += stats.call_count;
        }
        
        return total_calls > 0 ? total_latency / total_calls : 0.0;
    }
    
    if (stats_map.contains(model_name)) {
        const auto& stats = stats_map[model_name];
        return stats.call_count > 0 ? stats.total_latency_ms / stats.call_count : 0.0;
    }
    
    return 0.0;
}

int ModelInterface::getSuccessRate(const QString& model_name) const
{
    if (model_name.isEmpty()) {
        // Return success rate across all models
        int total_calls = 0;
        int total_successes = 0;
        
        for (const auto& model : stats_map.keys()) {
            const auto& stats = stats_map[model];
            total_calls += stats.call_count;
            total_successes += stats.success_count;
        }
        
        return total_calls > 0 ? (total_successes * 100) / total_calls : 0;
    }
    
    if (stats_map.contains(model_name)) {
        const auto& stats = stats_map[model_name];
        return stats.call_count > 0 ? (stats.success_count * 100) / stats.call_count : 0;
    }
    
    return 0;
}

double ModelInterface::getTotalCost() const
{
    double total = 0.0;
    
    for (const auto& model : stats_map.keys()) {
        total += stats_map[model].total_cost;
    }
    
    return total;
}

double ModelInterface::getCostByModel(const QString& model_name) const
{
    if (stats_map.contains(model_name)) {
        return stats_map[model_name].total_cost;
    }
    
    return 0.0;
}

QJsonObject ModelInterface::getCostBreakdown() const
{
    QJsonObject breakdown;
    
    for (const auto& model : stats_map.keys()) {
        breakdown[model] = stats_map[model].total_cost;
    }
    
    return breakdown;
}

void ModelInterface::setErrorCallback(std::function<void(const QString&)> callback)
{
    error_callback = callback;
}

void ModelInterface::setRetryPolicy(int max_retries, int retry_delay_ms)
{
    this->max_retries = max_retries;
    this->retry_delay_ms = retry_delay_ms;
}

int ModelInterface::estimateTokenCount(const QString& text) const
{
    // Simple estimation: ~4 characters per token on average
    return text.length() / 4;
}

QString ModelInterface::formatModelList() const
{
    QString list;
    
    auto models = getAvailableModels();
    for (const auto& model : models) {
        auto config = router->getModelConfig(model);
        QString backend_str = QString::number(static_cast<int>(config.backend));
        list += QString("%1 (%2)\n").arg(model, backend_str);
    }
    
    return list;
}

QJsonArray ModelInterface::getModelListAsJson() const
{
    QJsonArray array;
    
    auto models = getAvailableModels();
    for (const auto& model : models) {
        auto info = getModelInfo(model);
        if (!info.isEmpty()) {
            array.append(info);
        }
    }
    
    return array;
}

// ============ PRIVATE METHODS ============

GenerationResult ModelInterface::generateInternal(const QString& prompt,
                                                 const QString& model_name,
                                                 const GenerationOptions& options)
{
    GenerationResult result;
    result.model_name = model_name;
    
    auto config = getModelConfigOrThrow(model_name);
    QElapsedTimer timer;
    timer.start();
    
    if (isLocalModel(model_name)) {
        // Route through Ollama HTTP API on localhost:11434
        result.backend = "LOCAL";
        
        QString ollamaModel = config.model_id;
        if (ollamaModel.isEmpty()) {
            ollamaModel = model_name;
        }
        
        // Build Ollama /api/generate request
        QJsonObject requestBody;
        requestBody["model"] = ollamaModel;
        requestBody["prompt"] = prompt;
        requestBody["stream"] = false;
        
        // Apply generation options
        QJsonObject optionsObj;
        if (options.temperature > 0.0) {
            optionsObj["temperature"] = options.temperature;
        }
        if (options.max_tokens > 0) {
            optionsObj["num_predict"] = options.max_tokens;
        }
        if (options.top_p > 0.0) {
            optionsObj["top_p"] = options.top_p;
        }
        if (options.top_k > 0) {
            optionsObj["top_k"] = options.top_k;
        }
        if (options.repeat_penalty > 0.0) {
            optionsObj["repeat_penalty"] = options.repeat_penalty;
        }
        if (!optionsObj.isEmpty()) {
            requestBody["options"] = optionsObj;
        }
        
        // Stop sequences
        if (!options.stop_sequences.isEmpty()) {
            QJsonArray stopArr;
            for (const auto& s : options.stop_sequences) {
                stopArr.append(s);
            }
            requestBody["stop"] = stopArr;
        }
        
        // System prompt
        if (!options.system_prompt.isEmpty()) {
            requestBody["system"] = options.system_prompt;
        }
        
        QByteArray payload = QJsonDocument(requestBody).toJson(QJsonDocument::Compact);
        
        // Synchronous HTTP POST to Ollama
        QUrl url(QString("http://localhost:%1/api/generate").arg(config.port > 0 ? config.port : 11434));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setTransferTimeout(options.timeout_ms > 0 ? options.timeout_ms : 120000);
        
        QNetworkAccessManager nam;
        QEventLoop loop;
        QNetworkReply* reply = nam.post(request, payload);
        
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
            QJsonObject responseObj = responseDoc.object();
            
            result.content = responseObj.value("response").toString();
            result.success = !result.content.isEmpty();
            
            // Extract usage metadata if available
            if (responseObj.contains("total_duration")) {
                result.total_duration_ns = responseObj.value("total_duration").toDouble();
            }
            if (responseObj.contains("eval_count")) {
                result.token_count = responseObj.value("eval_count").toInt();
            }
            if (responseObj.contains("prompt_eval_count")) {
                result.prompt_tokens = responseObj.value("prompt_eval_count").toInt();
            }
            
            qDebug() << "[ModelInterface] Ollama response:" << result.content.left(200);
        } else {
            result.success = false;
            result.error = QString("Ollama HTTP error: %1 (%2)")
                .arg(reply->errorString())
                .arg(static_cast<int>(reply->error()));
            qWarning() << "[ModelInterface]" << result.error;
            
            if (error_callback) {
                error_callback(result.error);
            }
        }
        
        reply->deleteLater();
    } else {
        // Use cloud client
        result.content = cloud_client->generate(prompt, config);
        result.backend = "CLOUD";
        result.success = !result.content.isEmpty();
    }
    
    // Record latency
    double latency_ms = timer.elapsed();
    
    // Update statistics
    if (stats_map.contains(model_name)) {
        stats_map[model_name].call_count++;
        stats_map[model_name].total_latency_ms += latency_ms;
        if (result.success) {
            stats_map[model_name].success_count++;
            stats_map[model_name].total_tokens += result.token_count;
        } else {
            stats_map[model_name].failure_count++;
        }
    } else {
        ModelStats new_stats;
        new_stats.call_count = 1;
        new_stats.success_count = result.success ? 1 : 0;
        new_stats.failure_count = result.success ? 0 : 1;
        new_stats.total_latency_ms = latency_ms;
        new_stats.total_tokens = result.token_count;
        stats_map[model_name] = new_stats;
    }
    
    return result;
}

void ModelInterface::generateStreamInternal(const QString& prompt,
                                           const QString& model_name,
                                           std::function<void(const QString&)> on_chunk,
                                           std::function<void(const QString&)> on_error,
                                           const GenerationOptions& options)
{
    auto config = getModelConfigOrThrow(model_name);
    
    if (isLocalModel(model_name)) {
        // Stream from Ollama HTTP API on localhost:11434
        QString ollamaModel = config.model_id;
        if (ollamaModel.isEmpty()) {
            ollamaModel = model_name;
        }
        
        // Build Ollama /api/generate request with streaming enabled
        QJsonObject requestBody;
        requestBody["model"] = ollamaModel;
        requestBody["prompt"] = prompt;
        requestBody["stream"] = true;
        
        QJsonObject optionsObj;
        if (options.temperature > 0.0) optionsObj["temperature"] = options.temperature;
        if (options.max_tokens > 0) optionsObj["num_predict"] = options.max_tokens;
        if (options.top_p > 0.0) optionsObj["top_p"] = options.top_p;
        if (!optionsObj.isEmpty()) requestBody["options"] = optionsObj;
        if (!options.system_prompt.isEmpty()) requestBody["system"] = options.system_prompt;
        
        QByteArray payload = QJsonDocument(requestBody).toJson(QJsonDocument::Compact);
        
        QUrl url(QString("http://localhost:%1/api/generate").arg(config.port > 0 ? config.port : 11434));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setTransferTimeout(options.timeout_ms > 0 ? options.timeout_ms : 300000);
        
        QNetworkAccessManager* nam = new QNetworkAccessManager(this);
        QNetworkReply* reply = nam->post(request, payload);
        
        // Process streaming NDJSON chunks as they arrive
        QObject::connect(reply, &QNetworkReply::readyRead, this, [reply, on_chunk]() {
            while (reply->canReadLine()) {
                QByteArray line = reply->readLine().trimmed();
                if (line.isEmpty()) continue;
                
                QJsonDocument chunkDoc = QJsonDocument::fromJson(line);
                if (chunkDoc.isNull()) continue;
                
                QJsonObject chunkObj = chunkDoc.object();
                QString response = chunkObj.value("response").toString();
                
                if (!response.isEmpty() && on_chunk) {
                    on_chunk(response);
                }
                
                // Check if generation is done
                if (chunkObj.value("done").toBool(false)) {
                    return;
                }
            }
        });
        
        QObject::connect(reply, &QNetworkReply::finished, this, [reply, on_error, nam]() {
            if (reply->error() != QNetworkReply::NoError) {
                if (on_error) {
                    on_error(QString("Ollama stream error: %1").arg(reply->errorString()));
                }
            }
            reply->deleteLater();
            nam->deleteLater();
        });
        
    } else {
        // Use cloud client streaming
        cloud_client->generateStream(prompt, config, on_chunk, on_error);
    }
}

bool ModelInterface::isLocalModel(const QString& model_name) const
{
    if (!router) return false;
    
    auto backend = router->getModelBackend(model_name);
    return backend == ModelBackend::LOCAL_GGUF || backend == ModelBackend::OLLAMA_LOCAL;
}

bool ModelInterface::isCloudModel(const QString& model_name) const
{
    return !isLocalModel(model_name);
}

ModelConfig ModelInterface::getModelConfigOrThrow(const QString& model_name) const
{
    if (!router) {
        throw std::runtime_error("Router not initialized");
    }
    
    auto config = router->getModelConfig(model_name);
    if (config.model_id.isEmpty()) {
        throw std::runtime_error(QString("Model not found: %1").arg(model_name).toStdString());
    }
    
    return config;
}

void ModelInterface::onRouterInitialized()
{
    initialized_flag = true;
    emit initialized();
}

void ModelInterface::onModelRegistered(const QString& model_name)
{
    emit modelListUpdated(getAvailableModels());
}
