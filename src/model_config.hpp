/**
 * @file model_config.hpp
 * @brief Configuration for Ollama models in RawrXD IDE
 *
 * Automatically detects available models and provides smart defaults
 * based on the user's Ollama setup.
 */

#pragma once

#include "ollama_client.h"
#include <string>
#include <vector>
#include <map>

namespace RawrXD {
namespace Backend {

struct ModelConfig {
    std::string name;
    std::string display_name;
    std::string description;
    std::string category;  // "coding", "chat", "analysis", "creative"
    int priority;          // Higher = preferred default
    bool supports_streaming = true;
    bool supports_chat = true;
    
    // Editable model parameters
    int context_length = 4096;        // Context window size
    int max_tokens = 2048;            // Max output tokens
    std::vector<std::string> capabilities;  // Model capabilities
    std::map<std::string, double> default_options;
    
    // Model metadata
    uint64_t model_size_bytes = 0;
    std::string modified_at;
    std::string format;
    std::string family;
    std::string parameter_size;
    std::string quantization_level;
    
    // Usage tracking
    int usage_count = 0;
    double average_response_time = 0.0;
};

class ModelConfiguration {
public:
    explicit ModelConfiguration(OllamaClient* client);
    
    /**
     * @brief Load available models from Ollama and create configurations
     */
    void loadAvailableModels();
    
    /**
     * @brief Get the best model for a specific task
     */
    const ModelConfig* getBestModelForTask(const std::string& task) const;
    
    /**
     * @brief Get all available model configurations
     */
    const std::vector<ModelConfig>& getAllModels() const { return m_configs; }
    
    /**
     * @brief Check if a specific model is available
     */
    bool isModelAvailable(const std::string& model_name) const;
    
    /**
     * @brief Get model config by name
     */
    const ModelConfig* getModelConfig(const std::string& model_name) const;
    
    /**
     * @brief Update model configuration (context, capabilities, etc.)
     */
    bool updateModelConfig(const std::string& model_name, const ModelConfig& new_config);
    
    /**
     * @brief Set context length for a model
     */
    bool setModelContextLength(const std::string& model_name, int context_length);
    
    /**
     * @brief Add capability to a model
     */
    bool addModelCapability(const std::string& model_name, const std::string& capability);
    
    /**
     * @brief Set model option
     */
    bool setModelOption(const std::string& model_name, const std::string& option, double value);
    
    /**
     * @brief Save configurations to file
     */
    bool saveConfigurations(const std::string& filepath) const;
    
    /**
     * @brief Load configurations from file
     */
    bool loadConfigurations(const std::string& filepath);

private:
    void initializeDefaultConfigs();
    void detectCustomModels(const std::vector<OllamaModel>& available_models);
    void categorizeModel(const OllamaModel& model, ModelConfig& config);
    void populateModelDetails(const OllamaModel& ollama_model, ModelConfig& config);
    
    OllamaClient* m_client;
    std::vector<ModelConfig> m_configs;
    std::map<std::string, ModelConfig*> m_model_map;
};

// Predefined model categories and their preferred models
static const std::map<std::string, std::vector<std::string>> PREFERRED_MODELS = {
    {"coding", {"bigdaddyg-god-fast", "qwen2.5-coder", "deepseek-coder", "codellama"}},
    {"chat", {"bigdaddyg-16gb-balanced", "gemma3:27b", "ministral-3", "gpt-oss:20b"}},
    {"analysis", {"bigdaddyg-god", "quantumide-architect", "gpt-oss:120b"}},
    {"creative", {"bigdaddyg-comprehensive", "dolphin3", "llama3-comprehensive-agentic"}},
    {"security", {"quantumide-security"}},
    {"performance", {"quantumide-performance"}},
    {"feature", {"quantumide-feature"}}
};

} // namespace Backend
} // namespace RawrXD