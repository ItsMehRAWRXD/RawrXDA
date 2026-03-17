/**
 * @file test_ollama_models.cpp
 * @brief Test program to verify Ollama model integration
 */

#include "ollama_client.h"
#include "model_config.hpp"
#include <iostream>
#include <iomanip>

int main() {
    std::cout << "🔍 Testing RawrXD Ollama Integration..." << std::endl;
    
    // Create Ollama client
    RawrXD::Backend::OllamaClient client("http://localhost:11434");
    
    // Test connection
    if (!client.isRunning()) {
        std::cerr << "❌ Ollama server not running at localhost:11434" << std::endl;
        return 1;
    }
    
    std::cout << "✅ Connected to Ollama server" << std::endl;
    
    // Get version
    std::string version = client.getVersion();
    if (!version.empty()) {
        std::cout << "📦 Ollama version: " << version << std::endl;
    }
    
    // Load model configuration
    RawrXD::Backend::ModelConfiguration config(&client);
    
    // Display available models
    const auto& models = config.getAllModels();
    std::cout << "\n🤖 Available Models (" << models.size() << "):" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    for (const auto& model : models) {
        std::cout << std::left << std::setw(35) << model.display_name 
                  << std::setw(12) << model.category
                  << "Priority: " << model.priority << std::endl;
        std::cout << "  " << model.description << std::endl;
    }
    
    // Test best models for different tasks
    std::cout << "\n🎯 Recommended Models by Task:" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    std::vector<std::string> tasks = {"coding", "chat", "analysis", "security", "performance"};
    for (const std::string& task : tasks) {
        const auto* best_model = config.getBestModelForTask(task);
        if (best_model) {
            std::cout << std::left << std::setw(12) << task << ": " 
                      << best_model->display_name << std::endl;
        }
    }
    
    // Test filtering (equivalent to JavaScript pfs function)
    std::cout << "\n🔍 Testing Model Filtering:" << std::endl;
    std::cout << std::string(30, '-') << std::endl;
    
    // Filter for BigDaddyG models
    auto bigdaddyg_models = client.filterModels(client.listModels(), 
        [](const RawrXD::Backend::OllamaModel& m) {
            return m.name.find("bigdaddyg") != std::string::npos;
        });
    
    std::cout << "BigDaddyG models found: " << bigdaddyg_models.size() << std::endl;
    for (const auto& model : bigdaddyg_models) {
        std::cout << "  - " << model.name << std::endl;
    }
    
    // Test specific model lookup
    const auto* god_model = client.findModelById(client.listModels(), "bigdaddyg-god-fast:latest");
    if (god_model) {
        std::cout << "\n🎖️  Found BigDaddyG God Fast model:" << std::endl;
        std::cout << "  Name: " << god_model->name << std::endl;
        std::cout << "  Size: " << god_model->size / (1024*1024*1024) << " GB" << std::endl;
    }
    
    std::cout << "\n✅ Ollama integration test completed successfully!" << std::endl;
    return 0;
}