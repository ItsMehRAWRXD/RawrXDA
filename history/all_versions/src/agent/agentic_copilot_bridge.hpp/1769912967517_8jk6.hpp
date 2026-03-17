#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>
#include "model_invoker.hpp"
#include "agentic_puppeteer.hpp"

// Forward declarations
class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;
class AIIntegrationHub; // Forward declare

using json = nlohmann::json;

class AgenticCopilotBridge {

public:
    explicit AgenticCopilotBridge();
    ~AgenticCopilotBridge();

    // No copy
    AgenticCopilotBridge(const AgenticCopilotBridge&) = delete;
    AgenticCopilotBridge& operator=(const AgenticCopilotBridge&) = delete;

    // Initialization
    void initialize(AgenticEngine* engine, ChatInterface* chat, MultiTabEditor* editor, 
                   TerminalPool* terminals, AgenticExecutor* executor);
    
    void setIntegrationHub(std::shared_ptr<AIIntegrationHub> hub) { m_integrationHub = hub; }

    // Code Generation & Analysis
    std::string generateCodeCompletion(const std::string& context, const std::string& prefix);
    std::string analyzeActiveFile();
    std::string suggestRefactoring(const std::string& code);
    std::string generateTestsForCode(const std::string& code);

    // Conversation & Interaction
    std::string askAgent(const std::string& question, const json& context = json::object());
    std::string continuePreviousConversation(const std::string& followUp);

    // Execution & Error Recovery
    std::string executeWithFailureRecovery(const std::string& prompt);
    std::string hotpatchResponse(const std::string& originalResponse, const json& context);
    bool detectAndCorrectFailure(std::string& response, const json& context);

    // Accessors for derived classes and integration
    ModelInvoker* getModelInvoker() const { return m_modelInvoker.get(); } // Expose internal invoker if used as fallback

private:
    std::mutex m_mutex;
    std::shared_ptr<AIIntegrationHub> m_integrationHub;
    AgenticEngine* m_agenticEngine = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    AgenticExecutor* m_agenticExecutor = nullptr;

    // Fallback invoker if engine is missing
    std::unique_ptr<ModelInvoker> m_modelInvoker;
    std::unique_ptr<AgenticPuppeteer> m_puppeteer;

    void completionReady(const std::string& result);
    void analysisReady(const std::string& result);
    void errorOccurred(const std::string& error);
};
