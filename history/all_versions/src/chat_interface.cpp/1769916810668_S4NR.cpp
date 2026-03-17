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
                  std::string avail = "Available Models: ";
                  if(m_agenticEngine) {
                       auto models = m_agenticEngine->getAvailableModels();
                       for(const auto& m : models) avail += m + ", ";
                  }
                  addMessage("System", avail);
                  addMessage("System", "Current Model: " + m_selectedModel);
                  addMessage("System", "Usage: /model <model_name>");
             } else {
                  if(m_agenticEngine) {
                      m_agenticEngine->setModel(args);
                      m_selectedModel = args;
                      addMessage("System", "Model switched to: " + args); 
                  } else {
                        addMessage("System", "Error: Connection to Agent Engine lost.");
                  }
             }
        } else if (cmd == "help") {
             addMessage("System", "Available commands:");
             addMessage("System", "  /clear         - Clear chat history");
             addMessage("System", "  /model [name]  - Get or set current AI model");
             addMessage("System", "  /help          - Show this help");
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
    // Add current directory models
    searchPaths.push_back("models"); 
    
    for (const auto& searchPath : searchPaths) {
        if (!std::filesystem::exists(searchPath)) continue;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(searchPath)) {
                if (entry.is_regular_file()) {
                    std::string path = entry.path().string();
                    // Expanded extension support
                    if (path.find(".gguf") != std::string::npos || path.find(".bin") != std::string::npos) {
                        if (entry.path().filename().string().find(modelName) != std::string::npos) {
                            return entry.path().string();
                        }
                    }
                }
            }
        } catch (...) {}
    }
    return "";
}

void ChatInterface::loadAvailableModels() {
    if (m_agenticEngine) {
        auto models = m_agenticEngine->getAvailableModels();
        if (!models.empty()) {
            bool found = false;
            for(const auto& m : models) {
                if(m == m_selectedModel) found = true;
            }
            if(!found) m_selectedModel = models[0];
            return;
        }
    }
    if (m_selectedModel.empty()) m_selectedModel = "local-default";
}

