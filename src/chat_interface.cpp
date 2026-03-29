#include "chat_interface.h"
#include "universal_model_router.h"
#include "cpu_inference_engine.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace RawrXD {

ChatInterface::ChatInterface() {
}

ChatInterface::~ChatInterface() {
}

void ChatInterface::setModel(const std::string& modelPath) {
    m_engine = std::make_unique<CPUInference::CPUInferenceEngine>();
    if (!m_engine->LoadModel(modelPath)) {
        std::cerr << "Failed to load model: " << modelPath << std::endl;
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

void ChatInterface::processResponse(const std::string& modelOutput) {
    appendToHistory("assistant", modelOutput);
    
    // Removed bidirectional callback
}

void ChatInterface::appendToHistory(const std::string& role, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.push_back({ role, content, std::time(nullptr) });
}

std::vector<ChatInterface::Message> ChatInterface::getHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history;
}

void ChatInterface::clearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
}

} // namespace RawrXD

void ChatInterface::processResponse(const std::string& modelOutput) {
    appendToHistory("assistant", modelOutput);
    
    if (onMessageReceived) {
        onMessageReceived({ "assistant", modelOutput, std::time(nullptr) });
    }
}

void ChatInterface::appendToHistory(const std::string& role, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.push_back({ role, content, std::time(nullptr) });
}

std::vector<ChatInterface::Message> ChatInterface::getHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history;
}

void ChatInterface::clearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
}

} // namespace RawrXD

