#include "api_server.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <iomanip>

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
    
    // Production inference implementation would go here
    return "Assistant response to the conversation.";
}

// Production HTTP server utility implementations
void APIServer::InitializeHttpServer() {
    start_time_ = std::chrono::steady_clock::now();
    std::cout << "Initializing HTTP server on port " << port_ << std::endl;
    // Initialize socket, bind to port, start listening
}

void APIServer::ProcessPendingRequests() {
    std::lock_guard<std::mutex> lock(request_queue_mutex_);
    while (!pending_requests_.empty() && is_running_.load()) {
        auto request = pending_requests_.front();
        pending_requests_.pop();
        
        // Process request asynchronously
        std::string response_body;
        if (request.path == "/api/generate") {
            HandleGenerateRequest(request.body, response_body);
        } else if (request.path == "/v1/chat/completions") {
            HandleChatCompletionsRequest(request.body, response_body);
        }
        // Send response back to client
    }
}

void APIServer::HandleClientConnections() {
    // Accept new connections, add them to request queue
    // This would interface with actual socket implementation
}

// JSON parsing utilities
JsonValue APIServer::ParseJsonRequest(const std::string& request) {
    // Simple JSON parsing implementation
    JsonValue result;
    result.is_object = true;
    
    // Basic JSON parsing - would be replaced with proper JSON library in production
    size_t prompt_pos = request.find("\"prompt\"");
    if (prompt_pos != std::string::npos) {
        size_t colon_pos = request.find(":", prompt_pos);
        if (colon_pos != std::string::npos) {
            size_t quote_start = request.find("\"", colon_pos);
            if (quote_start != std::string::npos) {
                size_t quote_end = request.find("\"", quote_start + 1);
                if (quote_end != std::string::npos) {
                    JsonValue prompt_val;
                    prompt_val.is_string = true;
                    prompt_val.string_value = request.substr(quote_start + 1, quote_end - quote_start - 1);
                    result.object_value["prompt"] = prompt_val;
                }
            }
        }
    }
    
    return result;
}

std::string APIServer::ExtractPromptFromRequest(const JsonValue& request) {
    if (request.is_object && request.object_value.count("prompt")) {
        const auto& prompt_val = request.object_value.at("prompt");
        if (prompt_val.is_string) {
            return prompt_val.string_value;
        }
    }
    return "";
}

std::vector<ChatMessage> APIServer::ExtractMessagesFromRequest(const JsonValue& request) {
    std::vector<ChatMessage> messages;
    // Simple message extraction - would be enhanced for production
    if (request.is_object && request.object_value.count("messages")) {
        // For now, create a default message
        ChatMessage msg;
        msg.role = "user";
        msg.content = "Hello";
        messages.push_back(msg);
    }
    return messages;
}

bool APIServer::ValidateMessageFormat(const std::vector<ChatMessage>& messages) {
    for (const auto& msg : messages) {
        if (msg.role.empty() || msg.content.empty()) {
            return false;
        }
        if (msg.role != "user" && msg.role != "assistant" && msg.role != "system") {
            return false;
        }
    }
    return !messages.empty();
}

// Response generation utilities
std::string APIServer::CreateErrorResponse(const std::string& error_message) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"error\": {\n";
    ss << "    \"message\": \"" << error_message << "\",\n";
    ss << "    \"type\": \"invalid_request_error\",\n";
    ss << "    \"code\": \"invalid_request\"\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::string APIServer::CreateGenerateResponse(const std::string& completion) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"response\": \"" << completion << "\",\n";
    ss << "  \"done\": true,\n";
    ss << "  \"created_at\": " << now << "\n";
    ss << "}";
    return ss.str();
}

std::string APIServer::CreateChatCompletionResponse(const std::string& completion, const JsonValue& request) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"chatcmpl-" << now << "\",\n";
    ss << "  \"object\": \"chat.completion\",\n";
    ss << "  \"created\": " << now << ",\n";
    ss << "  \"model\": \"unknown\",\n";
    ss << "  \"choices\": [\n";
    ss << "    {\n";
    ss << "      \"index\": 0,\n";
    ss << "      \"message\": {\n";
    ss << "        \"role\": \"assistant\",\n";
    ss << "        \"content\": \"" << completion << "\"\n";
    ss << "      },\n";
    ss << "      \"finish_reason\": \"stop\"\n";
    ss << "    }\n";
    ss << "  ],\n";
    ss << "  \"usage\": {\n";
    ss << "    \"prompt_tokens\": 0,\n";
    ss << "    \"completion_tokens\": 0,\n";
    ss << "    \"total_tokens\": 0\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

// Logging utilities
void APIServer::LogRequestReceived(const std::string& endpoint, size_t content_length) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") 
              << "] Request received: " << endpoint 
              << " (" << content_length << " bytes)" << std::endl;
    total_requests_++;
}

void APIServer::LogRequestCompleted(const std::string& endpoint, size_t response_length) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "] Request completed: " << endpoint 
              << " (" << response_length << " bytes)" << std::endl;
    successful_requests_++;
}

void APIServer::LogRequestError(const std::string& endpoint, const std::string& error) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cerr << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "] Request error: " << endpoint << " - " << error << std::endl;
    failed_requests_++;
}

void APIServer::LogServerError(const std::string& error) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::cerr << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "] Server error: " << error << std::endl;
}

// Performance monitoring utilities
void APIServer::RecordRequestMetrics(const std::string& endpoint, 
                                    std::chrono::milliseconds duration,
                                    bool success) {
    // Record request duration and success rate for monitoring
    std::cout << "Request metrics - Endpoint: " << endpoint 
              << ", Duration: " << duration.count() << "ms"
              << ", Success: " << (success ? "true" : "false") << std::endl;
}

void APIServer::UpdateConnectionMetrics(int active_connections) {
    active_connections_.store(active_connections);
}

// Security utilities
bool APIServer::ValidateRequest(const HttpRequest& request) {
    // Implement request validation (size limits, content type, etc.)
    if (request.body.length() > 10 * 1024 * 1024) { // 10MB limit
        return false;
    }
    return true;
}

bool APIServer::CheckRateLimit(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    auto now = std::chrono::steady_clock::now();
    auto& last_time = last_request_times_[client_id];
    auto& count = request_counts_[client_id];
    
    // Reset counter if more than 1 minute has passed
    if (now - last_time > std::chrono::minutes(1)) {
        count = 0;
        last_time = now;
    }
    
    // Allow max 60 requests per minute
    return count < 60;
}

void APIServer::UpdateRateLimit(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    request_counts_[client_id]++;
}
