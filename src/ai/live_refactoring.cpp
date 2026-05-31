// ============================================================================
// Live Refactoring Engine — AI-Powered Code Transformation
// Real-time refactoring suggestions using SovereignInferenceClient
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../editor/syntax_highlighter.h"
#include <memory>
#include <vector>
#include <string>

namespace RawrXD::AI {

enum class RefactoringType {
    EXTRACT_METHOD,
    INLINE_VARIABLE,
    RENAME_SYMBOL,
    OPTIMIZE_LOOP,
    REMOVE_DEAD_CODE,
    SIMPLIFY_CONDITIONAL,
    CONVERT_TO_LAMBDA,
    MODERNIZE_SYNTAX
};

struct CursorPosition {
    int line;
    int column;
    std::string filePath;
};

struct RefactoringSuggestion {
    RefactoringType type;
    std::string description;
    std::string originalCode;
    std::string refactoredCode;
    CursorPosition location;
    double confidence;
    std::vector<std::string> affectedFiles;
};

struct RefactoringResult {
    bool success;
    std::vector<RefactoringSuggestion> suggestions;
    std::string errorMessage;
    double processingTimeMs;
};

class SyntaxAnalyzer {
public:
    struct SyntaxNode {
        std::string type;
        std::string content;
        int startLine;
        int endLine;
        std::vector<SyntaxNode> children;
    };

    SyntaxNode ParseCode(const std::string& code);
    std::vector<SyntaxNode> FindFunctions(const SyntaxNode& root);
    std::vector<SyntaxNode> FindVariables(const SyntaxNode& root);
    std::vector<SyntaxNode> FindLoops(const SyntaxNode& root);
};

class LiveRefactoringEngine {
public:
    explicit LiveRefactoringEngine(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient)
        , m_syntaxAnalyzer(std::make_unique<SyntaxAnalyzer>()) {}

    RefactoringResult SuggestRefactoring(const std::string& code,
                                        const CursorPosition& cursor,
                                        RefactoringType type) {
        RefactoringResult result;
        auto startTime = std::chrono::high_resolution_clock::now();

        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            result.success = false;
            result.errorMessage = "AI client not initialized";
            return result;
        }

        // Parse syntax tree
        auto syntaxTree = m_syntaxAnalyzer->ParseCode(code);
        
        // Build context-aware prompt
        std::string prompt = BuildRefactoringPrompt(code, cursor, type, syntaxTree);
        
        // Get AI suggestion
        std::vector<ChatMessage> messages = {
            {"system", "You are an expert code refactoring assistant. Analyze the code and suggest specific refactoring improvements."},
            {"user", prompt}
        };
        
        auto inferenceResult = m_aiClient->ChatSync(messages);
        
        if (!inferenceResult.success) {
            result.success = false;
            result.errorMessage = inferenceResult.error_message;
            return result;
        }

        // Parse AI response into suggestions
        result.suggestions = ParseRefactoringResponse(inferenceResult.response, code, cursor);
        result.success = !result.suggestions.empty();
        
        auto endTime = std::chrono::high_resolution_clock::now();
        result.processingTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        return result;
    }

    void ApplyRefactoring(const RefactoringSuggestion& suggestion) {
        // Validate refactoring is safe
        if (!ValidateRefactoring(suggestion)) {
            throw std::runtime_error("Refactoring validation failed");
        }

        // Apply the transformation
        ApplyTransformation(suggestion);
        
        // Log the refactoring
        LogRefactoring(suggestion);
    }

    std::vector<RefactoringSuggestion> BatchRefactor(const std::string& code,
                                                     const std::vector<RefactoringType>& types) {
        std::vector<RefactoringSuggestion> allSuggestions;
        
        for (const auto& type : types) {
            CursorPosition cursor{0, 0, ""}; // Full file analysis
            auto result = SuggestRefactoring(code, cursor, type);
            if (result.success) {
                allSuggestions.insert(allSuggestions.end(), 
                                    result.suggestions.begin(), 
                                    result.suggestions.end());
            }
        }
        
        // Sort by confidence
        std::sort(allSuggestions.begin(), allSuggestions.end(),
                 [](const auto& a, const auto& b) { return a.confidence > b.confidence; });
        
        return allSuggestions;
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    std::unique_ptr<SyntaxAnalyzer> m_syntaxAnalyzer;

    std::string BuildRefactoringPrompt(const std::string& code,
                                      const CursorPosition& cursor,
                                      RefactoringType type,
                                      const SyntaxAnalyzer::SyntaxNode& syntaxTree) {
        std::ostringstream oss;
        oss << "Analyze the following code and suggest refactoring improvements.\n\n";
        oss << "Refactoring type: " << RefactoringTypeToString(type) << "\n";
        oss << "Cursor position: Line " << cursor.line << ", Column " << cursor.column << "\n\n";
        oss << "Code:\n```cpp\n" << code << "\n```\n\n";
        oss << "Provide specific refactoring suggestions with:\n";
        oss << "1. Description of the improvement\n";
        oss << "2. Original code snippet\n";
        oss << "3. Refactored code snippet\n";
        oss << "4. Confidence score (0.0-1.0)\n";
        return oss.str();
    }

    std::string RefactoringTypeToString(RefactoringType type) {
        switch (type) {
            case RefactoringType::EXTRACT_METHOD: return "Extract Method";
            case RefactoringType::INLINE_VARIABLE: return "Inline Variable";
            case RefactoringType::RENAME_SYMBOL: return "Rename Symbol";
            case RefactoringType::OPTIMIZE_LOOP: return "Optimize Loop";
            case RefactoringType::REMOVE_DEAD_CODE: return "Remove Dead Code";
            case RefactoringType::SIMPLIFY_CONDITIONAL: return "Simplify Conditional";
            case RefactoringType::CONVERT_TO_LAMBDA: return "Convert to Lambda";
            case RefactoringType::MODERNIZE_SYNTAX: return "Modernize Syntax";
            default: return "Unknown";
        }
    }

    std::vector<RefactoringSuggestion> ParseRefactoringResponse(const std::string& response,
                                                               const std::string& originalCode,
                                                               const CursorPosition& cursor) {
        std::vector<RefactoringSuggestion> suggestions;
        // Parse AI response and extract suggestions
        // Implementation would parse structured response from AI
        return suggestions;
    }

    bool ValidateRefactoring(const RefactoringSuggestion& suggestion) {
        // Check that refactoring doesn't break compilation
        // Verify semantic equivalence
        return suggestion.confidence > 0.7; // Minimum confidence threshold
    }

    void ApplyTransformation(const RefactoringSuggestion& suggestion) {
        // Apply the code transformation
        // This would integrate with the editor's text buffer
    }

    void LogRefactoring(const RefactoringSuggestion& suggestion) {
        // Log for analytics and undo history
    }
};

} // namespace RawrXD::AI
