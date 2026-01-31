#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <functional>

namespace Agents {
namespace ZAC {

// Zero-Trust Agent Communication Bus
class SecureBus {
public:
    SecureBus();
    ~SecureBus();
    
    void start();
    void stop();
    void shutdown();
    
    void register_agent(const std::string& agent_name);
    void unregister_agent(const std::string& agent_name);
    
    void send_message(const std::string& sender, const std::string& receiver, 
                     const std::string& command, const std::map<std::string, std::string>& payload);
    
    void send_async(const std::string& sender, const std::string& receiver,
                   const std::string& command, const std::map<std::string, std::string>& payload,
                   std::function<void(const std::string&)> callback);
    
    bool is_agent_registered(const std::string& agent_name) const;
    std::vector<std::string> get_registered_agents() const;

private:
    void message_processing_loop();
    void process_message(const std::string& message);
    
    std::map<std::string, std::string> registered_agents_;
    std::queue<std::string> message_queue_;
    std::mutex agents_mutex_;
    std::mutex queue_mutex_;
    
    std::atomic<bool> running_;
    std::thread processing_thread_;
};

} // namespace ZAC
} // namespace Agents
