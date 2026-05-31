#include "chat_interface.h"
#include "universal_model_router.h"
#include "cpu_inference_engine.h"
#include "agentic/slash_command_parser.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace RawrXD {

ChatInterface::ChatInterface() {
}

ChatInterface::~ChatInterface() {
}

void ChatInterface::setModel(const std::string& modelPath) {
    m_engine = std::make_unique<CPUInferenceEngine>();
    if (!m_engine->LoadModel(modelPath)) {
        m_engine.reset();
    }
}

void ChatInterface::attachModelRouter(UniversalModelRouter* router) {
    m_router = router;
}

void ChatInterface::attachContextManager(ContextManager* ctx) {
    m_context = ctx;
}

void ChatInterface::sendMessage(const std::string& text) {
    appendToHistory("user", text);
    
    // Check for slash commands
    if (Agentic::SlashCommandParser::IsSlashCommand(text)) {
        processSlashCommand(text);
        return;
    }
    
    if (m_engine) {
        // Use native engine
        std::string prompt = "System: You are a helpful AI assistant.\n";
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& msg : m_history) {
                prompt += msg.role + ": " + msg.content + "\n";
            }
        }
        prompt += "Assistant: ";
        
        std::string response;
        std::vector<int32_t> tokens = m_engine->Tokenize(prompt);
        m_engine->GenerateStreaming(tokens, 100, [&response](const std::string& token) { response += token; }, [](){}, nullptr);
        
        processResponse(response);
    } else if (m_router) {
        // Fallback to router
        std::string model = "gpt-4"; // Fallback
        
        std::thread([this, text, model]() {
            try {
                std::string response = m_router->routeQuery(model, text);
                processResponse(response);
            } catch (const std::exception& e) {
                processResponse(std::string("Error: ") + e.what());
            }
        }).detach();
    } else {
        processResponse("Error: No model router or engine attached.");
    }
}

void ChatInterface::addMessage(const std::string& role, const std::string& content) {
    appendToHistory(role, content);
}

void ChatInterface::processResponse(const std::string& modelOutput) {
    appendToHistory("assistant", modelOutput);
    
    // Removed bidirectional callback
}

void ChatInterface::appendToHistory(const std::string& role, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.push_back({ role, content, static_cast<int64_t>(std::time(nullptr)) });
}

std::vector<ChatInterface::Message> ChatInterface::getHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history;
}

void ChatInterface::clearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
}

void ChatInterface::processSlashCommand(const std::string& text) {
    using namespace Agentic;
    
    auto cmd = SlashCommandParser::Parse(text);
    if (!cmd.valid) {
        processResponse("Error: " + cmd.error);
        return;
    }
    
    // Convert to tool call
    auto toolCall = cmd.ToToolCall();
    std::string toolName = toolCall.value("tool", "");
    
    // Handle agentic commands
    if (toolName == "agentic_explain") {
        std::string target = toolCall["args"].value("target", "selection");
        std::string prompt = "Explain the following code in detail:\n";
        if (m_context) {
            prompt += "Context: " + target + "\n";
        }
        prompt += "Provide a clear explanation of what this code does, its purpose, and any important details.";
        
        // Re-send as regular message with modified prompt
        appendToHistory("system", "[Slash Command: /explain " + target + "]");
        sendMessage(prompt);
        return;
    }
    else if (toolName == "agentic_fix") {
        std::string target = toolCall["args"].value("target", "selection");
        std::string prompt = "Fix any issues in the following code:\n";
        if (m_context) {
            prompt += "Target: " + target + "\n";
        }
        prompt += "Identify bugs, errors, or improvements needed. Provide the corrected code with explanations.";
        
        appendToHistory("system", "[Slash Command: /fix " + target + "]");
        sendMessage(prompt);
        return;
    }
    else if (toolName == "agentic_test") {
        std::string target = toolCall["args"].value("target", "current_file");
        std::string prompt = "Generate tests for the following code:\n";
        if (m_context) {
            prompt += "Target: " + target + "\n";
        }
        prompt += "Create comprehensive unit tests covering normal cases, edge cases, and error conditions.";
        
        appendToHistory("system", "[Slash Command: /test " + target + "]");
        sendMessage(prompt);
        return;
    }
    else if (toolName == "agentic_optimize") {
        std::string target = toolCall["args"].value("target", "selection");
        std::string prompt = "Optimize the following code for performance:\n";
        if (m_context) {
            prompt += "Target: " + target + "\n";
        }
        prompt += "Identify performance bottlenecks and provide optimized code with explanations of improvements.";
        
        appendToHistory("system", "[Slash Command: /optimize " + target + "]");
        sendMessage(prompt);
        return;
    }
    
    // For other commands, pass to tool registry if available
    processResponse("Executing slash command: /" + cmd.command);
}

} // namespace RawrXD

