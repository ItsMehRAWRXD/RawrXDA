// ============================================================================
// todo_scanner.h — Production-Ready TODO Scanner for RawrXD IDE
// ============================================================================
// Header for scanning source files for TODO comments and task tracking
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace RawrXD {

// Forward declaration
class TodoManager;

struct TodoItem {
    std::string type;          // TODO, FIXME, BUG, HACK, NOTE, XXX, OPTIMIZE, PERF
    std::string description;
    std::string filePath;
    int lineNumber;
    
    TodoItem() : lineNumber(-1) {}
};

struct ScanResult {
    std::vector<TodoItem> todos;
    std::string filePath;
    std::string error;
    int scannedFiles = 0;
    int errors = 0;
    
    bool success() const { return error.empty(); }
};

struct LanguagePattern {
    std::string singleLine;      // Single-line comment marker (e.g., "//", "#")
    std::string singleLineAlt;   // Alternative single-line marker (e.g., "#" for PHP)
    std::string multiLineStart;  // Multi-line comment start (e.g., "/*")
    std::string multiLineEnd;    // Multi-line comment end (e.g., "*/")
    std::vector<std::string> filePatterns; // File extensions for this language
};

class TodoScanner {
public:
    explicit TodoScanner(TodoManager* todoManager = nullptr);
    
    // File scanning
    ScanResult scanFile(const std::string& filePath);
    ScanResult scanDirectory(const std::string& directoryPath, 
                            const std::vector<std::string>& extensions = {});
    
    // Results management
    void exportToJson(const ScanResult& result, const std::string& outputPath);
    std::vector<TodoItem> getTodosByType(const std::string& type) const;
    std::vector<TodoItem> getTodosByFile(const std::string& filePath) const;
    
    // Configuration
    void addLanguagePattern(const std::string& language, const LanguagePattern& pattern);
    
private:
    void initializeLanguagePatterns();
    std::string detectLanguage(const std::string& filePath) const;
    void processLine(const std::string& line, const std::string& filePath, 
                    int lineNumber, const LanguagePattern& pattern, ScanResult& result);
    void processCommentBlock(const std::string& commentBlock, const std::string& filePath,
                           int startLine, ScanResult& result);
    void extractTodosFromComment(const std::string& commentText, const std::string& filePath,
                               int lineNumber, ScanResult& result);
    std::string escapeJsonString(const std::string& str);
    
    TodoManager* todoManager_;
    std::unordered_map<std::string, LanguagePattern> languagePatterns_;
};

} // namespace RawrXD
