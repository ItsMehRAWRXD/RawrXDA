/**
 * @file model_config.cpp
 * @brief Implementation of model configuration system
 */

#include "model_config.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Backend {

ModelConfiguration::ModelConfiguration(OllamaClient* client)
    : m_client(client) {
    initializeDefaultConfigs();
    if (m_client && m_client->isRunning()) {
        loadAvailableModels();
    }
}

void ModelConfiguration::initializeDefaultConfigs() {
    m_configs.clear();
    m_model_map.clear();
}

void ModelConfiguration::loadAvailableModels() {
    if (!m_client) return;
    
    try {
        auto available_models = m_client->listModels();
        detectCustomModels(available_models);
        
        std::cout << "[ModelConfiguration] Loaded " << m_configs.size() 
                  << " model configurations from Ollama" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[ModelConfiguration] Error loading models: " << e.what() << std::endl;
    }
}

void ModelConfiguration::populateModelDetails(const OllamaModel& ollama_model, ModelConfig& config) {
    config.model_size_bytes = ollama_model.size;
    config.modified_at = ollama_model.modified_at;
    config.format = ollama_model.format;
    config.family = ollama_model.family;
    config.parameter_size = ollama_model.parameter_size;
    config.quantization_level = ollama_model.quantization_level;
    
    // Set default capabilities based on model family
    if (!config.family.empty()) {
        if (config.family.find("llama") != std::string::npos) {
            config.capabilities = {"text_generation", "chat", "code"};
        } else if (config.family.find("codellama") != std::string::npos) {
            config.capabilities = {"code_generation", "code_completion", "debugging"};
        } else if (config.family.find("mistral") != std::string::npos) {
            config.capabilities = {"chat", "reasoning", "analysis"};
        } else {
            config.capabilities = {"text_generation", "chat"};
        }
    }
    
    // Estimate context length based on model size and family
    if (config.family.find("llama3") != std::string::npos) {
        config.context_length = 8192;
    } else if (config.family.find("llama2") != std::string::npos) {
        config.context_length = 4096;
    } else if (config.family.find("codellama") != std::string::npos) {
        config.context_length = 16384;
    } else if (config.family.find("mistral") != std::string::npos) {
        config.context_length = 32768;
    } else if (config.family.find("gemma") != std::string::npos) {
        config.context_length = 8192;
    } else {
        config.context_length = 4096; // Default
    }
    
    // Adjust for BigDaddyG models (likely have larger context)
    if (config.name.find("bigdaddyg") != std::string::npos) {
        config.context_length = 16384; // Larger context for custom models
    }
    
    // Set max tokens based on context
    config.max_tokens = std::min(4096, config.context_length / 2);
}

void ModelConfiguration::detectCustomModels(const std::vector<OllamaModel>& available_models) {
    m_configs.clear();
    m_model_map.clear();
    
    for (const auto& model : available_models) {
        ModelConfig config;
        config.name = model.name;
        populateModelDetails(model, config);
        categorizeModel(model, config);
        
        m_configs.push_back(config);
        m_model_map[config.name] = &m_configs.back();
    }
    
    // Sort by priority (highest first)
    std::sort(m_configs.begin(), m_configs.end(), 
              [](const ModelConfig& a, const ModelConfig& b) {
                  return a.priority > b.priority;
              });
}

void ModelConfiguration::categorizeModel(const OllamaModel& model, ModelConfig& config) {
    std::string name_lower = model.name;
    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
    
    // BigDaddyG models - highest priority
    if (name_lower.find("bigdaddyg") != std::string::npos) {
        if (name_lower.find("god-fast") != std::string::npos) {
            config.display_name = "BigDaddyG God Fast";
            config.description = "High-performance BigDaddyG model optimized for speed";
            config.category = "coding";
            config.priority = 10;
            config.default_options = {{"temperature", 0.1}, {"top_p", 0.95}};
        } else if (name_lower.find("god") != std::string::npos) {
            config.display_name = "BigDaddyG God";
            config.description = "Ultimate BigDaddyG model with maximum capabilities";
            config.category = "analysis";
            config.priority = 9;
            config.default_options = {{"temperature", 0.2}, {"top_p", 0.9}};
        } else if (name_lower.find("16gb-balanced") != std::string::npos) {
            config.display_name = "BigDaddyG 16GB Balanced";
            config.description = "Balanced BigDaddyG model for general use";
            config.category = "chat";
            config.priority = 8;
            config.default_options = {{"temperature", 0.7}, {"top_p", 0.9}};
        } else {
            config.display_name = "BigDaddyG Custom";
            config.description = "Custom BigDaddyG model variant";
            config.category = "chat";
            config.priority = 7;
        }
    }
    
    // QuantumIDE specialized models
    else if (name_lower.find("quantumide") != std::string::npos) {
        if (name_lower.find("architect") != std::string::npos) {
            config.display_name = "QuantumIDE Architect";
            config.description = "Specialized for system architecture and design";
            config.category = "analysis";
            config.priority = 9;
        } else if (name_lower.find("security") != std::string::npos) {
            config.display_name = "QuantumIDE Security";
            config.description = "Security-focused analysis and code review";
            config.category = "security";
            config.priority = 8;
        } else if (name_lower.find("performance") != std::string::npos) {
            config.display_name = "QuantumIDE Performance";
            config.description = "Performance optimization and benchmarking";
            config.category = "performance";
            config.priority = 8;
        } else if (name_lower.find("feature") != std::string::npos) {
            config.display_name = "QuantumIDE Feature";
            config.description = "Feature development and implementation";
            config.category = "feature";
            config.priority = 8;
        } else {
            config.display_name = "QuantumIDE";
            config.description = "QuantumIDE specialized model";
            config.category = "coding";
            config.priority = 7;
        }
    }
    
    // Coding models
    else if (name_lower.find("coder") != std::string::npos || 
             name_lower.find("code") != std::string::npos) {
        if (name_lower.find("qwen") != std::string::npos) {
            config.display_name = "Qwen2.5 Coder";
            config.description = "Alibaba's Qwen2.5 specialized for coding";
            config.category = "coding";
            config.priority = 6;
        } else if (name_lower.find("deepseek") != std::string::npos) {
            config.display_name = "DeepSeek Coder";
            config.description = "DeepSeek's coding model";
            config.category = "coding";
            config.priority = 6;
        } else {
            config.display_name = model.name;
            config.description = "Code generation and understanding model";
            config.category = "coding";
            config.priority = 5;
        }
        config.default_options = {{"temperature", 0.2}, {"top_p", 0.9}};
    }
    
    // Large language models
    else if (name_lower.find("gpt-oss") != std::string::npos) {
        config.display_name = "GPT-OSS";
        config.description = "Open-source GPT model";
        config.category = "chat";
        config.priority = 7;
        config.default_options = {{"temperature", 0.7}, {"top_p", 0.9}};
    }
    
    else if (name_lower.find("gemma3") != std::string::npos) {
        config.display_name = "Gemma 3";
        config.description = "Google's Gemma 3 model";
        config.category = "chat";
        config.priority = 6;
        config.default_options = {{"temperature", 0.7}, {"top_p", 0.9}};
    }
    
    else if (name_lower.find("ministral") != std::string::npos) {
        config.display_name = "Ministral 3";
        config.description = "Mistral's compact model";
        config.category = "chat";
        config.priority = 5;
        config.default_options = {{"temperature", 0.7}, {"top_p", 0.9}};
    }
    
    // Default fallback
    else {
        config.display_name = model.name;
        config.description = "General purpose language model";
        config.category = "chat";
        config.priority = 1;
        config.default_options = {{"temperature", 0.7}, {"top_p", 0.9}};
    }
}

const ModelConfig* ModelConfiguration::getBestModelForTask(const std::string& task) const {
    auto it = PREFERRED_MODELS.find(task);
    if (it == PREFERRED_MODELS.end()) {
        // Default to chat if task not found
        it = PREFERRED_MODELS.find("chat");
        if (it == PREFERRED_MODELS.end()) {
            return m_configs.empty() ? nullptr : &m_configs[0];
        }
    }
    
    // Find the highest priority available model for this task
    for (const std::string& preferred_name : it->second) {
        auto config_it = m_model_map.find(preferred_name);
        if (config_it != m_model_map.end()) {
            return config_it->second;
        }
        
        // Also check partial matches
        for (const auto& config : m_configs) {
            std::string config_name_lower = config.name;
            std::transform(config_name_lower.begin(), config_name_lower.end(), 
                          config_name_lower.begin(), ::tolower);
            
            if (config_name_lower.find(preferred_name) != std::string::npos) {
                return &config;
            }
        }
    }
    
    // Fallback to highest priority model
    return m_configs.empty() ? nullptr : &m_configs[0];
}

bool ModelConfiguration::isModelAvailable(const std::string& model_name) const {
    return m_model_map.find(model_name) != m_model_map.end();
}

const ModelConfig* ModelConfiguration::getModelConfig(const std::string& model_name) const {
    auto it = m_model_map.find(model_name);
    return it != m_model_map.end() ? it->second : nullptr;
}

bool ModelConfiguration::updateModelConfig(const std::string& model_name, const ModelConfig& new_config) {
    auto it = m_model_map.find(model_name);
    if (it == m_model_map.end()) {
        return false;
    }
    
    *it->second = new_config;
    return true;
}

bool ModelConfiguration::setModelContextLength(const std::string& model_name, int context_length) {
    auto it = m_model_map.find(model_name);
    if (it == m_model_map.end()) {
        return false;
    }
    
    it->second->context_length = context_length;
    // Adjust max tokens accordingly
    it->second->max_tokens = std::min(it->second->max_tokens, context_length / 2);
    return true;
}

bool ModelConfiguration::addModelCapability(const std::string& model_name, const std::string& capability) {
    auto it = m_model_map.find(model_name);
    if (it == m_model_map.end()) {
        return false;
    }
    
    // Check if capability already exists
    auto& caps = it->second->capabilities;
    if (std::find(caps.begin(), caps.end(), capability) == caps.end()) {
        caps.push_back(capability);
    }
    return true;
}

bool ModelConfiguration::setModelOption(const std::string& model_name, const std::string& option, double value) {
    auto it = m_model_map.find(model_name);
    if (it == m_model_map.end()) {
        return false;
    }
    
    it->second->default_options[option] = value;
    return true;
}

bool ModelConfiguration::saveConfigurations(const std::string& filepath) const {
    try {
        nlohmann::json j;
        j["models"] = nlohmann::json::array();
        
        for (const auto& config : m_configs) {
            nlohmann::json model_json;
            model_json["name"] = config.name;
            model_json["display_name"] = config.display_name;
            model_json["description"] = config.description;
            model_json["category"] = config.category;
            model_json["priority"] = config.priority;
            model_json["supports_streaming"] = config.supports_streaming;
            model_json["supports_chat"] = config.supports_chat;
            model_json["context_length"] = config.context_length;
            model_json["max_tokens"] = config.max_tokens;
            model_json["capabilities"] = config.capabilities;
            nlohmann::json default_options = nlohmann::json::object();
            for (const auto& [key, val] : config.default_options) {
                default_options[key] = val;
            }
            model_json["default_options"] = default_options;
            model_json["model_size_bytes"] = config.model_size_bytes;
            model_json["modified_at"] = config.modified_at;
            model_json["format"] = config.format;
            model_json["family"] = config.family;
            model_json["parameter_size"] = config.parameter_size;
            model_json["quantization_level"] = config.quantization_level;
            model_json["usage_count"] = config.usage_count;
            model_json["average_response_time"] = config.average_response_time;
            
            j["models"].push_back(model_json);
        }
        
        std::ofstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        file << j.dump(2);
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving configurations: " << e.what() << std::endl;
        return false;
    }
}

bool ModelConfiguration::loadConfigurations(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            return false;
        }
        
        nlohmann::json j = nlohmann::json::parse(std::string(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()));
        
        if (!j.contains("models") || !j["models"].is_array()) {
            return false;
        }
        
        m_configs.clear();
        m_model_map.clear();
        
        for (const auto& model_json : j["models"]) {
            ModelConfig config;
            config.name = model_json.value("name", "");
            config.display_name = model_json.value("display_name", "");
            config.description = model_json.value("description", "");
            config.category = model_json.value("category", "");
            config.priority = model_json.value("priority", 1);
            config.supports_streaming = model_json.value("supports_streaming", true);
            config.supports_chat = model_json.value("supports_chat", true);
            config.context_length = model_json.value("context_length", 4096);
            config.max_tokens = model_json.value("max_tokens", 2048);
            config.capabilities = model_json.value("capabilities", std::vector<std::string>{});
            config.default_options = model_json.value("default_options", std::map<std::string, double>{});
            config.model_size_bytes = model_json.value("model_size_bytes", 0ULL);
            config.modified_at = model_json.value("modified_at", "");
            config.format = model_json.value("format", "");
            config.family = model_json.value("family", "");
            config.parameter_size = model_json.value("parameter_size", "");
            config.quantization_level = model_json.value("quantization_level", "");
            config.usage_count = model_json.value("usage_count", 0);
            config.average_response_time = model_json.value("average_response_time", 0.0);
            
            if (!config.name.empty()) {
                m_configs.push_back(config);
                m_model_map[config.name] = &m_configs.back();
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading configurations: " << e.what() << std::endl;
        return false;
    }
}

} // namespace Backend
} // namespace RawrXD
