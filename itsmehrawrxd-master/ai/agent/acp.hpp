#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <atomic>
#include <iostream>
#include <chrono>

namespace ACP {

// Message structure for agent communication
struct Message {
    std::string sender;
    std::string receiver;
    std::string command;
    std::map<std::string, std::string> payload;
    std::string timestamp;
    int priority = 0; // Higher number = higher priority
    
    Message() {
        timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
};

// Base Agent class
class Agent {
public:
    Agent(const std::string& name) : name_(name), running_(false) {
        // Register with the message router
        MessageRouter::get_instance().register_agent(this);
    }
    
    virtual ~Agent() {
        stop();
        MessageRouter::get_instance().unregister_agent(name_);
    }
    
    // Virtual methods to be implemented by derived classes
    virtual void handle_message(const Message& msg) = 0;
    virtual void run() {}
    virtual void stop() {
        running_ = false;
        if (message_thread_.joinable()) {
            message_thread_.join();
        }
    }
    
    // Send a message to another agent
    void send_message(const Message& msg) {
        MessageRouter::get_instance().route_message(msg);
    }
    
    // Get agent name
    const std::string& get_name() const { return name_; }
    
    // Check if agent is running
    bool is_running() const { return running_; }

protected:
    void start_message_processing() {
        running_ = true;
        message_thread_ = std::thread(&Agent::message_processing_loop, this);
    }
    
    void message_processing_loop() {
        while (running_) {
            Message msg;
            if (message_queue_.try_pop(msg)) {
                handle_message(msg);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    
    // Add message to this agent's queue (called by MessageRouter)
    void add_message(const Message& msg) {
        message_queue_.push(msg);
    }

private:
    std::string name_;
    std::atomic<bool> running_;
    std::thread message_thread_;
    ThreadSafeQueue<Message> message_queue_;
};

// Thread-safe message queue
template<typename T>
class ThreadSafeQueue {
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
        condition_.notify_one();
    }
    
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::queue<T> queue_;
    std::condition_variable condition_;
};

// Message router for inter-agent communication
class MessageRouter {
public:
    static MessageRouter& get_instance() {
        static MessageRouter instance;
        return instance;
    }
    
    void register_agent(Agent* agent) {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        agents_[agent->get_name()] = agent;
        std::cout << "MessageRouter: Registered agent '" << agent->get_name() << "'\n";
    }
    
    void unregister_agent(const std::string& name) {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        agents_.erase(name);
        std::cout << "MessageRouter: Unregistered agent '" << name << "'\n";
    }
    
    void route_message(const Message& msg) {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        
        auto it = agents_.find(msg.receiver);
        if (it != agents_.end()) {
            it->second->add_message(msg);
            std::cout << "MessageRouter: Routed message from '" << msg.sender 
                      << "' to '" << msg.receiver << "'\n";
        } else {
            std::cout << "MessageRouter: Warning - No agent found with name '" 
                      << msg.receiver << "'\n";
        }
    }
    
    // Broadcast message to all agents
    void broadcast_message(const Message& msg) {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        
        for (const auto& [name, agent] : agents_) {
            if (name != msg.sender) { // Don't send to sender
                Message broadcast_msg = msg;
                broadcast_msg.receiver = name;
                agent->add_message(broadcast_msg);
            }
        }
        
        std::cout << "MessageRouter: Broadcasted message from '" << msg.sender 
                  << "' to " << agents_.size() - 1 << " agents\n";
    }
    
    // Get list of all registered agents
    std::vector<std::string> get_agent_names() const {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        std::vector<std::string> names;
        for (const auto& [name, agent] : agents_) {
            names.push_back(name);
        }
        return names;
    }
    
    // Check if agent exists
    bool agent_exists(const std::string& name) const {
        std::lock_guard<std::mutex> lock(agents_mutex_);
        return agents_.find(name) != agents_.end();
    }

private:
    std::map<std::string, Agent*> agents_;
    mutable std::mutex agents_mutex_;
};

} // namespace ACP
