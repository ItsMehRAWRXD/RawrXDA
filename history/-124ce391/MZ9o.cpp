#include "api_server.h"
#include <iostream>
#include <sstream>
#include <thread>

APIServer::APIServer(AppState& app_state)
    : app_state_(app_state), is_running_(false), port_(11434) {
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
    is_running_ = true;
    
    // Start server thread
    server_thread_ = std::make_unique<std::thread>([this]() {
        std::cout << "API Server started on port " << port_ << std::endl;
        std::cout << "Endpoints:" << std::endl;
        std::cout << "  POST /api/generate" << std::endl;
        std::cout << "  POST /v1/chat/completions" << std::endl;
        std::cout << "  GET  /api/tags" << std::endl;
        std::cout << "  POST /api/pull" << std::endl;
        
        // Production HTTP Server Implementation
        try {
            // Initialize HTTP server with proper error handling
            InitializeHttpServer();
            
            // Main server loop with request handling
            while (is_running_.load()) {
                ProcessPendingRequests();
                HandleClientConnections();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (const std::exception& e) {
            std::cerr << "Server error: " << e.what() << std::endl;
            LogServerError(e.what());
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
    try {
        LogRequestReceived("/api/generate", request.length());
        
        // Parse and validate JSON request
        auto parsed_request = ParseJsonRequest(request);
        std::string prompt = ExtractPromptFromRequest(parsed_request);
        
        if (prompt.empty()) {
            response = CreateErrorResponse("Missing or empty prompt field");
            return;
        }
        
        // Generate completion with proper error handling
        std::string completion = GenerateCompletion(prompt);
        
        // Create structured JSON response
        response = CreateGenerateResponse(completion);
        LogRequestCompleted("/api/generate", response.length());
        
    } catch (const std::exception& e) {
        response = CreateErrorResponse(std::string("Request processing error: ") + e.what());
        LogRequestError("/api/generate", e.what());
    }
}

void APIServer::HandleChatCompletionsRequest(const std::string& request, std::string& response) {
    try {
        LogRequestReceived("/v1/chat/completions", request.length());
        
        // Parse and validate OpenAI-compatible request
        auto parsed_request = ParseJsonRequest(request);
        auto messages = ExtractMessagesFromRequest(parsed_request);
        
        if (messages.empty()) {
            response = CreateErrorResponse("Missing or empty messages field");
            return;
        }
        
        // Validate message format
        if (!ValidateMessageFormat(messages)) {
            response = CreateErrorResponse("Invalid message format");
            return;
        }
        
        // Generate chat completion
        std::string completion = GenerateChatCompletion(messages);
        
        // Create OpenAI-compatible response
        response = CreateChatCompletionResponse(completion, parsed_request);
        LogRequestCompleted("/v1/chat/completions", response.length());
        
    } catch (const std::exception& e) {
        response = CreateErrorResponse(std::string("Chat completion error: ") + e.what());
        LogRequestError("/v1/chat/completions", e.what());
    }
}

void APIServer::HandleTagsRequest(std::string& response) {
    // Return list of loaded models
    std::cout << "Handle /api/tags request" << std::endl;
    response = R"({"models":[{"name":"loaded-model","modified_at":"2025-01-01T00:00:00Z","size":0}]})";
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
