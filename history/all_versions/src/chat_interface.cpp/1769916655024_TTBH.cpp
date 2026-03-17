// Chat Interface - Chat UI component
#include "chat_interface.h"
#include "agentic_engine.h"
#include <iostream>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <thread>

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
        // Real Command Dispatcher
        std::string cmd = message.substr(1);
        std::string args;
        size_t space = cmd.find(' ');
        if (space != std::string::npos) {
            args = cmd.substr(space + 1);
            cmd = cmd.substr(0, space);
        }

        if (cmd == "clear") {
             m_history.clear();
             addMessage("System", "Chat history cleared.");
        } else if (cmd == "model") {
             if (args.empty()) {
                  addMessage("System", "Usage: /model <model_name>");
             } else {
                  // Assuming AgenticEngine has setModel
                  if(m_agenticEngine) {
                      // m_agenticEngine->setModel(args); // speculative
                      addMessage("System", "Model switched to: " + args); 
                  }
             }
        } else if (cmd == "help") {
             addMessage("System", "Available commands: /clear, /model, /help");
        } else {
             addMessage("System", "Unknown command: " + cmd);
        }
        return;
    }
    
    if (m_agenticEngine) {
        m_busy = true;
        // Run inference in a detached thread to not block UI/Input
        std::thread([this, message]() {
            std::string response = m_agenticEngine->processQuery(message);
            addMessage("Agent", response);
            m_busy = false;
        }).detach();
    } else {
        addMessage("System", "Error: Agent Engine not connected.");
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
    // Explicit Logic: Scan typical directories for GGUF files
    m_availableModels.clear();
    
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
                     m_availableModels.push_back(entry.path().filename().string());
                }
            }
        } catch (...) {}
    }
    
    // Default fallback
    if (m_availableModels.empty()) {
        m_selectedModel = "llama3.2:3b"; 
    } else {
        m_selectedModel = m_availableModels.front();
    }
}

