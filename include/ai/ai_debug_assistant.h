#pragma once
/**
 * @file ai_debug_assistant.h
 * @brief AI-powered debugging assistance
 * Batch 5 - Item 73: AI debug assistant
 */

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <functional>

namespace RawrXD::AI {

struct StackFrame {
    std::string function;
    std::string file;
    int line;
    std::string module;
    std::vector<std::pair<std::string, std::string>> variables;
};

struct ErrorInfo {
    std::string type;
    std::string message;
    std::string file;
    int line;
    std::vector<StackFrame> stackTrace;
    std::string code;
};

struct VariableInfo {
    std::string name;
    std::string type;
    std::string value;
    bool isPointer;
    bool isNull;
};

struct DebugSuggestion {
    std::string description;
    std::string code;
    std::string explanation;
    int confidence;
};

struct DebugAnalysis {
    std::string rootCause;
    std::vector<DebugSuggestion> suggestions;
    std::vector<std::string> relatedIssues;
    std::string documentation;
};

class AIDebugAssistant {
public:
    AIDebugAssistant();
    ~AIDebugAssistant();

    // Initialization
    bool initialize();
    void shutdown();

    // Error analysis
    DebugAnalysis analyzeError(const ErrorInfo& error);
    DebugAnalysis analyzeException(const std::string& exceptionType,
                                    const std::string& message,
                                    const std::vector<StackFrame>& stackTrace);

    // Variable inspection
    std::string explainVariable(const VariableInfo& variable);
    std::vector<std::string> suggestVariableInspection(const std::vector<VariableInfo>& variables);

    // Breakpoint suggestions
    std::vector<int> suggestBreakpoints(const std::string& code);
    std::vector<int> suggestConditionalBreakpoints(const std::string& code,
                                                       const std::string& condition);

    // Step suggestions
    std::string suggestNextStep(const std::vector<StackFrame>& currentStack,
                                 const std::map<std::string, VariableInfo>& variables);

    // Fix suggestions
    std::vector<DebugSuggestion> suggestFixes(const ErrorInfo& error);
    std::optional<DebugSuggestion> getBestFix(const ErrorInfo& error);

    // Log analysis
    std::vector<std::string> analyzeLog(const std::vector<std::string>& logLines);
    std::optional<ErrorInfo> extractErrorFromLog(const std::vector<std::string>& logLines);

    // Memory analysis
    std::vector<std::string> detectMemoryIssues(const std::vector<VariableInfo>& variables);
    std::string explainMemoryLayout(const std::string& type, size_t size);

    // Configuration
    void setModel(const std::string& model);
    void setMaxTokens(int maxTokens);
    void setTemperature(float temperature);

    // Events
    using AnalysisCallback = std::function<void(const DebugAnalysis&)>;
    void onAnalysisComplete(AnalysisCallback callback);

private:
    std::string m_model;
    int m_maxTokens{2000};
    float m_temperature{0.3f};

    AnalysisCallback m_analysisCallback;

    std::string buildErrorPrompt(const ErrorInfo& error);
    DebugAnalysis parseAnalysisResponse(const std::string& response);
    void notifyAnalysisComplete(const DebugAnalysis& analysis);
};

// Global instance
AIDebugAssistant& getAIDebugAssistant();

} // namespace RawrXD::AI
