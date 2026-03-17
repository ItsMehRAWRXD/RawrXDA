#pragma once
#include <string>
#include <memory>
#include "gui.h"

// SCALAR-ONLY: All threading removed - synchronous scalar operations

class InferenceEngine;

class APIServer {
public:
    APIServer(AppState& app_state);
    ~APIServer();

    bool Start(uint16_t port = 11434);
    bool Stop();
    bool IsRunning() const { return is_running_; }

private:
    AppState& app_state_;
    bool is_running_;  // Scalar: no atomic needed
    uint16_t port_;
    std::shared_ptr<InferenceEngine> engine_;
    
    // Request handlers
    void HandleGenerateRequest(const std::string& request, std::string& response);
    void HandleChatCompletionsRequest(const std::string& request, std::string& response);
    void HandleTagsRequest(std::string& response);
    void HandlePullRequest(const std::string& request, std::string& response);
    
    // Helper functions
    std::string GenerateCompletion(const std::string& prompt);
    std::string GenerateChatCompletion(const std::vector<ChatMessage>& messages);
};
