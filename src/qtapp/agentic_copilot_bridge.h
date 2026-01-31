#pragma once


#include <memory>
#include <mutex>

// Forward declarations
class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;

/**
 * @class AgenticCopilotBridge
 * @brief Production-ready Copilot/Cursor-like agent system with thread safety
 * 
 * Features:
 * - Real-time code analysis and generation
 * - Inline code completions (like Copilot)
 * - Multi-turn agent conversations with history
 * - Automatic failure detection and recovery
 * - Model hotpatching for different models
 * - Puppeteer-based response correction
 * - User feedback collection and analysis
 * - Thread-safe concurrent operations
 * - Full IDE integration with all components
 * - On-device model fine-tuning
 */
class AgenticCopilotBridge : public void {

public:
    explicit AgenticCopilotBridge(void* parent = nullptr);
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
    std::string askAgent(const std::string& question, const void*& context = void*());
    std::string continuePreviousConversation(const std::string& followUp);
    
    // Puppeteering and hotpatching (Cursor IDE style)
    std::string executeWithFailureRecovery(const std::string& prompt);
    std::string hotpatchResponse(const std::string& originalResponse, const void*& context);
    bool detectAndCorrectFailure(std::string& response, const void*& context);
    
    // Direct agent command execution (full agentic)
    void* executeAgentTask(const void*& task);
    void* planMultiStepTask(const std::string& goal);
    
    // Code transformation (refactoring, generation, analysis)
    void* transformCode(const std::string& code, const std::string& transformation);
    std::string explainCode(const std::string& code);
    std::string findBugs(const std::string& code);
    
    // Production features: User feedback mechanism
    void submitFeedback(const std::string& feedback, bool isPositive);
    
    // Production features: Model updates
    void updateModel(const std::string& newModelPath);
    
    // Production features: Model training
    void* trainModel(const std::string& datasetPath, const std::string& modelPath, const void*& config);
    bool isTrainingModel() const;
    
    // Production features: Enhanced UI integration
     void showResponse(const std::string& response);
     void displayMessage(const std::string& message);
    
public:
    void onChatMessage(const std::string& message);
    void onModelLoaded(const std::string& modelPath);
    void onEditorContentChanged();
    void onTrainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void onTrainingCompleted(const std::string& modelPath, float finalPerplexity);
    

    void completionReady(const std::string& completion);
    void analysisReady(const std::string& analysis);
    void agentResponseReady(const std::string& response);
    void taskExecuted(const void*& result);
    void errorOccurred(const std::string& error);
    void feedbackSubmitted();
    void modelUpdated();
    void trainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void trainingCompleted(const std::string& modelPath, float finalPerplexity);
    
private:
    // Response correction and validation
    std::string correctHallucinations(const std::string& response, const void*& context);
    std::string enforceResponseFormat(const std::string& response, const std::string& format);
    std::string bypassRefusals(const std::string& response, const std::string& originalPrompt);
    
    // Context building
    void* buildExecutionContext();
    void* buildCodeContext(const std::string& code);
    void* buildFileContext();
    
    // Internal state
    AgenticEngine* m_agenticEngine = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    AgenticExecutor* m_agenticExecutor = nullptr;
    
    std::string m_lastConversationContext;
    void* m_conversationHistory;
    bool m_hotpatchingEnabled = true;
    
    std::mutex m_mutex; // Mutex for thread-safe operations
};

