// Agent Communication Protocol (ACP) for inter-agent communication
#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace IDE_AI {

// Message types for agent communication
enum class MessageType {
    REQUEST,
    RESPONSE,
    NOTIFICATION,
    ERROR
};

// Priority levels for messages
enum class Priority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL
};

// Base message structure
struct ACPMessage {
    std::string id;
    std::string from_agent;
    std::string to_agent;
    MessageType type;
    Priority priority;
    std::string content;
    std::map<std::string, std::string> metadata;
    std::string timestamp;
    
    ACPMessage() : type(MessageType::REQUEST), priority(Priority::NORMAL) {}
};

// Agent interface
class IAgent {
public:
    virtual ~IAgent() = default;
    
    virtual std::string getAgentId() const = 0;
    virtual std::string getAgentType() const = 0;
    virtual bool isAvailable() const = 0;
    
    virtual void sendMessage(const ACPMessage& message) = 0;
    virtual void receiveMessage(const ACPMessage& message) = 0;
    
    virtual void start() = 0;
    virtual void stop() = 0;
};

// Agent Communication Protocol implementation
class ACP {
public:
    ACP() {}
    
    // Register an agent
    void registerAgent(std::shared_ptr<IAgent> agent) {
        agents_[agent->getAgentId()] = agent;
    }
    
    // Unregister an agent
    void unregisterAgent(const std::string& agent_id) {
        agents_.erase(agent_id);
    }
    
    // Send message between agents
    bool sendMessage(const ACPMessage& message) {
        auto it = agents_.find(message.to_agent);
        if (it != agents_.end()) {
            it->second->receiveMessage(message);
            return true;
        }
        return false;
    }
    
    // Broadcast message to all agents
    void broadcastMessage(const ACPMessage& message) {
        for (auto& [id, agent] : agents_) {
            if (id != message.from_agent) {
                ACPMessage broadcast_msg = message;
                broadcast_msg.to_agent = id;
                agent->receiveMessage(broadcast_msg);
            }
        }
    }
    
    // Get available agents
    std::vector<std::string> getAvailableAgents() {
        std::vector<std::string> available;
        for (const auto& [id, agent] : agents_) {
            if (agent->isAvailable()) {
                available.push_back(id);
            }
        }
        return available;
    }
    
private:
    std::map<std::string, std::shared_ptr<IAgent>> agents_;
};

// Code Orchestrator Agent
class CodeOrchestrator : public IAgent {
public:
    CodeOrchestrator(std::shared_ptr<ACP> acp) : acp_(acp) {}
    
    std::string getAgentId() const override { return "code_orchestrator"; }
    std::string getAgentType() const override { return "orchestrator"; }
    bool isAvailable() const override { return true; }
    
    void sendMessage(const ACPMessage& message) override {
        acp_->sendMessage(message);
    }
    
    void receiveMessage(const ACPMessage& message) override {
        // Handle incoming messages
        if (message.type == MessageType::REQUEST) {
            handleRequest(message);
        }
    }
    
    void start() override {
        // Start the orchestrator
    }
    
    void stop() override {
        // Stop the orchestrator
    }
    
private:
    void handleRequest(const ACPMessage& message) {
        // Handle orchestration requests
    }
    
    std::shared_ptr<ACP> acp_;
};

// AI Collaborator Agent
class AICollaborator : public IAgent {
public:
    AICollaborator(std::shared_ptr<ACP> acp, std::shared_ptr<IAIProvider> ai_provider) 
        : acp_(acp), ai_provider_(ai_provider) {}
    
    std::string getAgentId() const override { return "ai_collaborator"; }
    std::string getAgentType() const override { return "ai_assistant"; }
    bool isAvailable() const override { return ai_provider_->isAvailable(); }
    
    void sendMessage(const ACPMessage& message) override {
        acp_->sendMessage(message);
    }
    
    void receiveMessage(const ACPMessage& message) override {
        if (message.type == MessageType::REQUEST) {
            handleAIRequest(message);
        }
    }
    
    void start() override {
        // Start the AI collaborator
    }
    
    void stop() override {
        // Stop the AI collaborator
    }
    
private:
    void handleAIRequest(const ACPMessage& message) {
        // Process AI requests
        std::string response = ai_provider_->generateCompletion(message.content);
        
        ACPMessage response_msg;
        response_msg.id = generateMessageId();
        response_msg.from_agent = getAgentId();
        response_msg.to_agent = message.from_agent;
        response_msg.type = MessageType::RESPONSE;
        response_msg.content = response;
        
        sendMessage(response_msg);
    }
    
    std::string generateMessageId() {
        return "msg_" + std::to_string(std::time(nullptr));
    }
    
    std::shared_ptr<ACP> acp_;
    std::shared_ptr<IAIProvider> ai_provider_;
};

} // namespace IDE_AI
