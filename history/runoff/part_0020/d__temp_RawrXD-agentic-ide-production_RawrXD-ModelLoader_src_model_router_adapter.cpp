#include "model_router_adapter.h"
#include "universal_model_router.h"
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <QtGlobal>
#include <cmath>

/**
 * @class GenerationThread
 * @brief Worker thread for async generation operations
 */
class ModelRouterAdapter::GenerationThread : public QThread {
    Q_OBJECT
public:
    GenerationThread(ModelInterface* router, const QString& prompt, 
                     const QString& model, int max_tokens)
        : QThread(nullptr), m_router(router), m_prompt(prompt),
          m_model(model), m_max_tokens(max_tokens) {}

    void run() override {
        if (!m_router) return;
        
        QElapsedTimer timer;
        timer.start();
        
        try {
            GenerationResult result = 
                m_router->generate(m_prompt, m_model.isEmpty() ? "default" : m_model);
            
            m_result = result;
            m_latency = timer.elapsed();
            m_success = result.success;
            m_error = result.error;
        } catch (const std::exception& e) {
            m_success = false;
            m_error = QString::fromStdString(e.what());
            m_latency = timer.elapsed();
        }
    }

    GenerationResult m_result;
    QString m_error;
    bool m_success = false;
    qint64 m_latency = 0;

private:
    ModelInterface* m_router;
    QString m_prompt;
    QString m_model;
    int m_max_tokens;
};

// ============================================================================
// ModelRouterAdapter Implementation
// ============================================================================

ModelRouterAdapter::ModelRouterAdapter(QObject *parent)
    : QObject(parent), m_router(nullptr), m_initialized(false)
{
    qDebug() << "[ModelRouterAdapter] Constructed";
}

ModelRouterAdapter::~ModelRouterAdapter()
{
    if (m_generation_thread) {
        m_generation_thread->quit();
        m_generation_thread->wait();
        delete m_generation_thread;
    }
    emit shutting_down();
    qDebug() << "[ModelRouterAdapter] Destroyed";
}

bool ModelRouterAdapter::initialize(const QString& config_file_path)
{
    qDebug() << "[ModelRouterAdapter::initialize] Starting initialization with config:"
             << config_file_path;
    
    try {
        // Create router instance
        m_router = std::make_unique<ModelInterface>();
        
        // Load configuration
        if (!m_router->loadConfig(config_file_path)) {
            m_last_error = "Failed to load configuration file: " + config_file_path;
            qWarning() << "[ModelRouterAdapter::initialize]" << m_last_error;
            m_router.reset();
            return false;
        }
        
        // Load API keys from environment
        if (!loadApiKeys()) {
            m_last_error = "Warning: Some API keys not found in environment. Cloud providers may be unavailable.";
            qWarning() << "[ModelRouterAdapter::initialize]" << m_last_error;
            // Don't fail - local models should still work
        }
        
        // Get available models
        auto models = m_router->getAvailableModels();
        if (models.isEmpty()) {
            m_last_error = "No models available after loading configuration";
            qWarning() << "[ModelRouterAdapter::initialize]" << m_last_error;
            m_router.reset();
            return false;
        }
        
        // Set default model (prefer local GGUF if available)
        if (models.contains("quantumide-q4km")) {
            m_active_model = "quantumide-q4km";
        } else if (models.contains("ollama-local")) {
            m_active_model = "ollama-local";
        } else {
            m_active_model = models.first();
        }
        
        m_initialized = true;
        
        qDebug() << "[ModelRouterAdapter::initialize] Success!"
                 << "Available models:" << models.size()
                 << "Default model:" << m_active_model;
        
        emit statusChanged("Initialized with " + QString::number(models.size()) + " models");
        emit modelListUpdated(models);
        emit initialized();
        
        return true;
        
    } catch (const std::exception& e) {
        m_last_error = QString("Initialization exception: %1").arg(QString::fromStdString(e.what()));
        qCritical() << "[ModelRouterAdapter::initialize]" << m_last_error;
        m_router.reset();
        return false;
    }
}

bool ModelRouterAdapter::loadApiKeys()
{
    qDebug() << "[ModelRouterAdapter::loadApiKeys] Loading API keys from environment";
    
    // Environment variables checked by cloud_api_client internally
    // This is called to verify at least some keys are available
    // Keys should be: OPENAI_API_KEY, ANTHROPIC_API_KEY, GOOGLE_API_KEY, etc.
    
    bool found_any = false;
    QStringList expected_keys = {
        "OPENAI_API_KEY",
        "ANTHROPIC_API_KEY", 
        "GOOGLE_API_KEY",
        "MOONSHOT_API_KEY",
        "AZURE_OPENAI_API_KEY",
        "AWS_ACCESS_KEY_ID"
    };
    
    for (const auto& key : expected_keys) {
        QByteArray keyBytes = key.toUtf8();
        if (!qEnvironmentVariableIsEmpty(keyBytes.constData())) {
            qDebug() << "[ModelRouterAdapter::loadApiKeys] Found:" << key;
            found_any = true;
        }
    }
    
    if (!found_any) {
        qWarning() << "[ModelRouterAdapter::loadApiKeys] No cloud API keys found in environment";
        return false;
    }
    
    qDebug() << "[ModelRouterAdapter::loadApiKeys] Loaded successfully";
    return true;
}

QStringList ModelRouterAdapter::getAvailableModels() const
{
    if (!m_router) return {};
    try {
        return m_router->getAvailableModels();
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::getAvailableModels] Exception:"
                   << QString::fromStdString(e.what());
        return {};
    }
}

QString ModelRouterAdapter::selectBestModel(const QString& task_type, 
                                             const QString& language, 
                                             bool prefer_local)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        return QString();
    }
    
    try {
        QString selected = m_router->selectBestModel(task_type, language);
        
        // If prefer_local, check if we should switch to local model
        if (prefer_local) {
            auto models = m_router->getAvailableModels();
            if (models.contains("quantumide-q4km")) {
                selected = "quantumide-q4km";
            } else if (models.contains("ollama-local")) {
                selected = "ollama-local";
            }
        }
        
        qDebug() << "[ModelRouterAdapter::selectBestModel]"
                 << "task:" << task_type << "lang:" << language
                 << "result:" << selected;
        
        return selected;
    } catch (const std::exception& e) {
        m_last_error = QString::fromStdString(e.what());
        qWarning() << "[ModelRouterAdapter::selectBestModel] Exception:" << m_last_error;
        return QString();
    }
}

QString ModelRouterAdapter::selectCostOptimalModel(const QString& prompt, double max_cost_usd)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        return QString();
    }
    
    try {
        QString selected = m_router->selectCostOptimalModel(prompt, max_cost_usd);
        
        qDebug() << "[ModelRouterAdapter::selectCostOptimalModel]"
                 << "budget: $" << max_cost_usd << "result:" << selected;
        
        return selected;
    } catch (const std::exception& e) {
        m_last_error = QString::fromStdString(e.what());
        qWarning() << "[ModelRouterAdapter::selectCostOptimalModel] Exception:" << m_last_error;
        return QString();
    }
}

QString ModelRouterAdapter::selectFastestModel()
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        return QString();
    }
    
    try {
        QString selected = m_router->selectFastestModel();
        qDebug() << "[ModelRouterAdapter::selectFastestModel] Selected:" << selected;
        return selected;
    } catch (const std::exception& e) {
        m_last_error = QString::fromStdString(e.what());
        qWarning() << "[ModelRouterAdapter::selectFastestModel] Exception:" << m_last_error;
        return QString();
    }
}

void ModelRouterAdapter::setDefaultModel(const QString& model_name)
{
    if (model_name.isEmpty()) return;
    
    auto models = getAvailableModels();
    if (!models.contains(model_name)) {
        qWarning() << "[ModelRouterAdapter::setDefaultModel] Model not found:" << model_name;
        return;
    }
    
    m_active_model = model_name;
    qDebug() << "[ModelRouterAdapter::setDefaultModel] Changed to:" << model_name;
    emit modelChanged(model_name);
}

QString ModelRouterAdapter::generate(const QString& prompt, const QString& model_name, int max_tokens)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        emit generationError(m_last_error);
        return QString();
    }
    
    if (prompt.isEmpty()) {
        m_last_error = "Prompt cannot be empty";
        emit generationError(m_last_error);
        return QString();
    }
    
    QString model = model_name.isEmpty() ? m_active_model : model_name;
    
    try {
        emit generationStarted(model);
        emit statusChanged(QString("Generating with %1...").arg(model));
        
        QElapsedTimer timer;
        timer.start();
        
        auto result = m_router->generate(prompt, model);
        
        qint64 elapsed = timer.elapsed();
        
        if (!result.success) {
            m_last_error = result.error;
            
            // Try fallback if enabled
            if (m_auto_fallback && model != "quantumide-q4km") {
                qDebug() << "[ModelRouterAdapter::generate] Fallback triggered to quantumide-q4km";
                emit statusChanged("Falling back to local model...");
                
                auto fallback_result = m_router->generate(prompt, "quantumide-q4km");
                if (fallback_result.success) {
                    result = fallback_result;
                }
            }
        }
        
        if (result.success) {
            int tokens = result.metadata.value("tokens_used").toInt();
            
            qDebug() << "[ModelRouterAdapter::generate] Success"
                     << "model:" << result.model_name
                     << "latency:" << elapsed << "ms"
                     << "tokens:" << tokens;
            
            emit generationComplete(result.content, tokens, elapsed);
            emit statusChanged(QString("Generated %1 tokens in %2ms").arg(tokens).arg(elapsed));
            
            // Update total cost
            double new_cost = getTotalCost();
            emit costUpdated(new_cost);
            
            return result.content;
        } else {
            emit generationError(m_last_error);
            emit statusChanged("Generation failed: " + m_last_error);
            return QString();
        }
        
    } catch (const std::exception& e) {
        m_last_error = QString::fromStdString(e.what());
        qCritical() << "[ModelRouterAdapter::generate] Exception:" << m_last_error;
        emit generationError(m_last_error);
        return QString();
    }
}

void ModelRouterAdapter::generateAsync(const QString& prompt, const QString& model_name, int max_tokens)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        emit generationError(m_last_error);
        return;
    }
    
    if (m_generation_thread && m_generation_thread->isRunning()) {
        m_last_error = "Generation already in progress";
        emit generationError(m_last_error);
        return;
    }
    
    QString model = model_name.isEmpty() ? m_active_model : model_name;
    
    // Create and start generation thread
    if (m_generation_thread) {
        delete m_generation_thread;
    }
    
    m_generation_thread = new GenerationThread(m_router.get(), prompt, model, max_tokens);
    
    connect(m_generation_thread, &QThread::finished, this, &ModelRouterAdapter::onGenerationThreadFinished);
    
    emit generationStarted(model);
    emit statusChanged(QString("Generating with %1 (async)...").arg(model));
    
    m_generation_thread->start();
    
    qDebug() << "[ModelRouterAdapter::generateAsync] Started async generation with:" << model;
}

void ModelRouterAdapter::generateStream(const QString& prompt, const QString& model_name, int max_tokens)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        emit generationError(m_last_error);
        return;
    }
    
    QString model = model_name.isEmpty() ? m_active_model : model_name;
    
    try {
        emit generationStarted(model);
        emit statusChanged(QString("Streaming from %1...").arg(model));
        
        QElapsedTimer timer;
        timer.start();
        
        // Note: ModelInterface::generateStream would emit via callback
        // For now, we use regular generation as fallback
        auto result = m_router->generate(prompt, model);
        
        if (result.success) {
            // Emit as single chunk (would be per-chunk in real streaming)
            emit generationChunk(result.content);
            
            int tokens = result.metadata.value("tokens_used").toInt();
            emit generationComplete(result.content, tokens, timer.elapsed());
            
            qDebug() << "[ModelRouterAdapter::generateStream] Completed"
                     << "latency:" << timer.elapsed() << "ms";
        } else {
            emit generationError(result.error);
        }
    } catch (const std::exception& e) {
        m_last_error = QString::fromStdString(e.what());
        qCritical() << "[ModelRouterAdapter::generateStream] Exception:" << m_last_error;
        emit generationError(m_last_error);
    }
}

double ModelRouterAdapter::getAverageLatency(const QString& model_name) const
{
    if (!m_router) return 0.0;
    
    try {
        QString model = model_name.isEmpty() ? m_active_model : model_name;
        return m_router->getAverageLatency(model);
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::getAverageLatency] Exception:"
                   << QString::fromStdString(e.what());
        return 0.0;
    }
}

int ModelRouterAdapter::getSuccessRate(const QString& model_name) const
{
    if (!m_router) return 0;
    
    try {
        QString model = model_name.isEmpty() ? m_active_model : model_name;
        return m_router->getSuccessRate(model);
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::getSuccessRate] Exception:"
                   << QString::fromStdString(e.what());
        return 0;
    }
}

double ModelRouterAdapter::getTotalCost() const
{
    if (!m_router) return 0.0;
    
    try {
        return m_router->getTotalCost();
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::getTotalCost] Exception:"
                   << QString::fromStdString(e.what());
        return 0.0;
    }
}

QMap<QString, double> ModelRouterAdapter::getCostBreakdown() const
{
    QMap<QString, double> breakdown;
    
    if (!m_router) return breakdown;
    
    try {
        auto stats = m_router->getUsageStatistics();
        QJsonArray models = stats.value("models").toArray();
        
        for (const auto& model_val : models) {
            QJsonObject model_obj = model_val.toObject();
            QString name = model_obj.value("name").toString();
            double cost = model_obj.value("total_cost").toDouble();
            breakdown[name] = cost;
        }
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::getCostBreakdown] Exception:"
                   << QString::fromStdString(e.what());
    }
    
    return breakdown;
}

QJsonObject ModelRouterAdapter::getStatistics() const
{
    if (!m_router) return QJsonObject();
    
    try {
        return m_router->getUsageStatistics();
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::getStatistics] Exception:"
                   << QString::fromStdString(e.what());
        return QJsonObject();
    }
}

bool ModelRouterAdapter::exportStatisticsToCsv(const QString& file_path) const
{
    try {
        auto stats = getStatistics();
        
        QFile file(file_path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[ModelRouterAdapter::exportStatisticsToCsv] Cannot open file:" << file_path;
            return false;
        }
        
        QTextStream stream(&file);
        
        // Write header
        stream << "Model,Total_Cost,Avg_Latency_ms,Success_Rate,Request_Count\n";
        
        // Write data
        QJsonArray models = stats.value("models").toArray();
        for (const auto& model_val : models) {
            QJsonObject model_obj = model_val.toObject();
            
            QString name = model_obj.value("name").toString();
            double cost = model_obj.value("total_cost").toDouble();
            double latency = model_obj.value("avg_latency_ms").toDouble();
            int success_rate = model_obj.value("success_rate").toInt();
            int count = model_obj.value("request_count").toInt();
            
            stream << QString("%1,%2,%3,%4,%5\n").arg(name).arg(cost).arg(latency).arg(success_rate).arg(count);
        }
        
        file.close();
        qDebug() << "[ModelRouterAdapter::exportStatisticsToCsv] Exported to:" << file_path;
        return true;
        
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::exportStatisticsToCsv] Exception:"
                   << QString::fromStdString(e.what());
        return false;
    }
}

bool ModelRouterAdapter::registerModel(const QString& name, const QJsonObject& config)
{
    if (!m_router) return false;
    
    try {
        ModelConfig model_config;
        model_config.model_id = config.value("model_id").toString();
        model_config.api_key = config.value("api_key").toString();
        model_config.endpoint = config.value("endpoint").toString();
        
        m_router->registerModel(name, model_config);
        
        qDebug() << "[ModelRouterAdapter::registerModel] Registered:" << name;
        emit modelListUpdated(getAvailableModels());
        
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::registerModel] Exception:"
                   << QString::fromStdString(e.what());
        return false;
    }
}

QJsonObject ModelRouterAdapter::getConfiguration() const
{
    if (!m_router) return QJsonObject();
    
    try {
        // Reconstruct configuration from available models
        QJsonObject config;
        QJsonArray models_array;
        
        auto models = getAvailableModels();
        for (const auto& model_name : models) {
            QJsonObject model_obj;
            model_obj["name"] = model_name;
            models_array.append(model_obj);
        }
        
        config["models"] = models_array;
        return config;
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::getConfiguration] Exception:"
                   << QString::fromStdString(e.what());
        return QJsonObject();
    }
}

bool ModelRouterAdapter::saveConfiguration(const QString& file_path)
{
    try {
        auto config = getConfiguration();
        
        QJsonDocument doc(config);
        QFile file(file_path);
        
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[ModelRouterAdapter::saveConfiguration] Cannot open file:" << file_path;
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        
        qDebug() << "[ModelRouterAdapter::saveConfiguration] Saved to:" << file_path;
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[ModelRouterAdapter::saveConfiguration] Exception:"
                   << QString::fromStdString(e.what());
        return false;
    }
}

void ModelRouterAdapter::setRetryPolicy(int max_retries, int retry_delay_ms)
{
    qDebug() << "[ModelRouterAdapter::setRetryPolicy]"
             << "max_retries:" << max_retries << "delay:" << retry_delay_ms << "ms";
    // Implementation would set policy in router if supported
}

void ModelRouterAdapter::setCostAlertThreshold(double threshold_usd)
{
    m_cost_alert_threshold = threshold_usd;
    qDebug() << "[ModelRouterAdapter::setCostAlertThreshold]" << "$" << threshold_usd;
}

void ModelRouterAdapter::setLatencyThreshold(int threshold_ms)
{
    m_latency_threshold_ms = threshold_ms;
    qDebug() << "[ModelRouterAdapter::setLatencyThreshold]" << threshold_ms << "ms";
}

// Private slot implementation
void ModelRouterAdapter::onGenerationThreadFinished()
{
    if (!m_generation_thread) return;
    
    if (m_generation_thread->m_success) {
        int tokens = m_generation_thread->m_result.metadata.contains("tokens_used") 
                    ? m_generation_thread->m_result.metadata.value("tokens_used").toInt() : 0;
        
        qDebug() << "[ModelRouterAdapter::onGenerationThreadFinished] Success"
                 << "latency:" << m_generation_thread->m_latency << "ms";
        
        emit generationComplete(m_generation_thread->m_result.content, tokens, 
                               m_generation_thread->m_latency);
        emit statusChanged("Generation complete");
    } else {
        m_last_error = m_generation_thread->m_error;
        qWarning() << "[ModelRouterAdapter::onGenerationThreadFinished] Error:" << m_last_error;
        emit generationError(m_last_error);
        emit statusChanged("Generation error: " + m_last_error);
    }
}

void ModelRouterAdapter::onStreamChunkReceived(const QString& chunk)
{
    emit generationChunk(chunk);
}

#include "model_router_adapter.moc"
