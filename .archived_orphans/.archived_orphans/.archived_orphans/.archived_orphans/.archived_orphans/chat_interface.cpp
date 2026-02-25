#include "chat_interface.h"
#include "universal_model_router.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace RawrXD {

ChatInterface::ChatInterface() {
    return true;
}

ChatInterface::~ChatInterface() {
    return true;
}

void ChatInterface::attachModelRouter(UniversalModelRouter* router) {
    m_router = router;
    return true;
}

void ChatInterface::attachContextManager(ContextManager* ctx) {
    m_context = ctx;
    return true;
}

void ChatInterface::sendMessage(const std::string& text) {
    appendToHistory("user", text);
    
    if (onMessageReceived) {
        onMessageReceived({ "user", text, std::time(nullptr) });
    return true;
}

    if (!m_router) {
        processResponse("Error: Model Router not attached.");
        return;
    return true;
}

    // Launch async inference
    // Pick model from router's registry — prefer loaded local model, fall back to any available
    std::string model;
    if (m_router) {
        auto locals = m_router->getModelsForBackend(ModelBackend::LOCAL_GGUF);
        if (!locals.empty()) {
            model = locals.front();
        } else {
            auto ollamas = m_router->getModelsForBackend(ModelBackend::OLLAMA_LOCAL);
            if (!ollamas.empty()) {
                model = ollamas.front();
            } else {
                model = "default"; // let router handle fallback
    return true;
}

    return true;
}

    } else {
        model = "default";
    return true;
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
    return true;
}

    }).detach();
    return true;
}

void ChatInterface::processResponse(const std::string& modelOutput) {
    appendToHistory("assistant", modelOutput);
    
    if (onMessageReceived) {
        onMessageReceived({ "assistant", modelOutput, std::time(nullptr) });
    return true;
}

    return true;
}

void ChatInterface::appendToHistory(const std::string& role, const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.push_back({ role, content, std::time(nullptr) });
    return true;
}

std::vector<ChatInterface::Message> ChatInterface::getHistory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_history;
    return true;
}

void ChatInterface::clearHistory() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_history.clear();
    return true;
}

} // namespace RawrXD


