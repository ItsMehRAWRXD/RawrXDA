// Chat Interface - Chat UI component
#include "chat_interface.h"
#include "agentic_engine.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

ChatInterface::ChatInterface() {
}

ChatInterface::~ChatInterface() {
}

void ChatInterface::initialize() {
    loadAvailableModels();
}

void ChatInterface::addMessage(const std::string& sender, const std::string& message) {
    m_history.push_back({sender, message});
    if (onMessageAdded) onMessageAdded(sender, message);
}

void ChatInterface::sendMessage(const std::string& message) {
    if (m_busy) return;
    
    addMessage("User", message);
    
    if (isAgentCommand(message)) {
        // Logic for commands
        return;
    }
    
    if (m_agenticEngine) {
        m_busy = true;
        // In reality, this would trigger inference
        m_busy = false;
    }
}

bool ChatInterface::isAgentCommand(const std::string& message) const {
    return message.find("/") == 0;
}

std::string ChatInterface::resolveGgufPath(const std::string& modelName) {
    // Shared logic with AgenticEngine could be moved to a utility
    char* userProfile;
    size_t len;
    _dupenv_s(&userProfile, &len, "USERPROFILE");
    std::string homeDir = userProfile ? userProfile : "";
    free(userProfile);

    std::vector<std::string> searchPaths = { "D:/OllamaModels", homeDir + "/.ollama/models" };
    
    for (const auto& searchPath : searchPaths) {
        if (!std::filesystem::exists(searchPath)) continue;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(searchPath)) {
                if (entry.is_regular_file() && entry.path().extension() == ".gguf") {
                    if (entry.path().filename().string().find(modelName) != std::string::npos) {
                        return entry.path().string();
                    }
                }
            }
        } catch (...) {}
    }
    return "";
}

void ChatInterface::loadAvailableModels() {
    // Mock loading models
    m_selectedModel = "llama3.2:3b";
}

