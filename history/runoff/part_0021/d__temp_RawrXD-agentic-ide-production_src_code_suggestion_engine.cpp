/**
 * @file code_suggestion_engine.cpp
 * @brief Implementation of lightweight code suggestion engine
 */

#include "code_suggestion_engine.h"
#include <algorithm>
#include <cctype>

CodeSuggestionEngine::CodeSuggestionEngine()
{
    // Initialize C++ keywords
    m_cppKeywords = {
        {"if", " () {\n    \n}"},
        {"for", " (int i = 0; i < ; ++i) {\n    \n}"},
        {"while", " () {\n    \n}"},
        {"switch", " () {\n    case:\n        break;\n}"},
        {"class", " {\npublic:\nprivate:\n};"},
        {"struct", " {\n};"},
        {"enum", " {\n};"},
        {"try", " {\n    \n} catch (...) {\n}"},
        {"catch", " (...) {\n    \n}"},
        {"void", "();"},
        {"int", " = 0;"},
        {"auto", " = ;"},
        {"const", " int = 0;"},
        {"static", " int = 0;"},
        {"virtual", " void();"},
        {"namespace", " {\n\n}"},
        {"template", "<typename T>"},
        {"return", " ;"},
        {"include", " <>"},
        {"using", " ;"},
    };
    
    // Initialize Python keywords
    m_pythonKeywords = {
        {"if", ":"},
        {"else", ":"},
        {"elif", ":"},
        {"for", " in :"},
        {"while", ":"},
        {"def", " ():"},
        {"class", ":"},
        {"try", ":"},
        {"except", " Exception:"},
        {"finally", ":"},
        {"with", " as :"},
        {"import", " "},
        {"from", " import "},
        {"return", " "},
        {"yield", " "},
        {"raise", " Exception()"},
        {"assert", " "},
        {"pass", ""},
        {"break", ""},
        {"continue", ""},
    };
    
    // Initialize JavaScript keywords
    m_jsKeywords = {
        {"if", " () {}"},
        {"else", " {}"},
        {"for", " (let i = 0; i < ; i++) {}"},
        {"while", " () {}"},
        {"function", " () {}"},
        {"const", " = ;"},
        {"let", " = ;"},
        {"var", " = ;"},
        {"async", " function() {}"},
        {"await", " ;"},
        {"try", " {} catch (e) {}"},
        {"catch", " (error) {}"},
        {"throw", " new Error();"},
        {"return", " ;"},
        {"class", " {}"},
        {"extends", " "},
        {"super", "();"},
        {"import", " from '';"},
        {"export", " ;"},
        {"switch", " () { case: break; }"},
    };
}

CodeSuggestionEngine::~CodeSuggestionEngine()
{
}

std::vector<CodeSuggestion> CodeSuggestionEngine::generateSuggestions(
    const std::string& currentLine,
    const std::string& previousLines,
    const std::string& fileType,
    int cursorColumn)
{
    std::vector<CodeSuggestion> suggestions;
    
    // Generate primary suggestion
    CodeSuggestion primary = generateSuggestion(currentLine, previousLines, "complete");
    if (!primary.text.empty()) {
        suggestions.push_back(primary);
    }
    
    // Generate secondary suggestions based on last token
    std::string lastToken = getLastToken(currentLine);
    if (!lastToken.empty()) {
        // Try to find keyword completions
        std::vector<std::pair<std::string, std::string>>* keywords = nullptr;
        
        if (fileType == "cpp" || fileType == "c" || fileType == "h") {
            keywords = &m_cppKeywords;
        } else if (fileType == "python" || fileType == "py") {
            keywords = &m_pythonKeywords;
        } else if (fileType == "javascript" || fileType == "js") {
            keywords = &m_jsKeywords;
        }
        
        // Find matching keywords
        if (keywords) {
            for (const auto& kv : *keywords) {
                if (kv.first.find(lastToken) == 0) { // Starts with lastToken
                    std::string remaining = kv.first.substr(lastToken.length());
                    CodeSuggestion sugg(
                        remaining + kv.second,
                        "Complete '" + kv.first + "' keyword",
                        90,
                        true
                    );
                    suggestions.push_back(sugg);
                    
                    if (suggestions.size() >= 3) break; // Limit to 3 suggestions
                }
            }
        }
    }
    
    // Sort by confidence
    std::sort(suggestions.begin(), suggestions.end(),
              [](const CodeSuggestion& a, const CodeSuggestion& b) {
                  return a.confidence > b.confidence;
              });
    
    return suggestions;
}

CodeSuggestion CodeSuggestionEngine::generateSuggestion(
    const std::string& currentLine,
    const std::string& previousLines,
    const std::string& suggestionType)
{
    // Empty line or whitespace only
    if (currentLine.find_first_not_of(" \t\r\n") == std::string::npos) {
        // Suggest indentation-aware new statement
        std::string indent = getIndentation(previousLines);
        return CodeSuggestion(
            "// TODO: ",
            "Add a comment or statement",
            85
        );
    }
    
    // Check if line ends with opening brace
    if (currentLine.find('{') != std::string::npos && 
        currentLine.find('}') == std::string::npos) {
        std::string indent = getIndentation(currentLine) + "    ";
        return CodeSuggestion(
            "\n" + indent,
            "New statement inside block",
            95,
            false
        );
    }
    
    // Check for incomplete control flow statements
    if (currentLine.find("if") != std::string::npos && 
        currentLine.find("(") != std::string::npos &&
        currentLine.find(")") == std::string::npos) {
        return CodeSuggestion(
            ") {\n    \n}",
            "Complete if statement",
            92,
            true
        );
    }
    
    // Default: suggest closing statement
    if (currentLine.find(';') == std::string::npos && 
        !currentLine.empty() &&
        currentLine[currentLine.length()-1] != '{' &&
        currentLine[currentLine.length()-1] != ',' &&
        currentLine[currentLine.length()-1] == ')') {
        return CodeSuggestion(
            ";",
            "Complete statement with semicolon",
            88,
            true
        );
    }
    
    // Generic suggestion
    return CodeSuggestion(
        "// Continue...",
        "Add next statement",
        50
    );
}

void CodeSuggestionEngine::setLanguageKeywords(
    const std::string& fileType,
    const std::vector<std::pair<std::string, std::string>>& keywords)
{
    if (fileType == "cpp" || fileType == "c") {
        m_cppKeywords = keywords;
    } else if (fileType == "python") {
        m_pythonKeywords = keywords;
    } else if (fileType == "javascript") {
        m_jsKeywords = keywords;
    }
}

std::string CodeSuggestionEngine::detectPattern(const std::string& currentLine)
{
    if (currentLine.find("if") != std::string::npos) return "if";
    if (currentLine.find("for") != std::string::npos) return "for";
    if (currentLine.find("while") != std::string::npos) return "while";
    if (currentLine.find("function") != std::string::npos) return "function";
    if (currentLine.find("class") != std::string::npos) return "class";
    return "unknown";
}

CodeSuggestion CodeSuggestionEngine::generateFromPattern(
    const std::string& pattern,
    const std::string& currentLine,
    const std::string& fileType)
{
    if (pattern == "if") {
        return CodeSuggestion(") {\n    \n}", "Complete if block", 90, true);
    } else if (pattern == "for") {
        return CodeSuggestion(") {\n    \n}", "Complete for loop", 90, true);
    } else if (pattern == "class") {
        return CodeSuggestion(" {\n};", "Complete class definition", 85, true);
    }
    return CodeSuggestion("", "", 0);
}

bool CodeSuggestionEngine::isInComment(const std::string& line, int position)
{
    // Find // comment marker
    size_t commentPos = line.find("//");
    if (commentPos != std::string::npos && (int)commentPos < position) {
        return true;
    }
    return false;
}

bool CodeSuggestionEngine::isInString(const std::string& line, int position)
{
    int quoteCount = 0;
    for (int i = 0; i < position && i < (int)line.length(); ++i) {
        if (line[i] == '"' && (i == 0 || line[i-1] != '\\')) {
            quoteCount++;
        }
    }
    return quoteCount % 2 == 1;
}

std::string CodeSuggestionEngine::getIndentation(const std::string& line)
{
    std::string indent;
    for (char c : line) {
        if (c == ' ' || c == '\t') {
            indent += c;
        } else {
            break;
        }
    }
    return indent;
}

std::string CodeSuggestionEngine::getLastToken(const std::string& line)
{
    std::string token;
    
    // Skip whitespace from end
    int endPos = line.length() - 1;
    while (endPos >= 0 && (line[endPos] == ' ' || line[endPos] == '\t')) {
        endPos--;
    }
    
    // Extract token
    for (int i = endPos; i >= 0; --i) {
        char c = line[i];
        if (std::isalnum(c) || c == '_') {
            token = c + token;
        } else if (!token.empty()) {
            break;
        }
    }
    
    return token;
}

CodeSuggestion CodeSuggestionEngine::suggestCpp(
    const std::string& currentLine,
    const std::string& previousLines)
{
    // Simple C++ specific suggestions
    if (currentLine.find("int main") != std::string::npos) {
        return CodeSuggestion("() {\n    return 0;\n}", "Complete main function", 95, true);
    }
    return generateSuggestion(currentLine, previousLines, "cpp");
}

CodeSuggestion CodeSuggestionEngine::suggestPython(
    const std::string& currentLine,
    const std::string& previousLines)
{
    // Simple Python specific suggestions
    if (currentLine.find("def ") != std::string::npos && 
        currentLine.find(":") == std::string::npos) {
        return CodeSuggestion(":", "Complete function definition", 95, true);
    }
    return generateSuggestion(currentLine, previousLines, "python");
}

CodeSuggestion CodeSuggestionEngine::suggestJavaScript(
    const std::string& currentLine,
    const std::string& previousLines)
{
    // Simple JavaScript specific suggestions
    if (currentLine.find("function") != std::string::npos && 
        currentLine.find(")") == std::string::npos) {
        return CodeSuggestion(") {}", "Complete function definition", 95, true);
    }
    return generateSuggestion(currentLine, previousLines, "javascript");
}
