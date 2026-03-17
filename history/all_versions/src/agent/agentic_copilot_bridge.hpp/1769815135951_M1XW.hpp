#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Forward declarations
class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;

/**
 * @class AgenticCopilotBridge
 * @brief Bridge between agentic backend and IDE frontend UI components
 * 
 * Provides a unified interface for:
 * - Code completion and suggestions
 * - Code analysis and refactoring
 * - Test generation
 * - Multi-turn conversations
 * - Agent task execution
 * - Model training and updates
 */
class AgenticCopilotBridge {
public:
    AgenticCopilotBridge();
    ~AgenticCopilotBridge();

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
    std::string askAgent(const std::string& question, const json& context = json());
    std::string continuePreviousConversation(const std::string& followUp);

    // Execution & Error Recovery
    std::string executeWithFailureRecovery(const std::string& prompt);
    std::string hotpatchResponse(const std::string& originalResponse, const json& context);
    bool detectAndCorrectFailure(std::string& response, const json& context);

    // Agent Task Execution
    json executeAgentTask(const json& task);
    json planMultiStepTask(const std::string& goal);

    // Code Transformation
    json transformCode(const std::string& code, const std::string& transformation);
    std::string explainCode(const std::string& code);
    std::string findBugs(const std::string& code);

    // Feedback & Model Management
    void submitFeedback(const std::string& feedback, bool isPositive);
    void updateModel(const std::string& newModelPath);
    json trainModel(const std::string& datasetPath, const std::string& modelPath, const json& config);
    bool isTrainingModel() const;

    // Display methods (Headless compatible)
    void showResponse(const std::string& response);
    void displayMessage(const std::string& message);

    // Event Handlers
    void onChatMessage(const std::string& message);
    void onModelLoaded(const std::string& modelPath);
    void onEditorContentChanged();
    void onTrainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void onTrainingCompleted(const std::string& modelPath, float finalPerplexity);

    // Callbacks (Instead of signals)
    void completionReady(const std::string& completion);
    void analysisReady(const std::string& analysis);
    void agentResponseReady(const std::string& response);
    void taskExecuted(const json& result);
    void errorOccurred(const std::string& errorMsg);
    void modelUpdated();
    void trainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void trainingCompleted(const std::string& modelPath, float finalPerplexity);
    void feedbackSubmitted();
    void responseReady(const std::string& response);
    void messageDisplayed(const std::string& message);
    void chatMessageProcessed(const std::string& originalMessage, const std::string& agentResponse);
    void modelLoaded(const std::string& modelPath);
    void editorAnalysisReady(const std::string& analysis);

private:
    // Private helper methods for corrections
    std::string correctHallucinations(const std::string& response, const json& context);
    std::string enforceResponseFormat(const std::string& response, const std::string& format);
    std::string bypassRefusals(const std::string& response, const std::string& originalPrompt);

    // Context building
    json buildExecutionContext();
    json buildCodeContext(const std::string& code);
    json buildFileContext();

    // Member variables
    mutable std::mutex m_mutex;
    AgenticEngine* m_agenticEngine = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    AgenticExecutor* m_agenticExecutor = nullptr;

    // Conversation & state tracking
    json m_conversationHistory;
    std::string m_lastConversationContext;
    bool m_hotpatchingEnabled = true;

    // Training state
    bool m_isTraining = false;
};



