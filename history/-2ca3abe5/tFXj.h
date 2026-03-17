#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Backend {

// Chat message structure
struct OllamaChatMessage {
    std::string role;  // "system", "user", "assistant"
    std::string content;
    
    OllamaChatMessage() = default;
    OllamaChatMessage(const std::string& r, const std::string& c) : role(r), content(c) {}
};

// Chat request structure
struct OllamaChatRequest {
    std::vector<OllamaChatMessage> messages;
    std::string model;
    bool stream;
    
    OllamaChatRequest() : stream(false) {}
    OllamaChatRequest(const std::vector<OllamaChatMessage>& m, const std::string& model_name = "", bool s = false)
        : messages(m), model(model_name), stream(s) {}
};

// Chat response structure
struct OllamaChatResponse {
    std::string response;
    bool done;
    
    OllamaChatResponse() : done(false) {}
    OllamaChatResponse(const std::string& resp, bool d) : response(resp), done(d) {}
};

// Tool result structure
struct ToolResult {
    bool success;
    std::string result_data;
    std::string error_message;
    std::string tool_name;
    
    ToolResult() : success(false) {}
    ToolResult(bool s, const std::string& data, const std::string& error = "", const std::string& name = "")
        : success(s), result_data(data), error_message(error), tool_name(name) {}
};

} // namespace Backend
} // namespace RawrXD

// Include tool_registry.h to get ToolResult definition
#include "tool_registry.h"

namespace RawrXD {
namespace Backend {

// Agentic tool executor implementation
inline ToolResult AgenticToolExecutor::Execute(const std::string& tool_name, const std::string& config) {
    // Stub implementation
    (void)tool_name;
    (void)config;
    return ToolResult(false, "", "Not implemented");
}

// Forward declare ToolResult to avoid redefinition
struct ToolResult;

// Agentic tool executor
class AgenticToolExecutor {
public:
    AgenticToolExecutor() = default;
    ~AgenticToolExecutor() = default;
    
    // Execute tool with configuration
    ToolResult Execute(const std::string& tool_name, const std::string& config);
    
    // Execute tool with configuration (alias)
    ToolResult execute(const std::string& tool_name, const std::string& config) {
        return Execute(tool_name, config);
    }
    
    // Chat with tools (stub implementation)
    std::string chatWithTools(const std::string& task, 
                              std::vector<OllamaChatMessage>& history,
                              const ChatConfig& config);
};

// Ollama API client for model inference
class OllamaClient {
public:
    OllamaClient();
    ~OllamaClient();
    
    // Initialize the client
    bool Initialize(const std::string& base_url = "http://localhost:11434");
    
    // Generate completion
    std::string GenerateCompletion(const std::string& prompt, const std::string& model = "");
    
    // Generate chat completion
    std::string GenerateChatCompletion(const std::vector<std::string>& messages, const std::string& model = "");
    
    // Generate chat completion with structured messages
    std::string GenerateChatCompletion(const std::vector<OllamaChatMessage>& messages, const std::string& model = "");
    
    // Synchronous chat
    OllamaChatResponse ChatSync(const OllamaChatRequest& request);
    
    // Synchronous chat (alias)
    OllamaChatResponse chatSync(const OllamaChatRequest& request) { return ChatSync(request); }
    
    // Synchronous chat (alias with vector)
    OllamaChatResponse chatSync(const std::vector<OllamaChatMessage>& messages) {
        OllamaChatRequest req(messages);
        return ChatSync(req);
    }
    
    // Synchronous chat (alias with messages and model)
    OllamaChatResponse chatSync(const std::vector<OllamaChatMessage>& messages, const std::string& model) {
        OllamaChatRequest req(messages, model);
        return ChatSync(req);
    }
    
    // List available models
    std::vector<std::string> ListModels();
    
    // Pull a model
    bool PullModel(const std::string& model_name);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Backend
} // namespace RawrXD