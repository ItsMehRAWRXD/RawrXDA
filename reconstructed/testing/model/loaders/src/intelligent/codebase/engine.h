// intelligent_codebase_engine.h - Multi-Language Codebase Intelligence Engine
// Converted from Qt to pure C++17
#ifndef INTELLIGENT_CODEBASE_ENGINE_H
#define INTELLIGENT_CODEBASE_ENGINE_H

#include "common/json_types.hpp"
#include "common/callback_system.hpp"
#include "common/time_utils.hpp"
#include "common/string_utils.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <mutex>
#include <regex>

// Parsed symbol information
struct CodeSymbol {
    std::string name;
    std::string type;          // function, class, variable, method, enum, struct
    std::string filePath;
    int lineNumber = 0;
    int endLine = 0;
    std::string signature;
    std::string documentation;
    std::string visibility;    // public, private, protected
    std::vector<std::string> references;
    std::vector<std::string> dependencies;
};

// File analysis result
struct FileAnalysis {
    std::string filePath;
    std::string language;
    int totalLines = 0;
    int codeLines = 0;
    int commentLines = 0;
    int blankLines = 0;
    std::vector<CodeSymbol> symbols;
    std::vector<std::string> imports;
    std::vector<std::string> exports;
    double complexity = 0.0;
    TimePoint analyzedAt;
};

// Dependency graph node
struct DependencyNode {
    std::string filePath;
    std::vector<std::string> dependsOn;
    std::vector<std::string> dependedBy;
    int depth = 0;
};

// Search result
struct CodeSearchResult {
    std::string filePath;
    int lineNumber = 0;
    std::string lineContent;
    std::string context;       // surrounding lines
    double relevance = 0.0;
};

// Codebase statistics
struct CodebaseStats {
    int totalFiles = 0;
    int totalLines = 0;
    int totalCodeLines = 0;
    int totalCommentLines = 0;
    int totalBlankLines = 0;
    std::unordered_map<std::string, int> languageDistribution;
    std::unordered_map<std::string, int> symbolCounts;
    double averageComplexity = 0.0;
    int totalSymbols = 0;
};

class IntelligentCodebaseEngine {
public:
    IntelligentCodebaseEngine();
    ~IntelligentCodebaseEngine();

    // Project scanning
    void scanProject(const std::string& projectPath);
    void scanFile(const std::string& filePath);
    void rescanModifiedFiles();

    // Symbol lookup
    std::vector<CodeSymbol> findSymbol(const std::string& name) const;
    std::vector<CodeSymbol> findSymbolsInFile(const std::string& filePath) const;
    std::vector<CodeSymbol> findSymbolsByType(const std::string& type) const;
    CodeSymbol getSymbolDefinition(const std::string& name) const;
    std::vector<CodeSearchResult> findReferences(const std::string& symbolName) const;

    // Code search
    std::vector<CodeSearchResult> searchCode(const std::string& query) const;
    std::vector<CodeSearchResult> searchRegex(const std::string& pattern) const;
    std::vector<CodeSearchResult> searchByLanguage(const std::string& query,
                                                    const std::string& language) const;

    // Dependency analysis
    std::vector<DependencyNode> getDependencyGraph() const;
    std::vector<std::string> getDependencies(const std::string& filePath) const;
    std::vector<std::string> getDependents(const std::string& filePath) const;
    std::vector<std::vector<std::string>> findCircularDependencies() const;

    // Statistics
    CodebaseStats getCodebaseStats() const;
    FileAnalysis getFileAnalysis(const std::string& filePath) const;
    std::string detectLanguage(const std::string& filePath) const;

    // Navigation
    std::vector<std::string> getProjectFiles() const;
    std::vector<std::string> getProjectFilesByLanguage(const std::string& language) const;

    // Callbacks
    CallbackList<const std::string&> onFileScanComplete;
    CallbackList<const std::string&> onProjectScanComplete;
    CallbackList<const std::string&> onErrorOccurred;

private:
    // Language-specific parsers
    FileAnalysis parseCppFile(const std::string& filePath, const std::string& content);
    FileAnalysis parsePythonFile(const std::string& filePath, const std::string& content);
    FileAnalysis parseJavaScriptFile(const std::string& filePath, const std::string& content);
    FileAnalysis parseRustFile(const std::string& filePath, const std::string& content);
    FileAnalysis parseGoFile(const std::string& filePath, const std::string& content);
    FileAnalysis parseJavaFile(const std::string& filePath, const std::string& content);

    void buildDependencyGraph();
    void countLines(const std::string& content, int& code, int& comment, int& blank);

    mutable std::mutex m_mutex;
    std::string m_projectPath;
    std::unordered_map<std::string, FileAnalysis> m_fileAnalyses;
    std::vector<DependencyNode> m_dependencyGraph;
    std::unordered_set<std::string> m_supportedExtensions;
    CodebaseStats m_stats;
};

#endif // INTELLIGENT_CODEBASE_ENGINE_H
