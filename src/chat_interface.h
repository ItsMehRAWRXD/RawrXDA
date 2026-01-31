#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

class AgenticEngine;
class ZeroDayAgenticEngine;

namespace RawrXD {
    class PlanOrchestrator;
}

class ChatInterface {
public:
    ChatInterface();
    ~ChatInterface();

    void initialize();
    
    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }
    void setPlanOrchestrator(RawrXD::PlanOrchestrator* orchestrator) { m_planOrchestrator = orchestrator; }
    void setZeroDayAgent(ZeroDayAgenticEngine* agent) { m_zeroDayAgent = agent; }
    
    void addMessage(const std::string& sender, const std::string& message);
    std::string selectedModel() const { return m_selectedModel; }
    bool isMaxMode() const { return m_maxMode; }
    
    void sendMessage(const std::string& message);
    void executeAgentCommand(const std::string& command, const std::string& args = "");
    bool isAgentCommand(const std::string& message) const;

    // Callbacks
    std::function<void(const std::string&, const std::string&)> onMessageAdded;
    std::function<void(const std::string&)> onResponseReceived;
    std::function<void(bool)> onMaxModeChanged;

private:
    void loadAvailableModels();
    std::string resolveGgufPath(const std::string& modelName);
    
    bool m_maxMode = false;
    bool m_busy = false;
    std::string m_selectedModel;
    std::vector<std::pair<std::string, std::string>> m_history;
    
    AgenticEngine* m_agenticEngine = nullptr;
    RawrXD::PlanOrchestrator* m_planOrchestrator = nullptr;
    ZeroDayAgenticEngine* m_zeroDayAgent = nullptr;
};

