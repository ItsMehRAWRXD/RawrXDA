#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <ctime>
#include <mutex>

namespace RawrXD {
<<<<<<< HEAD

class UniversalModelRouter;
class ContextManager; 

namespace CPUInference {
class CPUInferenceEngine;
}

=======

class UniversalModelRouter;
class ContextManager; 

>>>>>>> origin/main
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
    
<<<<<<< HEAD
    // Native engine integration
    void setModel(const std::string& modelPath);
    
    // Callback for UI updates
    // std::function<void(const Message&)> onMessageReceived;
=======
    // Callback for UI updates
    std::function<void(const Message&)> onMessageReceived;
>>>>>>> origin/main
    
private:
    std::vector<Message> m_history;
    mutable std::mutex m_mutex;
    
    UniversalModelRouter* m_router = nullptr;
    ContextManager* m_context = nullptr;
    
<<<<<<< HEAD
    std::unique_ptr<CPUInference::CPUInferenceEngine> m_engine;
    
=======
>>>>>>> origin/main
    void processResponse(const std::string& modelOutput);
    void appendToHistory(const std::string& role, const std::string& content);
};

} // namespace RawrXD

