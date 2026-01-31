#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

namespace ACP {

// Agent Communication Protocol message structure
struct Message {
    std::string sender;
    std::string receiver;
    std::string command;
    std::map<std::string, std::string> payload;
    std::chrono::steady_clock::time_point timestamp;
    
    Message() {
        timestamp = std::chrono::steady_clock::now();
    }
};

// Base agent class
class Agent {
public:
    Agent(const std::string& name) : name_(name) {}
    virtual ~Agent() = default;
    
    virtual void handle_message(const Message& msg) = 0;
    virtual void send_message(const Message& msg) = 0;
    
    const std::string& get_name() const { return name_; }

protected:
    std::string name_;
};

} // namespace ACP
