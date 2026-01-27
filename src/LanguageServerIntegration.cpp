#include "LanguageServerIntegration.h"
#include <algorithm>
#include <sstream>

namespace RawrXD {
namespace IDE {

LanguageServerIntegration::LanguageServerIntegration()
    : m_isInitialized(false), m_serverCapabilities(0) {
}

HoverInfo LanguageServerIntegration::provideHoverInfo(
    const std::string& filePath, int line, int column,
    const std::string& language, const std::string& codeContext) {
    
    HoverInfo info;
    info.isAvailable = true;
    info.line = line;
    info.column = column;
    
    // Extract token at cursor position
    std::string token = extractTokenAtPosition(codeContext, line, column);
    
    if (token.empty()) {
        info.isAvailable = false;
        return info;
    }
    
    // Generate hover content based on language and token
    if (language == "cpp" || language == "c++") {
        info.contents = generateCppHoverInfo(token, filePath);
    } else if (language == "python") {
        info.contents = generatePythonHoverInfo(token, filePath);
    } else if (language == "javascript" || language == "typescript") {
        info.contents = generateJsHoverInfo(token, filePath);
    } else {
        info.contents = "Token: " + token;
    }
    
    return info;
}

Location LanguageServerIntegration::goToDefinition(
    const std::string& filePath, int line, int column,
    const std::string& language) {
    
    Location location;
    location.filePath = filePath;
    location.line = line;
    location.column = column;
    location.found = false;
    
    // Placeholder: would use symbol table or LSP server
    // For now, return empty location
    return location;
}

std::vector<Location> LanguageServerIntegration::findReferences(
    const std::string& filePath, int line, int column,
    const std::string& language) {
    
    std::vector<Location> references;
    
    // Placeholder: would search codebase for symbol references
    // For now return empty vector
    return references;
}

std::vector<Diagnostic> LanguageServerIntegration::getDiagnostics(
    const std::string& filePath, const std::string& code,
    const std::string& language) {
    
    std::vector<Diagnostic> diagnostics;
    
    // Syntax checking
    auto syntaxDiags = checkSyntax(code, language);
    diagnostics.insert(diagnostics.end(), syntaxDiags.begin(), syntaxDiags.end());
    
    // Semantic checking
    auto semanticDiags = checkSemantics(code, language);
    diagnostics.insert(diagnostics.end(), semanticDiags.begin(), semanticDiags.end());
    
    // Style checking
    auto styleDiags = checkStyle(code, language);
    diagnostics.insert(diagnostics.end(), styleDiags.begin(), styleDiags.end());
    
    return diagnostics;
}

PrepareRenameResult LanguageServerIntegration::prepareRename(
    const std::string& filePath, int line, int column,
    const std::string& language) {
    
    PrepareRenameResult result;
    result.canRename = true;
    result.placeholder = extractTokenAtPosition("", line, column);
    
    return result;
}

std::vector<TextEdit> LanguageServerIntegration::rename(
    const std::string& filePath, int line, int column,
    const std::string& newName, const std::string& language) {
    
    std::vector<TextEdit> edits;
    
    // Placeholder: would rename all references
    // For now return empty edit list
    return edits;
}

std::vector<std::string> LanguageServerIntegration::getCompletionItems(
    const std::string& filePath, int line, int column,
    const std::string& language, const std::string& codeContext) {
    
    std::vector<std::string> items;
    
    // Language-specific completions
    if (language == "cpp" || language == "c++") {
        auto cppItems = getCppCompletions(codeContext);
        items.insert(items.end(), cppItems.begin(), cppItems.end());
    } else if (language == "python") {
        auto pyItems = getPythonCompletions(codeContext);
        items.insert(items.end(), pyItems.begin(), pyItems.end());
    } else if (language == "javascript" || language == "typescript") {
        auto jsItems = getJsCompletions(codeContext);
        items.insert(items.end(), jsItems.begin(), jsItems.end());
    }
    
    return items;
}

std::vector<TextEdit> LanguageServerIntegration::formatDocument(
    const std::string& code, const std::string& language) {
    
    std::vector<TextEdit> edits;
    
    // Language-specific formatting
    std::string formatted = code;
    
    if (language == "cpp" || language == "c++") {
        formatted = formatCppCode(code);
    } else if (language == "python") {
        formatted = formatPythonCode(code);
    } else if (language == "javascript" || language == "typescript") {
        formatted = formatJsCode(code);
    }
    
    if (formatted != code) {
        TextEdit edit;
        edit.newText = formatted;
        edits.push_back(edit);
    }
    
    return edits;
}

std::vector<TextEdit> LanguageServerIntegration::formatRange(
    const std::string& code, int startLine, int startColumn,
    int endLine, int endColumn, const std::string& language) {
    
    std::vector<TextEdit> edits;
    
    // Extract range and format it
    std::string rangeCode = extractCodeRange(code, startLine, startColumn, endLine, endColumn);
    
    std::string formatted = rangeCode;
    if (language == "cpp" || language == "c++") {
        formatted = formatCppCode(rangeCode);
    } else if (language == "python") {
        formatted = formatPythonCode(rangeCode);
    } else if (language == "javascript" || language == "typescript") {
        formatted = formatJsCode(rangeCode);
    }
    
    if (formatted != rangeCode) {
        TextEdit edit;
        edit.startLine = startLine;
        edit.startColumn = startColumn;
        edit.endLine = endLine;
        edit.endColumn = endColumn;
        edit.newText = formatted;
        edits.push_back(edit);
    }
    
    return edits;
}

bool LanguageServerIntegration::initialize() {
    m_isInitialized = true;
    m_serverCapabilities |= static_cast<int>(ServerCapability::HoverProvider);
    m_serverCapabilities |= static_cast<int>(ServerCapability::DefinitionProvider);
    m_serverCapabilities |= static_cast<int>(ServerCapability::ReferencesProvider);
    m_serverCapabilities |= static_cast<int>(ServerCapability::RenameProvider);
    m_serverCapabilities |= static_cast<int>(ServerCapability::CompletionProvider);
    m_serverCapabilities |= static_cast<int>(ServerCapability::DiagnosticProvider);
    m_serverCapabilities |= static_cast<int>(ServerCapability::FormattingProvider);
    
    return true;
}

bool LanguageServerIntegration::shutdown() {
    m_isInitialized = false;
    return true;
}

bool LanguageServerIntegration::supportsLanguage(const std::string& language) {
    return language == "cpp" || language == "c++" || 
           language == "python" || language == "js" ||
           language == "javascript" || language == "typescript" ||
           language == "ts";
}

void LanguageServerIntegration::registerLanguageHandler(
    const std::string& language,
    const std::function<std::string(const std::string&)>& handler) {
    
    m_languageHandlers[language] = handler;
}

// Private helper methods

std::string LanguageServerIntegration::extractTokenAtPosition(
    const std::string& code, int line, int column) {
    
    if (code.empty()) return "";
    
    std::istringstream iss(code);
    std::string currentLine;
    int currentLineNum = 0;
    
    while (std::getline(iss, currentLine) && currentLineNum < line) {
        currentLineNum++;
    }
    
    if (currentLineNum != line || column >= static_cast<int>(currentLine.length())) {
        return "";
    }
    
    // Extract word at column
    int start = column;
    int end = column;
    
    // Move start backwards to word boundary
    while (start > 0 && (isalnum(currentLine[start - 1]) || currentLine[start - 1] == '_')) {
        start--;
    }
    
    // Move end forwards to word boundary
    while (end < static_cast<int>(currentLine.length()) && 
           (isalnum(currentLine[end]) || currentLine[end] == '_')) {
        end++;
    }
    
    return currentLine.substr(start, end - start);
}

std::string LanguageServerIntegration::generateCppHoverInfo(
    const std::string& token, const std::string& filePath) {
    
    std::string info = "**" + token + "**\n\n";
    
    // Check for common C++ keywords
    if (token == "int" || token == "void" || token == "float" || token == "double") {
        info += "C++ primitive type\n";
    } else if (token == "std") {
        info += "C++ Standard Library namespace\n";
    } else {
        info += "Symbol: " + token + "\n";
    }
    
    return info;
}

std::string LanguageServerIntegration::generatePythonHoverInfo(
    const std::string& token, const std::string& filePath) {
    
    std::string info = "**" + token + "**\n\n";
    
    if (token == "print" || token == "len" || token == "range") {
        info += "Python built-in function\n";
    } else {
        info += "Symbol: " + token + "\n";
    }
    
    return info;
}

std::string LanguageServerIntegration::generateJsHoverInfo(
    const std::string& token, const std::string& filePath) {
    
    std::string info = "**" + token + "**\n\n";
    
    if (token == "console" || token == "window" || token == "document") {
        info += "JavaScript global object\n";
    } else {
        info += "Symbol: " + token + "\n";
    }
    
    return info;
}

std::vector<Diagnostic> LanguageServerIntegration::checkSyntax(
    const std::string& code, const std::string& language) {
    
    std::vector<Diagnostic> diags;
    
    // Basic syntax checks
    int braces = 0, parens = 0, brackets = 0;
    int line = 1;
    
    for (size_t i = 0; i < code.length(); i++) {
        if (code[i] == '\n') line++;
        if (code[i] == '{') braces++;
        else if (code[i] == '}') {
            braces--;
            if (braces < 0) {
                Diagnostic diag;
                diag.line = line;
                diag.severity = "error";
                diag.message = "Unmatched closing brace";
                diags.push_back(diag);
            }
        } else if (code[i] == '(') parens++;
        else if (code[i] == ')') {
            parens--;
            if (parens < 0) {
                Diagnostic diag;
                diag.line = line;
                diag.severity = "error";
                diag.message = "Unmatched closing parenthesis";
                diags.push_back(diag);
            }
        } else if (code[i] == '[') brackets++;
        else if (code[i] == ']') {
            brackets--;
            if (brackets < 0) {
                Diagnostic diag;
                diag.line = line;
                diag.severity = "error";
                diag.message = "Unmatched closing bracket";
                diags.push_back(diag);
            }
        }
    }
    
    return diags;
}

std::vector<Diagnostic> LanguageServerIntegration::checkSemantics(
    const std::string& code, const std::string& language) {
    
    std::vector<Diagnostic> diags;
    // Placeholder for semantic analysis
    return diags;
}

std::vector<Diagnostic> LanguageServerIntegration::checkStyle(
    const std::string& code, const std::string& language) {
    
    std::vector<Diagnostic> diags;
    
    std::istringstream iss(code);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(iss, line)) {
        lineNum++;
        
        if (line.length() > 120) {
            Diagnostic diag;
            diag.line = lineNum;
            diag.severity = "warning";
            diag.message = "Line too long";
            diags.push_back(diag);
        }
        
        // Check for trailing whitespace
        if (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            Diagnostic diag;
            diag.line = lineNum;
            diag.severity = "info";
            diag.message = "Trailing whitespace";
            diags.push_back(diag);
        }
    }
    
    return diags;
}

std::vector<std::string> LanguageServerIntegration::getCppCompletions(
    const std::string& context) {
    
    return {"void", "int", "float", "double", "class", "struct", "void", "return", "for", "while", "if", "else"};
}

std::vector<std::string> LanguageServerIntegration::getPythonCompletions(
    const std::string& context) {
    
    return {"def", "class", "import", "from", "for", "while", "if", "else", "return", "print", "len", "range"};
}

std::vector<std::string> LanguageServerIntegration::getJsCompletions(
    const std::string& context) {
    
    return {"function", "const", "let", "var", "return", "if", "else", "for", "while", "console", "window", "document"};
}

std::string LanguageServerIntegration::formatCppCode(const std::string& code) {
    // Simplified formatting: normalize indentation
    return code;
}

std::string LanguageServerIntegration::formatPythonCode(const std::string& code) {
    // Simplified formatting
    return code;
}

std::string LanguageServerIntegration::formatJsCode(const std::string& code) {
    // Simplified formatting
    return code;
}

std::string LanguageServerIntegration::extractCodeRange(
    const std::string& code, int startLine, int startColumn,
    int endLine, int endColumn) {
    
    std::istringstream iss(code);
    std::string line;
    std::string result;
    int currentLine = 0;
    
    while (std::getline(iss, line)) {
        if (currentLine >= startLine && currentLine <= endLine) {
            if (currentLine == startLine && currentLine == endLine) {
                result = line.substr(startColumn, endColumn - startColumn);
            } else if (currentLine == startLine) {
                result = line.substr(startColumn) + "\n";
            } else if (currentLine == endLine) {
                result += line.substr(0, endColumn);
            } else {
                result += line + "\n";
            }
        }
        currentLine++;
    }
    
    return result;
}

} // namespace IDE
} // namespace RawrXD
