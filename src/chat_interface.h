#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <ctime>
#include <mutex>

namespace RawrXD {

class UniversalModelRouter;
class ContextManager; 

namespace CPUInference {
class CPUInferenceEngine;
}

class ChatInterface {
public:
    struct Message {
        std::string role; // "user", "assistant", "system"
        std::string content;
        long timestamp;
    };
    
    ChatInterface();
    ~ChatInterface();
    
    void sendMessage(const std::string& text);
    std::vector<Message> getHistory() const;
    void clearHistory();
    
    // Real Integration
    void attachModelRouter(UniversalModelRouter* router);
    void attachContextManager(ContextManager* ctx);
    
    // Native engine integration
    void setModel(const std::string& modelPath);
    
    // Callback for UI updates
    // std::function<void(const Message&)> onMessageReceived;
    
private:
    std::vector<Message> m_history;
    mutable std::mutex m_mutex;
    
    UniversalModelRouter* m_router = nullptr;
    ContextManager* m_context = nullptr;
    
    std::unique_ptr<CPUInference::CPUInferenceEngine> m_engine;
    
    void processResponse(const std::string& modelOutput);
    void appendToHistory(const std::string& role, const std::string& content);
};

} // namespace RawrXD

