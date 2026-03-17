#pragma once

// ============================================================================
// AgenticCopilotBridge — C++20 | Win32 | MASM build. No Qt.
// ============================================================================
// Copilot/Cursor-like agent bridge; std::function callbacks replace Qt signals.
// ============================================================================

#include <string>
#include <memory>
#include <mutex>
#include <functional>

class AgenticEngine;
class ChatInterface;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;

class AgenticCopilotBridge
{
public:
    using CompletionReadyFn     = std::function<void(const std::string&)>;
    using AnalysisReadyFn       = std::function<void(const std::string&)>;
    using AgentResponseReadyFn  = std::function<void(const std::string&)>;
    using TaskExecutedFn        = std::function<void(const std::string& resultJson)>;
    using ErrorOccurredFn       = std::function<void(const std::string&)>;
    using TrainingProgressFn    = std::function<void(int epoch, int totalEpochs, float loss, float perplexity)>;
    using TrainingCompletedFn   = std::function<void(const std::string& modelPath, float finalPerplexity)>;

    AgenticCopilotBridge() = default;
    virtual ~AgenticCopilotBridge();

    void initialize(AgenticEngine* engine, ChatInterface* chat,
                   MultiTabEditor* editor, TerminalPool* terminals, AgenticExecutor* executor);

    void setOnCompletionReady(CompletionReadyFn f)      { m_onCompletionReady = std::move(f); }
    void setOnAnalysisReady(AnalysisReadyFn f)          { m_onAnalysisReady = std::move(f); }
    void setOnAgentResponseReady(AgentResponseReadyFn f) { m_onAgentResponseReady = std::move(f); }
    void setOnTaskExecuted(TaskExecutedFn f)            { m_onTaskExecuted = std::move(f); }
    void setOnErrorOccurred(ErrorOccurredFn f)          { m_onErrorOccurred = std::move(f); }
    void setOnTrainingProgress(TrainingProgressFn f)    { m_onTrainingProgress = std::move(f); }
    void setOnTrainingCompleted(TrainingCompletedFn f)   { m_onTrainingCompleted = std::move(f); }

    std::string generateCodeCompletion(const std::string& context, const std::string& prefix = "");
    std::string analyzeActiveFile();
    std::string suggestRefactoring(const std::string& code);
    std::string generateTestsForCode(const std::string& code);

    std::string askAgent(const std::string& question, const std::string& contextJson = "{}");
    std::string continuePreviousConversation(const std::string& followUp);

    std::string executeWithFailureRecovery(const std::string& prompt);
    std::string hotpatchResponse(const std::string& originalResponse, const std::string& contextJson);
    bool detectAndCorrectFailure(std::string& response, const std::string& contextJson);

    std::string executeAgentTask(const std::string& taskJson);
    std::string planMultiStepTask(const std::string& goal);

    std::string transformCode(const std::string& code, const std::string& transformation);
    std::string explainCode(const std::string& code);
    std::string findBugs(const std::string& code);

    void submitFeedback(const std::string& feedback, bool isPositive);
    void updateModel(const std::string& newModelPath);

    std::string trainModel(const std::string& datasetPath, const std::string& modelPath, const std::string& configJson);
    bool isTrainingModel() const;

    void showResponse(const std::string& response);
    void displayMessage(const std::string& message);

    void onChatMessage(const std::string& message);
    void onModelLoaded(const std::string& modelPath);
    void onEditorContentChanged();
    void onTrainingProgress(int epoch, int totalEpochs, float loss, float perplexity);
    void onTrainingCompleted(const std::string& modelPath, float finalPerplexity);

private:
    std::string correctHallucinations(const std::string& response, const std::string& contextJson);
    std::string enforceResponseFormat(const std::string& response, const std::string& format);
    std::string bypassRefusals(const std::string& response, const std::string& originalPrompt);
    std::string buildExecutionContext();
    std::string buildCodeContext(const std::string& code);
    std::string buildFileContext();

    AgenticEngine* m_agenticEngine = nullptr;
    ChatInterface* m_chatInterface = nullptr;
    MultiTabEditor* m_multiTabEditor = nullptr;
    TerminalPool* m_terminalPool = nullptr;
    AgenticExecutor* m_agenticExecutor = nullptr;

    std::string m_lastConversationContext;
    std::string m_conversationHistoryJson;
    bool m_hotpatchingEnabled = true;
    std::mutex m_mutex;

    CompletionReadyFn    m_onCompletionReady;
    AnalysisReadyFn      m_onAnalysisReady;
    AgentResponseReadyFn m_onAgentResponseReady;
    TaskExecutedFn       m_onTaskExecuted;
    ErrorOccurredFn      m_onErrorOccurred;
    TrainingProgressFn   m_onTrainingProgress;
    TrainingCompletedFn  m_onTrainingCompleted;
};
