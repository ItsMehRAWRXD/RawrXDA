#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

// Forward declarations
class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;

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

    // Signals replacement
    // In C++ we can use callbacks, but for now I'll just log or assume caller polls
    // or provide setter methods for callbacks if needed.
    // Given the size, I'll keep it simple: no signals.

private:
    mutable std::mutex m_mutex;
    AgenticEngine* m_agenticEngine = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    AgenticExecutor* m_agenticExecutor = nullptr;

    void completionReady(const std::string& result);
    void analysisReady(const std::string& result);
    void errorOccurred(const std::string& error);
};
