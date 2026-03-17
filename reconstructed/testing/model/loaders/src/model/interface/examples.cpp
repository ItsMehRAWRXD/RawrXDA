// model_interface_examples.cpp - Model Interface Usage Examples
// Converted from Qt to pure C++17 (QCoreApplication removed)
#include "model_interface.h"
#include "common/logger.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

// Example 1: Basic text generation
void example_basic_generation() {
    std::cout << "=== Example 1: Basic Text Generation ===" << std::endl;

    ModelInterface interface;
    interface.setDefaultModel("llama3");

    GenerationParams params;
    params.prompt = "Explain quantum computing in simple terms.";
    params.maxTokens = 256;
    params.temperature = 0.7f;

    auto result = interface.generate(params);
    if (result.success) {
        std::cout << "Response: " << result.text << std::endl;
        std::cout << "Duration: " << result.durationMs << "ms" << std::endl;
    } else {
        std::cerr << "Error: " << result.error << std::endl;
    }
}

// Example 2: Chat conversation
void example_chat_conversation() {
    std::cout << "\n=== Example 2: Chat Conversation ===" << std::endl;

    ModelInterface interface;
    std::vector<ChatMessage> conversation;

    // Add system message
    conversation.push_back({"system", "You are a helpful programming assistant."});

    // User asks a question
    conversation.push_back({"user", "What is a mutex?"});

    GenerationParams params;
    params.temperature = 0.5f;
    params.maxTokens = 300;

    auto result = interface.chat(conversation, params);
    if (result.success) {
        std::cout << "Assistant: " << result.text << std::endl;

        // Continue conversation
        conversation.push_back({"assistant", result.text});
        conversation.push_back({"user", "Can you give me a C++ example?"});

        result = interface.chat(conversation, params);
        if (result.success) {
            std::cout << "Assistant: " << result.text << std::endl;
        }
    } else {
        std::cerr << "Chat error: " << result.error << std::endl;
    }
}

// Example 3: Streaming generation
void example_streaming() {
    std::cout << "\n=== Example 3: Streaming Generation ===" << std::endl;

    ModelInterface interface;

    GenerationParams params;
    params.prompt = "Write a haiku about programming.";
    params.maxTokens = 100;
    params.stream = true;

    std::cout << "Stream: ";
    interface.generateStream(params, [](const std::string& token) {
        std::cout << token << std::flush;
    });
    std::cout << std::endl;
}

// Example 4: Multiple backends
void example_multi_backend() {
    std::cout << "\n=== Example 4: Multiple Backends ===" << std::endl;

    ModelInterface interface;

    // Configure Ollama
    interface.setEndpoint(ModelBackend::Ollama, "http://localhost:11434");
    interface.loadModel("llama3", ModelBackend::Ollama);

    // Configure llama.cpp
    interface.setEndpoint(ModelBackend::LlamaCpp, "http://localhost:8080");
    interface.loadModel("mistral", ModelBackend::LlamaCpp);

    // Check availability
    std::cout << "Ollama: " << interface.getBackendStatus(ModelBackend::Ollama) << std::endl;
    std::cout << "llama.cpp: " << interface.getBackendStatus(ModelBackend::LlamaCpp) << std::endl;

    // List models
    auto models = interface.getAvailableModels();
    std::cout << "Available models: " << models.size() << std::endl;
    for (const auto& m : models) {
        std::cout << "  - " << m.name << " (" << m.backend << ")" << std::endl;
    }
}

// Example 5: Generation with callbacks
void example_callbacks() {
    std::cout << "\n=== Example 5: Callbacks ===" << std::endl;

    ModelInterface interface;

    // Register callbacks
    interface.onGenerationComplete.connect([](const GenerationResult& r) {
        std::cout << "[Callback] Generation complete: " << r.durationMs << "ms" << std::endl;
    });

    interface.onErrorOccurred.connect([](const std::string& err) {
        std::cerr << "[Callback] Error: " << err << std::endl;
    });

    interface.onModelLoaded.connect([](const std::string& name) {
        std::cout << "[Callback] Model loaded: " << name << std::endl;
    });

    // This will trigger the onModelLoaded callback
    interface.loadModel("codellama", ModelBackend::Ollama);

    GenerationParams params;
    params.model = "codellama";
    params.prompt = "Write a function to reverse a linked list in C++.";
    params.maxTokens = 500;

    // This will trigger onGenerationComplete or onErrorOccurred
    interface.generate(params);
}

// Example 6: Embeddings
void example_embeddings() {
    std::cout << "\n=== Example 6: Embeddings ===" << std::endl;

    ModelInterface interface;

    auto result = interface.getEmbedding("Hello, world!");
    if (result.success) {
        std::cout << "Embedding dimensions: " << result.dimensions << std::endl;
        std::cout << "First 5 values: ";
        for (int i = 0; i < std::min(5, (int)result.embedding.size()); i++) {
            std::cout << result.embedding[i] << " ";
        }
        std::cout << std::endl;
    } else {
        std::cerr << "Embedding error: " << result.error << std::endl;
    }
}

// Example 7: Temperature comparison
void example_temperature_comparison() {
    std::cout << "\n=== Example 7: Temperature Comparison ===" << std::endl;

    ModelInterface interface;
    std::string prompt = "Complete this sentence: The future of AI is";

    for (float temp : {0.1f, 0.5f, 1.0f, 1.5f}) {
        GenerationParams params;
        params.prompt = prompt;
        params.temperature = temp;
        params.maxTokens = 50;

        auto result = interface.generate(params);
        std::cout << "  temp=" << temp << ": ";
        if (result.success) std::cout << result.text << std::endl;
        else std::cout << "[error: " << result.error << "]" << std::endl;
    }
}

// Example 8: Error handling
void example_error_handling() {
    std::cout << "\n=== Example 8: Error Handling ===" << std::endl;

    ModelInterface interface;

    // Try with unavailable backend
    interface.setEndpoint(ModelBackend::Ollama, "http://localhost:99999");

    GenerationParams params;
    params.prompt = "This should fail.";

    auto result = interface.generate(params);
    if (!result.success) {
        std::cout << "Expected error: " << result.error << std::endl;
        std::cout << "Duration before failure: " << result.durationMs << "ms" << std::endl;
    }
}

// Main entry point for examples
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    std::cout << "Model Interface Examples" << std::endl;
    std::cout << "========================" << std::endl;

    example_basic_generation();
    example_chat_conversation();
    example_streaming();
    example_multi_backend();
    example_callbacks();
    example_embeddings();
    example_temperature_comparison();
    example_error_handling();

    std::cout << "\nAll examples complete." << std::endl;
    return 0;
}
