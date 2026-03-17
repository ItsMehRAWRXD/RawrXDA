#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <functional>
#include "../src/nlohmann/json.hpp"

using json = nlohmann::json;

// Forward declarations
class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;

/**
 * @class AgenticCopilotBridge
 * @brief Production-ready Copilot/Cursor-like agent system
 */
class AgenticCopilotBridge {
public:
    explicit AgenticCopilotBridge();
    virtual ~AgenticCopilotBridge();
    
    // Initialize with IDE components
    void initialize(AgenticEngine* engine, ChatInterface* chat, 
                   MultiTabEditor* editor, TerminalPool* terminals, AgenticExecutor* executor);
    
    // Core Copilot-like capabilities (thread-safe)
    std::string generateCodeCompletion(const std::string& context, const std::string& prefix = "");
    std::string analyzeActiveFile();
    std::string suggestRefactoring(const std::string& code);
    std::string generateTestsForCode(const std::string& code);
    
    // Multi-turn conversation (like Copilot Chat)
    std::string askAgent(const std::string& question, const json& context = json());
    std::string continuePreviousConversation(const std::string& followUp);
    
    // Puppeteering and hotpatching (Cursor IDE style)
    std::string executeWithFailureRecovery(const std::string& prompt);
    std::string hotpatchResponse(const std::string& originalResponse, const json& context);
    bool detectAndCorrectFailure(std::string& response, const json& context);
    
    // Direct agent command execution (full agentic)
    json executeAgentTask(const json& task);
    json planMultiStepTask(const std::string& goal);
    
    // Code transformation (refactoring, generation, analysis)
    json transformCode(const std::string& code, const std::string& transformation);
    std::string explainCode(const std::string& code);
    std::string findBugs(const std::string& code);
    
    // Production features: User feedback mechanism
    void submitFeedback(const std::string& feedback, bool isPositive);
    
    // Production features: Model updates
    void updateModel(const std::string& newModelPath);
    
    // Production features: Model training
    json trainModel(const std::string& datasetPath, const std::string& modelPath, const json& config);
    bool isTrainingModel() const;
    
    void showResponse(const std::string& response);
    void displayMessage(const std::string& message);
    
    // Callback registration (Signal/Slot replacement)
    void setCompletionReadyCallback(std::function<void(const std::string&)> cb) { m_completionReadyCb = cb; }
    void setAgentResponseReadyCallback(std::function<void(const std::string&)> cb) { m_agentResponseReadyCb = cb; }

private:
    AgenticEngine* m_engine = nullptr;
    ChatInterface* m_chat = nullptr;
    MultiTabEditor* m_editor = nullptr;
    TerminalPool* m_terminals = nullptr;
    AgenticExecutor* m_executor = nullptr;
    
    std::mutex m_mutex;
    bool m_isTraining = false;
    
    std::function<void(const std::string&)> m_completionReadyCb;
    std::function<void(const std::string&)> m_agentResponseReadyCb;
};
