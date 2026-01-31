#include "model_router_adapter.h"
#include "universal_model_router.h"


#include <cmath>

/**
 * @class GenerationThread
 * @brief Worker thread for async generation operations
 */
class ModelRouterAdapter::GenerationThread : public std::thread {

public:
    GenerationThread(ModelInterface* router, const std::string& prompt, 
                     const std::string& model, int max_tokens)
        : std::thread(nullptr), m_router(router), m_prompt(prompt),
          m_model(model), m_max_tokens(max_tokens) {}

    void run() override {
        if (!m_router) return;
        
        std::chrono::steady_clock timer;
        timer.start();
        
        try {
            GenerationResult result = 
                m_router->generate(m_prompt, m_model.empty() ? "default" : m_model);
            
            m_result = result;
            m_latency = timer.elapsed();
            m_success = result.success;
            m_error = result.error;
        } catch (const std::exception& e) {
            m_success = false;
            m_error = std::string::fromStdString(e.what());
            m_latency = timer.elapsed();
        }
    }

    GenerationResult m_result;
    std::string m_error;
    bool m_success = false;
    int64_t m_latency = 0;

private:
    ModelInterface* m_router;
    std::string m_prompt;
    std::string m_model;
    int m_max_tokens;
};

// ============================================================================
// ModelRouterAdapter Implementation
// ============================================================================

ModelRouterAdapter::ModelRouterAdapter(void *parent)
    : void(parent), m_router(nullptr), m_initialized(false)
{
}

ModelRouterAdapter::~ModelRouterAdapter()
{
    if (m_generation_thread) {
        m_generation_thread->quit();
        m_generation_thread->wait();
        delete m_generation_thread;
    }
    shutting_down();
}

bool ModelRouterAdapter::initialize(const std::string& config_file_path)
{
             << config_file_path;
    
    try {
        // Create router instance
        m_router = std::make_unique<ModelInterface>();
        
        // Load configuration
        if (!m_router->loadConfig(config_file_path)) {
            m_last_error = "Failed to load configuration file: " + config_file_path;
            m_router.reset();
            return false;
        }
        
        // Load API keys from environment
        if (!loadApiKeys()) {
            m_last_error = "Warning: Some API keys not found in environment. Cloud providers may be unavailable.";
            // Don't fail - local models should still work
        }
        
        // Get available models
        auto models = m_router->getAvailableModels();
        if (models.empty()) {
            m_last_error = "No models available after loading configuration";
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
        
                 << "Available models:" << models.size()
                 << "Default model:" << m_active_model;
        
        statusChanged("Initialized with " + std::string::number(models.size()) + " models");
        modelListUpdated(models);
        initialized();
        
        return true;
        
    } catch (const std::exception& e) {
        m_last_error = std::string("Initialization exception: %1")));
        m_router.reset();
        return false;
    }
}

bool ModelRouterAdapter::loadApiKeys()
{
    
    // Environment variables checked by cloud_api_client internally
    // This is called to verify at least some keys are available
    // Keys should be: OPENAI_API_KEY, ANTHROPIC_API_KEY, GOOGLE_API_KEY, etc.
    
    bool found_any = false;
    std::vector<std::string> expected_keys = {
        "OPENAI_API_KEY",
        "ANTHROPIC_API_KEY", 
        "GOOGLE_API_KEY",
        "MOONSHOT_API_KEY",
        "AZURE_OPENAI_API_KEY",
        "AWS_ACCESS_KEY_ID"
    };
    
    for (const auto& key : expected_keys) {
        std::vector<uint8_t> keyBytes = key.toUtf8();
        if (!qEnvironmentVariableIsEmpty(keyBytes.constData())) {
            found_any = true;
        }
    }
    
    if (!found_any) {
        return false;
    }
    
    return true;
}

std::vector<std::string> ModelRouterAdapter::getAvailableModels() const
{
    if (!m_router) return {};
    try {
        return m_router->getAvailableModels();
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return {};
    }
}

std::string ModelRouterAdapter::selectBestModel(const std::string& task_type, 
                                             const std::string& language, 
                                             bool prefer_local)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        return std::string();
    }
    
    try {
        std::string selected = m_router->selectBestModel(task_type, language);
        
        // If prefer_local, check if we should switch to local model
        if (prefer_local) {
            auto models = m_router->getAvailableModels();
            if (models.contains("quantumide-q4km")) {
                selected = "quantumide-q4km";
            } else if (models.contains("ollama-local")) {
                selected = "ollama-local";
            }
        }
        
                 << "task:" << task_type << "lang:" << language
                 << "result:" << selected;
        
        return selected;
    } catch (const std::exception& e) {
        m_last_error = std::string::fromStdString(e.what());
        return std::string();
    }
}

std::string ModelRouterAdapter::selectCostOptimalModel(const std::string& prompt, double max_cost_usd)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        return std::string();
    }
    
    try {
        std::string selected = m_router->selectCostOptimalModel(prompt, max_cost_usd);
        
                 << "budget: $" << max_cost_usd << "result:" << selected;
        
        return selected;
    } catch (const std::exception& e) {
        m_last_error = std::string::fromStdString(e.what());
        return std::string();
    }
}

std::string ModelRouterAdapter::selectFastestModel()
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        return std::string();
    }
    
    try {
        std::string selected = m_router->selectFastestModel();
        return selected;
    } catch (const std::exception& e) {
        m_last_error = std::string::fromStdString(e.what());
        return std::string();
    }
}

void ModelRouterAdapter::setDefaultModel(const std::string& model_name)
{
    if (model_name.empty()) return;
    
    auto models = getAvailableModels();
    if (!models.contains(model_name)) {
        return;
    }
    
    m_active_model = model_name;
    modelChanged(model_name);
}

std::string ModelRouterAdapter::generate(const std::string& prompt, const std::string& model_name, int max_tokens)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        generationError(m_last_error);
        return std::string();
    }
    
    if (prompt.empty()) {
        m_last_error = "Prompt cannot be empty";
        generationError(m_last_error);
        return std::string();
    }
    
    std::string model = model_name.empty() ? m_active_model : model_name;
    
    try {
        generationStarted(model);
        statusChanged(std::string("Generating with %1..."));
        
        std::chrono::steady_clock timer;
        timer.start();
        
        auto result = m_router->generate(prompt, model);
        
        int64_t elapsed = timer.elapsed();
        
        if (!result.success) {
            m_last_error = result.error;
            
            // Try fallback if enabled
            if (m_auto_fallback && model != "quantumide-q4km") {
                statusChanged("Falling back to local model...");
                
                auto fallback_result = m_router->generate(prompt, "quantumide-q4km");
                if (fallback_result.success) {
                    result = fallback_result;
                }
            }
        }
        
        if (result.success) {
            int tokens = result.metadata.value("tokens_used").toInt();
            
                     << "model:" << result.model_name
                     << "latency:" << elapsed << "ms"
                     << "tokens:" << tokens;
            
            generationComplete(result.content, tokens, elapsed);
            statusChanged(std::string("Generated %1 tokens in %2ms"));
            
            // Update total cost
            double new_cost = getTotalCost();
            costUpdated(new_cost);
            
            return result.content;
        } else {
            generationError(m_last_error);
            statusChanged("Generation failed: " + m_last_error);
            return std::string();
        }
        
    } catch (const std::exception& e) {
        m_last_error = std::string::fromStdString(e.what());
        generationError(m_last_error);
        return std::string();
    }
}

void ModelRouterAdapter::generateAsync(const std::string& prompt, const std::string& model_name, int max_tokens)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        generationError(m_last_error);
        return;
    }
    
    if (m_generation_thread && m_generation_thread->isRunning()) {
        m_last_error = "Generation already in progress";
        generationError(m_last_error);
        return;
    }
    
    std::string model = model_name.empty() ? m_active_model : model_name;
    
    // Create and start generation thread
    if (m_generation_thread) {
        delete m_generation_thread;
    }
    
    m_generation_thread = new GenerationThread(m_router.get(), prompt, model, max_tokens);
// Qt connect removed
    generationStarted(model);
    statusChanged(std::string("Generating with %1 (async)..."));
    
    m_generation_thread->start();
    
}

void ModelRouterAdapter::generateStream(const std::string& prompt, const std::string& model_name, int max_tokens)
{
    if (!m_router) {
        m_last_error = "Router not initialized";
        generationError(m_last_error);
        return;
    }
    
    std::string model = model_name.empty() ? m_active_model : model_name;
    
    try {
        generationStarted(model);
        statusChanged(std::string("Streaming from %1..."));
        
        std::chrono::steady_clock timer;
        timer.start();
        
        // Note: ModelInterface::generateStream would via callback
        // For now, we use regular generation as fallback
        auto result = m_router->generate(prompt, model);
        
        if (result.success) {
            // as single chunk (would be per-chunk in real streaming)
            generationChunk(result.content);
            
            int tokens = result.metadata.value("tokens_used").toInt();
            generationComplete(result.content, tokens, timer.elapsed());
            
                     << "latency:" << timer.elapsed() << "ms";
        } else {
            generationError(result.error);
        }
    } catch (const std::exception& e) {
        m_last_error = std::string::fromStdString(e.what());
        generationError(m_last_error);
    }
}

double ModelRouterAdapter::getAverageLatency(const std::string& model_name) const
{
    if (!m_router) return 0.0;
    
    try {
        std::string model = model_name.empty() ? m_active_model : model_name;
        return m_router->getAverageLatency(model);
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return 0.0;
    }
}

int ModelRouterAdapter::getSuccessRate(const std::string& model_name) const
{
    if (!m_router) return 0;
    
    try {
        std::string model = model_name.empty() ? m_active_model : model_name;
        return m_router->getSuccessRate(model);
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return 0;
    }
}

double ModelRouterAdapter::getTotalCost() const
{
    if (!m_router) return 0.0;
    
    try {
        return m_router->getTotalCost();
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return 0.0;
    }
}

std::map<std::string, double> ModelRouterAdapter::getCostBreakdown() const
{
    std::map<std::string, double> breakdown;
    
    if (!m_router) return breakdown;
    
    try {
        auto stats = m_router->getUsageStatistics();
        void* models = stats.value("models").toArray();
        
        for (const auto& model_val : models) {
            void* model_obj = model_val.toObject();
            std::string name = model_obj.value("name").toString();
            double cost = model_obj.value("total_cost").toDouble();
            breakdown[name] = cost;
        }
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
    }
    
    return breakdown;
}

void* ModelRouterAdapter::getStatistics() const
{
    if (!m_router) return void*();
    
    try {
        return m_router->getUsageStatistics();
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return void*();
    }
}

bool ModelRouterAdapter::exportStatisticsToCsv(const std::string& file_path) const
{
    try {
        auto stats = getStatistics();
        
        std::fstream file(file_path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        
        QTextStream stream(&file);
        
        // Write header
        stream << "Model,Total_Cost,Avg_Latency_ms,Success_Rate,Request_Count\n";
        
        // Write data
        void* models = stats.value("models").toArray();
        for (const auto& model_val : models) {
            void* model_obj = model_val.toObject();
            
            std::string name = model_obj.value("name").toString();
            double cost = model_obj.value("total_cost").toDouble();
            double latency = model_obj.value("avg_latency_ms").toDouble();
            int success_rate = model_obj.value("success_rate").toInt();
            int count = model_obj.value("request_count").toInt();
            
            stream << std::string("%1,%2,%3,%4,%5\n");
        }
        
        file.close();
        return true;
        
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return false;
    }
}

bool ModelRouterAdapter::registerModel(const std::string& name, const void*& config)
{
    if (!m_router) return false;
    
    try {
        ModelConfig model_config;
        model_config.model_id = config.value("model_id").toString();
        model_config.api_key = config.value("api_key").toString();
        model_config.endpoint = config.value("endpoint").toString();
        
        m_router->registerModel(name, model_config);
        
        modelListUpdated(getAvailableModels());
        
        return true;
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return false;
    }
}

void* ModelRouterAdapter::getConfiguration() const
{
    if (!m_router) return void*();
    
    try {
        // Reconstruct configuration from available models
        void* config;
        void* models_array;
        
        auto models = getAvailableModels();
        for (const auto& model_name : models) {
            void* model_obj;
            model_obj["name"] = model_name;
            models_array.append(model_obj);
        }
        
        config["models"] = models_array;
        return config;
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return void*();
    }
}

bool ModelRouterAdapter::saveConfiguration(const std::string& file_path)
{
    try {
        auto config = getConfiguration();
        
        void* doc(config);
        std::fstream file(file_path);
        
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        
        file.write(doc.toJson());
        file.close();
        
        return true;
    } catch (const std::exception& e) {
                   << std::string::fromStdString(e.what());
        return false;
    }
}

void ModelRouterAdapter::setRetryPolicy(int max_retries, int retry_delay_ms)
{
             << "max_retries:" << max_retries << "delay:" << retry_delay_ms << "ms";
    // Implementation would set policy in router if supported
}

void ModelRouterAdapter::setCostAlertThreshold(double threshold_usd)
{
    m_cost_alert_threshold = threshold_usd;
}

void ModelRouterAdapter::setLatencyThreshold(int threshold_ms)
{
    m_latency_threshold_ms = threshold_ms;
}

// Private slot implementation
void ModelRouterAdapter::onGenerationThreadFinished()
{
    if (!m_generation_thread) return;
    
    if (m_generation_thread->m_success) {
        int tokens = m_generation_thread->m_result.metadata.contains("tokens_used") 
                    ? m_generation_thread->m_result.metadata.value("tokens_used").toInt() : 0;
        
                 << "latency:" << m_generation_thread->m_latency << "ms";
        
        generationComplete(m_generation_thread->m_result.content, tokens, 
                               m_generation_thread->m_latency);
        statusChanged("Generation complete");
    } else {
        m_last_error = m_generation_thread->m_error;
        generationError(m_last_error);
        statusChanged("Generation error: " + m_last_error);
    }
}

void ModelRouterAdapter::onStreamChunkReceived(const std::string& chunk)
{
    generationChunk(chunk);
}

// MOC removed


