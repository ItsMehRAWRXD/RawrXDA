#include "agent_orchestra.h"
#include "logger.h"
#include <iostream>
#include <chrono>
#include <sstream>

AgentOrchestra::AgentOrchestra() : m_activeAgent(""), m_voiceAccent("American") {
    log_info("AgentOrchestra initialized");
}

AgentOrchestra::~AgentOrchestra() {
    log_info("AgentOrchestra destroyed");
}

void AgentOrchestra::addAgent(const AgentConfig& config) {
    m_agents[config.id] = config;
    if (m_activeAgent.empty()) {
        m_activeAgent = config.id;
    }
    log_debug("Agent added: " + config.id);
}

void AgentOrchestra::setActiveAgent(const std::string& agentId) {
    if (m_agents.find(agentId) != m_agents.end()) {
        m_activeAgent = agentId;
        log_debug("Active agent set to: " + agentId);
    } else {
        log_error("Agent not found: " + agentId);
    }
}

std::vector<AgentConfig> AgentOrchestra::getAgents() const {
    std::vector<AgentConfig> result;
    for (const auto& [id, config] : m_agents) {
        result.push_back(config);
    }
    return result;
}

void AgentOrchestra::sendMessage(const std::string& message) {
    if (m_activeAgent.empty()) {
        if (m_errorCallback) m_errorCallback("No active agent");
        return;
    }

    // Create chat message
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    ChatMessage msg;
    msg.agentId = m_activeAgent;
    msg.sender = "user";
    msg.text = message;
    msg.timestamp = std::ctime(&time);
    msg.isVoice = false;
    
    m_chatHistory.push_back(msg);
    
    if (m_messageCallback) {
        m_messageCallback(msg);
    }

    // Generate response
    std::string response = generateResponse(message);
    
    ChatMessage responseMsg;
    responseMsg.agentId = m_activeAgent;
    responseMsg.sender = m_activeAgent;
    responseMsg.text = response;
    responseMsg.timestamp = std::ctime(&std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    responseMsg.isVoice = false;
    
    m_chatHistory.push_back(responseMsg);
    
    if (m_messageCallback) {
        m_messageCallback(responseMsg);
    }

    log_debug("Message sent: " + message);
}

void AgentOrchestra::sendVoiceMessage(const std::string& voiceData) {
    if (m_voiceCallback) {
        m_voiceCallback(voiceData);
    }
}

void AgentOrchestra::clearHistory() {
    m_chatHistory.clear();
    log_debug("Chat history cleared");
}

void AgentOrchestra::startVoiceInput() {
    m_recordingVoice = true;
    log_info("Voice recording started");
}

void AgentOrchestra::stopVoiceInput() {
    m_recordingVoice = false;
    log_info("Voice recording stopped");
}

void AgentOrchestra::setVoiceAccent(const std::string& accent) {
    m_voiceAccent = accent;
    log_debug("Voice accent set to: " + accent);
}

std::string AgentOrchestra::generateResponse(const std::string& message) {
    // Simple response generation (placeholder for LLM integration)
    std::stringstream ss;
    ss << "Agent (" << m_activeAgent << ") response to: " << message;
    return ss.str();
}
