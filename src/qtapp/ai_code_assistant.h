#ifndef AI_CODE_ASSISTANT_H
#define AI_CODE_ASSISTANT_H


#include <functional>

/**
 * @brief AICodeAssistant - AGENTIC AI assistant with full IDE integration
 * 
 * Full IDE integration capabilities:
 * - Code completion/refactoring/explanation via Ollama
 * - File searching and grepping across workspace
 * - PowerShell command execution and automation
 * - Structured logging and performance metrics
 * - Real-time performance monitoring
 */
class AICodeAssistant : public void
{

public:
    // Data structures
    struct CodeSuggestion {
        std::string text;           // The suggestion text
        std::string type;           // Type: "completion", "refactoring", "explanation", "bugfix", "optimization"
        std::string context;        // Original code context
        float confidence;       // Confidence score 0.0-1.0
        int64_t latency_ms;      // Response latency in milliseconds
        std::chrono::system_clock::time_point timestamp;    // When the suggestion was generated
    };

    explicit AICodeAssistant(void *parent = nullptr);
    ~AICodeAssistant();

    // Configuration
    void setOllamaUrl(const std::string &url);
    void setModel(const std::string &model);
    void setTemperature(float temp);
    void setMaxTokens(int tokens);
    void setWorkspaceRoot(const std::string &root);

    // AI Code Suggestions
    void getCodeCompletion(const std::string &code);
    void getRefactoringSuggestions(const std::string &code);
    void getCodeExplanation(const std::string &code);
    void getBugFixSuggestions(const std::string &code);
    void getOptimizationSuggestions(const std::string &code);

    // IDE Integration - File Operations
    void searchFiles(const std::string &pattern, const std::string &directory = "");
    void grepFiles(const std::string &pattern, const std::string &directory = "", bool caseSensitive = false);
    void findInFile(const std::string &filePath, const std::string &pattern);

    // IDE Integration - Command Execution (PowerShell)
    void executePowerShellCommand(const std::string &command);
    void runBuildCommand(const std::string &command);
    void runTestCommand(const std::string &command);

    // Agentic reasoning
    void analyzeAndRecommend(const std::string &context);
    void autoFixIssue(const std::string &issueDescription, const std::string &codeContext);


    // AI response signals
    void suggestionReceived(const std::string &suggestion, const std::string &type);
    void suggestionStreamChunk(const std::string &chunk);
    void suggestionComplete(bool success, const std::string &message);
    
    // File search signals
    void searchResultsReady(const std::vector<std::string> &results);
    void grepResultsReady(const std::vector<std::string> &results);
    void fileSearchProgress(int processed, int total);
    
    // Command execution signals
    void commandOutputReceived(const std::string &output);
    void commandErrorReceived(const std::string &error);
    void commandCompleted(int exitCode);
    void commandProgress(const std::string &status);
    
    // Agentic signals
    void analysisComplete(const std::string &recommendation);
    void agentActionExecuted(const std::string &action, const std::string &result);
    
    // Metrics and logging
    void latencyMeasured(int64_t milliseconds);
    void errorOccurred(const std::string &error);

private:
    void onNetworkReply();
    void onProcessFinished(int exitCode, void*::ExitStatus exitStatus);
    void onProcessError(void*::ProcessError error);
    void onProcessOutput();

private:
    // Network helpers
    void performOllamaRequest(const std::string &systemPrompt, const std::string &userPrompt, 
                             const std::string &suggestType);
    void setupNetworkRequest(void* &request);
    
    // File system helpers
    std::vector<std::string> recursiveFileSearch(const std::string &directory, const std::string &pattern);
    std::vector<std::string> performGrep(const std::string &directory, const std::string &pattern, bool caseSensitive);
    
    // Command execution helpers
    std::string executePowerShellSync(const std::string &command, bool &success);
    void executePowerShellAsync(const std::string &command);
    
    // Agentic reasoning helpers
    std::string parseAIResponse(const std::string &response);
    std::string formatAgentPrompt(const std::string &context);
    
    // Performance measurement
    void startTiming();
    void endTiming(const std::string &operation);
    
    // Logging
    void logStructured(const std::string &level, const std::string &message, 
                      const void* &metadata = void*());

    // Members
    void* *m_networkManager;
    void* *m_process;
    std::string m_ollamaUrl;
    std::string m_model;
    float m_temperature;
    int m_maxTokens;
    std::string m_workspaceRoot;
    std::chrono::steady_clock m_timer;
    std::vector<uint8_t> m_responseBuffer;
    std::string m_currentSuggestionType;
};

#endif // AI_CODE_ASSISTANT_H


