/**
 * @file code_suggestion_engine.h
 * @brief Lightweight code suggestion engine for ghost text
 * 
 * Provides context-aware code completion suggestions without Qt dependencies.
 */

#pragma once

#include <string>
#include <vector>

/**
 * @struct CodeSuggestion
 * @brief A single code suggestion
 */
struct CodeSuggestion {
    std::string text;              ///< The suggested code
    std::string explanation;       ///< Why this suggestion is relevant
    int confidence = 100;          ///< Confidence score 0-100
    bool isComplete = false;       ///< Is this a complete statement?
    
    CodeSuggestion() = default;
    CodeSuggestion(const std::string& t, const std::string& e, int conf = 100, bool complete = false)
        : text(t), explanation(e), confidence(conf), isComplete(complete) {}
};

/**
 * @class CodeSuggestionEngine
 * @brief Generates intelligent code suggestions based on context
 */
class CodeSuggestionEngine {
public:
    CodeSuggestionEngine();
    ~CodeSuggestionEngine();
    
    /**
     * @brief Generate code suggestions for given context
     * @param currentLine The line being edited
     * @param previousLines Context from previous lines
     * @param fileType Programming language/file type
     * @param cursorColumn Column position in current line
     * @return Vector of suggestions sorted by confidence
     */
    std::vector<CodeSuggestion> generateSuggestions(
        const std::string& currentLine,
        const std::string& previousLines,
        const std::string& fileType = "cpp",
        int cursorColumn = -1);
    
    /**
     * @brief Generate a specific type of suggestion
     * @param currentLine The line being edited
     * @param previousLines Context from previous lines
     * @param suggestionType Type of suggestion (complete, next_statement, etc)
     * @return Best matching suggestion
     */
    CodeSuggestion generateSuggestion(
        const std::string& currentLine,
        const std::string& previousLines,
        const std::string& suggestionType = "complete");
    
    /**
     * @brief Set custom keyword mappings for a language
     * @param fileType Programming language identifier
     * @param keywords Keywords and their common completions
     */
    void setLanguageKeywords(const std::string& fileType, 
                            const std::vector<std::pair<std::string, std::string>>& keywords);
    
private:
    // Pattern matching and suggestion generation
    std::string detectPattern(const std::string& currentLine);
    CodeSuggestion generateFromPattern(const std::string& pattern, 
                                       const std::string& currentLine,
                                       const std::string& fileType);
    
    // Language-specific helpers
    CodeSuggestion suggestCpp(const std::string& currentLine, const std::string& previousLines);
    CodeSuggestion suggestPython(const std::string& currentLine, const std::string& previousLines);
    CodeSuggestion suggestJavaScript(const std::string& currentLine, const std::string& previousLines);
    
    // Context analysis
    bool isInComment(const std::string& line, int position);
    bool isInString(const std::string& line, int position);
    std::string getIndentation(const std::string& line);
    std::string getLastToken(const std::string& line);
    
    // Keyword suggestion database
    std::vector<std::pair<std::string, std::string>> m_cppKeywords;
    std::vector<std::pair<std::string, std::string>> m_pythonKeywords;
    std::vector<std::pair<std::string, std::string>> m_jsKeywords;
};
