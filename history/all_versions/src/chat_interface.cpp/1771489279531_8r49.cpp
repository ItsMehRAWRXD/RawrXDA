#include "chat_interface.h"
#include "universal_model_router.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace RawrXD {

ChatInterface::ChatInterface() {
}

ChatInterface::~ChatInterface() {
}

void ChatInterface::attachModelRouter(UniversalModelRouter* router) {
    m_router = router;
}

void ChatInterface::attachContextManager(ContextManager* ctx) {
    m_context = ctx;
}

void ChatInterface::sendMessage(const std::string& text) {
    appendToHistory("user", text);
    
    if (onMessageReceived) {
        onMessageReceived({ "user", text, std::time(nullptr) });
    }
    
    if (!m_router) {
        processResponse("Error: Model Router not attached.");
        return;
    }
    
    // Launch async inference
    // Pick model from router's registry — prefer loaded local model, fall back to any available
    std::string model;
    if (m_router) {
        auto locals = m_router->getModelsForBackend(ModelBackend::LOCAL_GGUF);
        if (!locals.empty()) {
            model = locals.front();
        } else {
            auto ollamas = m_router->getModelsForBackend(ModelBackend::OLLAMA);
            if (!ollamas.empty()) {
                model = ollamas.front();
            } else {
                model = "default"; // let router handle fallback
            }
        }
    } else {
        model = "default";
    }
    
    std::thread([this, text, model]() {
        try {
            // Context Management would happen here
            // e.g. std::string context = m_context->retrieveContext(text);
            // std::string fullPrompt = context + "\n" + text;
            
            std::string response = m_router->routeQuery(model, text);
            processResponse(response);
        } catch (const std::exception& e) {
            processResponse(std::string("Error: ") + e.what());
        }
    }).detach();
}

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

