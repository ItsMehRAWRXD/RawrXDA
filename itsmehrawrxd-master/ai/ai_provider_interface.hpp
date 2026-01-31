// Universal AI provider interface
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace IDE_AI {

// Base AI provider interface
class IAIProvider {
public:
    virtual ~IAIProvider() = default;
    
    // Generate completion
    virtual std::string generateCompletion(const std::string& prompt) = 0;
    
    // Generate completion with streaming
    virtual void generateCompletionAsync(const std::string& prompt, 
                                       std::function<void(const std::string&)> callback) = 0;
    
    // Get provider information
    virtual std::string getProviderName() const = 0;
    virtual bool isAvailable() const = 0;
};

// OpenAI provider implementation
class OpenAIProvider : public IAIProvider {
public:
    OpenAIProvider(const std::string& api_key) : api_key_(api_key) {}
    
    std::string generateCompletion(const std::string& prompt) override {
        // Implementation for OpenAI API calls
        return "OpenAI completion: " + prompt;
    }
    
    void generateCompletionAsync(const std::string& prompt, 
                               std::function<void(const std::string&)> callback) override {
        // Async implementation
        callback(generateCompletion(prompt));
    }
    
    std::string getProviderName() const override { return "OpenAI"; }
    bool isAvailable() const override { return !api_key_.empty(); }
    
private:
    std::string api_key_;
};

// GitHub Copilot provider implementation
class CopilotProvider : public IAIProvider {
public:
    CopilotProvider(const std::string& token) : token_(token) {}
    
    std::string generateCompletion(const std::string& prompt) override {
        // Implementation for Copilot API calls
        return "Copilot completion: " + prompt;
    }
    
    void generateCompletionAsync(const std::string& prompt, 
                               std::function<void(const std::string&)> callback) override {
        callback(generateCompletion(prompt));
    }
    
    std::string getProviderName() const override { return "GitHub Copilot"; }
    bool isAvailable() const override { return !token_.empty(); }
    
private:
    std::string token_;
};

// Self-hosted provider implementation
class SelfHostedProvider : public IAIProvider {
public:
    SelfHostedProvider(const std::string& model_path) : model_path_(model_path) {}
    
    std::string generateCompletion(const std::string& prompt) override {
        // Implementation for local model inference
        return "Self-hosted completion: " + prompt;
    }
    
    void generateCompletionAsync(const std::string& prompt, 
                               std::function<void(const std::string&)> callback) override {
        callback(generateCompletion(prompt));
    }
    
    std::string getProviderName() const override { return "Self-Hosted"; }
    bool isAvailable() const override { return !model_path_.empty(); }
    
private:
    std::string model_path_;
};

// AI Core for managing providers
class AICore {
public:
    AICore() {}
    
    void addProvider(const std::string& name, std::shared_ptr<IAIProvider> provider) {
        providers_[name] = provider;
    }
    
    std::shared_ptr<IAIProvider> getProvider(const std::string& name) {
        auto it = providers_.find(name);
        if (it != providers_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
    std::vector<std::string> getAvailableProviders() {
        std::vector<std::string> available;
        for (const auto& [name, provider] : providers_) {
            if (provider->isAvailable()) {
                available.push_back(name);
            }
        }
        return available;
    }
    
    void setDefaultProvider(const std::string& name) {
        default_provider_ = name;
    }
    
    std::shared_ptr<IAIProvider> getDefaultProvider() {
        if (!default_provider_.empty()) {
            return getProvider(default_provider_);
        }
        return nullptr;
    }
    
private:
    std::map<std::string, std::shared_ptr<IAIProvider>> providers_;
    std::string default_provider_;
};

} // namespace IDE_AI
