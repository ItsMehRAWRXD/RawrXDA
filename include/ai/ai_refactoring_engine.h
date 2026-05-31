#pragma once
/**
 * @file ai_refactoring_engine.h
 * @brief AI-powered code refactoring
 * Batch 5 - Item 74: AI refactoring engine
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <future>

namespace RawrXD::AI {

enum class RefactoringType {
    ExtractFunction,
    ExtractVariable,
    Inline,
    Rename,
    Move,
    ReorderParameters,
    ChangeSignature,
    ConvertToModern,
    OptimizeImports,
    SimplifyExpression,
    RemoveDeadCode,
    ConvertToLambda,
    IntroduceParameter,
    EncapsulateField,
    Custom
};

struct RefactoringRequest {
    RefactoringType type;
    std::string code;
    std::string language;
    std::string target;
    std::string newName;
    std::vector<std::string> parameters;
    std::string description;
};

struct RefactoringChange {
    std::string filePath;
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    std::string oldText;
    std::string newText;
    std::string description;
};

struct RefactoringResult {
    std::vector<RefactoringChange> changes;
    std::string explanation;
    bool isSafe;
    std::vector<std::string> warnings;
    bool isComplete;
    std::string error;
};

struct CodeSmell {
    std::string type;
    std::string description;
    int line;
    int column;
    int severity;
    std::string suggestion;
};

class AIRefactoringEngine {
public:
    AIRefactoringEngine();
    ~AIRefactoringEngine();

    // Initialization
    bool initialize();
    void shutdown();

    // Refactoring
    RefactoringResult refactor(const RefactoringRequest& request);
    std::future<RefactoringResult> refactorAsync(const RefactoringRequest& request);

    // Quick refactorings
    RefactoringResult extractFunction(const std::string& code,
                                       int startLine, int startColumn,
                                       int endLine, int endColumn,
                                       const std::string& functionName);
    RefactoringResult extractVariable(const std::string& code,
                                       const std::string& expression,
                                       const std::string& variableName);
    RefactoringResult inlineVariable(const std::string& code,
                                      const std::string& variableName);
    RefactoringResult renameSymbol(const std::string& code,
                                    const std::string& oldName,
                                    const std::string& newName);
    RefactoringResult convertToModern(const std::string& code,
                                        const std::string& language);

    // Code smells
    std::vector<CodeSmell> detectCodeSmells(const std::string& code,
                                              const std::string& language);
    RefactoringResult fixCodeSmell(const std::string& code,
                                    const CodeSmell& smell);

    // Preview
    std::string previewChange(const RefactoringChange& change);
    bool validateChange(const RefactoringChange& change);

    // Apply
    bool applyRefactoring(const RefactoringResult& result);
    bool applyChange(const RefactoringChange& change);
    void undoLastRefactoring();
    void redoLastRefactoring();

    // Configuration
    void setModel(const std::string& model);
    void setMaxTokens(int maxTokens);
    void setTemperature(float temperature);
    void setSafeMode(bool safe);

    // History
    std::vector<RefactoringResult> getHistory() const;
    void clearHistory();

    // Events
    using RefactoringCallback = std::function<void(const RefactoringResult&)>;
    void onRefactoringComplete(RefactoringCallback callback);

private:
    std::string m_model;
    int m_maxTokens{2000};
    float m_temperature{0.3f};
    bool m_safeMode{true};
    std::vector<RefactoringResult> m_history;
    std::vector<RefactoringResult> m_redoStack;

    RefactoringCallback m_refactoringCallback;

    RefactoringResult performRefactoring(const RefactoringRequest& request);
    std::string buildPrompt(const RefactoringRequest& request);
    RefactoringResult parseResponse(const std::string& response);
    void notifyRefactoringComplete(const RefactoringResult& result);
};

// Global instance
AIRefactoringEngine& getAIRefactoringEngine();

} // namespace RawrXD::AI
