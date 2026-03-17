#include "api_server.h"
#include "inference_engine.h"
#include <iostream>
#include <sstream>
#include <thread>

APIServer::APIServer(AppState& app_state)
    : app_state_(app_state), is_running_(false), port_(11434) {
    engine_ = std::make_shared<InferenceEngine>();
}

APIServer::~APIServer() {
    Stop();
}

bool APIServer::Start(uint16_t port) {
    if (is_running_.load()) {
        std::cerr << "Server already running" << std::endl;
        return false;
    }
    
    port_ = port;
    
    // Initialize inference engine with model
    std::string model_path = "models/bigdaddyg-q2k.gguf";
    if (!engine_->Initialize(model_path)) {
        std::cerr << "Failed to initialize inference engine" << std::endl;
        return false;
    }
    
    is_running_ = true;
    
    // Start server thread
    server_thread_ = std::make_unique<std::thread>([this]() {
        std::cout << "API Server started on port " << port_ << std::endl;
        std::cout << "Endpoints:" << std::endl;
        std::cout << "  POST /api/generate" << std::endl;
        std::cout << "  POST /v1/chat/completions" << std::endl;
        std::cout << "  GET  /api/tags" << std::endl;
        std::cout << "  POST /api/pull" << std::endl;
        
        // Placeholder: In real implementation, would use cpp-httplib
        // Server loop would handle incoming requests
        while (is_running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    
    return true;
}

bool APIServer::Stop() {
    is_running_ = false;
    
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    std::cout << "API Server stopped" << std::endl;
    return true;
}

void APIServer::HandleGenerateRequest(const std::string& request, std::string& response) {
    std::cout << "Handle /api/generate request" << std::endl;
    
    // Simple JSON parsing - extract prompt (production would use proper JSON parser)
    std::string prompt = "Hello world";
    size_t prompt_pos = request.find("\"prompt\":");
    if (prompt_pos != std::string::npos) {
        size_t start = request.find("\"", prompt_pos + 10);
        size_t end = request.find("\"", start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            prompt = request.substr(start + 1, end - start - 1);
        }
    }
    
    std::cout << "Prompt: " << prompt << std::endl;
    
    if (!engine_) {
        response = R"({"error":"Engine not initialized"})";
        return;
    }
    
    std::string generated = engine_->GenerateToken(prompt, 1);
    response = R"({"model":"loaded-model","response":"" + generated + "","done":true})";
}

void APIServer::HandleChatCompletionsRequest(const std::string& request, std::string& response) {
    std::cout << "Handle /v1/chat/completions request" << std::endl;
    
    // Extract last message content (simplified JSON parsing)
    std::string prompt = "User query";
    size_t content_pos = request.rfind("\"content\":");
    if (content_pos != std::string::npos) {
        size_t start = request.find("\"", content_pos + 11);
        size_t end = request.find("\"", start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            prompt = request.substr(start + 1, end - start - 1);
        }
    }
    
    std::cout << "Chat prompt: " << prompt << std::endl;
    
    if (!engine_) {
        response = R"({"error":"Engine not initialized"})";
        return;
    }
    
    std::string generated = engine_->GenerateToken(prompt, 1);
    response = R"({
        "id":"chatcmpl-123",
        "object":"chat.completion",
        "created":0,
        "model":"loaded-model",
        "choices":[{"message":{"role":"assistant","content":"" + generated + ""},"finish_reason":"stop"}]
    })";
}

void APIServer::HandleTagsRequest(std::string& response) {
    // Return list of loaded models
    std::cout << "Handle /api/tags request" << std::endl;
    response = R"({"models":[{"name":"loaded-model","model":"loaded-model","modified_at":"2025-12-01T00:00:00Z","size":25463410912,"digest":"gpu-loaded","details":{"format":"gguf","family":"llama","parameter_size":"69B","quantization_level":"Q2_K"}}]})";
}

void APIServer::HandlePullRequest(const std::string& request, std::string& response) {
    // Parse request JSON
    // Extract model name
    // Start HuggingFace download
    // Return status
    
    std::cout << "Handle /api/pull request" << std::endl;
    response = R"({"status":"downloading"})";
}

std::string APIServer::GenerateCompletion(const std::string& prompt) {
    std::cout << "Generating completion for: " << prompt.substr(0, 50) << "..." << std::endl;
    
    if (!app_state_.loaded_model || !app_state_.gpu_context) {
        return "Error: No model loaded";
    }
    
    // Placeholder inference logic
    return "This is a generated response from the model.";
}

std::string APIServer::GenerateChatCompletion(const std::vector<ChatMessage>& messages) {
    std::cout << "Generating chat completion with " << messages.size() << " messages" << std::endl;
    
    if (!app_state_.loaded_model || !app_state_.gpu_context) {
        return "Error: No model loaded";
    }
    
    // Placeholder inference logic
    return "Assistant response to the conversation.";
}
