// Custom AI models for IDE-specific tasks - Enhanced Implementation
#include "model_layer.hpp"
#include "enhanced_ai_models.cpp"
#include <vector>
#include <string>
#include <memory>

namespace IDE_AI {

// Factory functions for creating enhanced models
std::unique_ptr<EnhancedEmbeddingModel> create_embedding_model(
    const std::string& name = "ide_embedding", 
    size_t input_dim = 512, 
    size_t output_dim = 256) {
    return std::make_unique<EnhancedEmbeddingModel>(name, input_dim, output_dim);
}

std::unique_ptr<EnhancedCompletionModel> create_completion_model(
    const std::string& name = "ide_completion",
    size_t vocab_size = 10000,
    size_t model_dim = 512,
    size_t layers = 6,
    size_t heads = 8) {
    return std::make_unique<EnhancedCompletionModel>(name, vocab_size, model_dim, layers, heads);
}

std::unique_ptr<SelfHostedModel> create_self_hosted_model(
    const std::string& name = "ide_self_hosted") {
    return std::make_unique<SelfHostedModel>(name);
}

// Global model manager instance
class IDEModelManager {
private:
    static std::unique_ptr<ModelManager> instance;
    static std::mutex instance_mutex;
    
public:
    static ModelManager& get_instance() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (!instance) {
            instance = std::make_unique<ModelManager>();
            
            // Initialize default models
            instance->register_model("embedding", create_embedding_model());
            instance->register_model("completion", create_completion_model());
            instance->register_model("self_hosted", create_self_hosted_model());
        }
        return *instance;
    }
    
    static void cleanup() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (instance) {
            instance->unload_all();
            instance.reset();
        }
    }
};

// Static member definitions
std::unique_ptr<ModelManager> IDEModelManager::instance = nullptr;
std::mutex IDEModelManager::instance_mutex;

// Convenience functions for easy access
std::vector<float> encode_text(const std::string& text) {
    auto& manager = IDEModelManager::get_instance();
    auto* embedding_model = manager.get_embedding_model("embedding");
    if (embedding_model) {
        return embedding_model->encode(text);
    }
    return std::vector<float>(256, 0.0f); // Fallback
}

std::string generate_completion(const std::string& prompt, size_t max_length = 100) {
    auto& manager = IDEModelManager::get_instance();
    auto* completion_model = manager.get_completion_model("completion");
    if (completion_model) {
        return completion_model->generate_completion(prompt, max_length);
    }
    return "// TODO: Add implementation"; // Fallback
}

void generate_completion_async(const std::string& prompt,
                              std::function<void(const std::string&)> callback,
                              size_t max_length = 100) {
    auto& manager = IDEModelManager::get_instance();
    auto* self_hosted_model = manager.get_self_hosted_model("self_hosted");
    if (self_hosted_model) {
        self_hosted_model->generate_completion_async(prompt, callback, max_length);
    } else {
        // Fallback to synchronous generation
        std::string result = generate_completion(prompt, max_length);
        callback(result);
    }
}

// Legacy compatibility classes
class EmbeddingModel {
private:
    std::unique_ptr<EnhancedEmbeddingModel> enhanced_model;
    
public:
    EmbeddingModel(size_t input_dim, size_t output_dim) 
        : enhanced_model(std::make_unique<EnhancedEmbeddingModel>("legacy_embedding", input_dim, output_dim)) {}
    
    std::vector<float> encode(const std::string& text) {
        return enhanced_model->encode(text);
    }
};

class CompletionModel {
private:
    std::unique_ptr<EnhancedCompletionModel> enhanced_model;
    
public:
    CompletionModel(size_t vocab_size, size_t hidden_dim) 
        : enhanced_model(std::make_unique<EnhancedCompletionModel>("legacy_completion", vocab_size, hidden_dim)) {}
    
    std::string generateCompletion(const std::string& prompt, size_t max_length = 100) {
        return enhanced_model->generate_completion(prompt, max_length);
    }
};

} // namespace IDE_AI
