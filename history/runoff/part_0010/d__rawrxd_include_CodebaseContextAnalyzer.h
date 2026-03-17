#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <set>
#include <chrono>

namespace RawrXD {
namespace IDE {

// Symbol information
struct Symbol {
    std::string name;
    std::string type;              // "function", "class", "variable", "enum", etc.
    std::string filePath;
    int lineNumber;
    int columnNumber;
    std::string signature;         // For functions
    std::string documentation;
    std::vector<std::string> usedBy;
    std::vector<std::string> uses;
};

// Codebase analysis result
struct CodebaseAnalysis {
    std::vector<Symbol> allSymbols;
    std::unordered_map<std::string, std::vector<std::string>> dependencies;
    std::unordered_map<std::string, std::vector<std::string>> reverseDependencies;
    std::set<std::string> allFiles;
};

// Current scope information
struct ScopeInfo {
    std::vector<std::string> localVariables;
    std::vector<std::string> functionParameters;
    std::vector<std::string> classMembers;
    std::vector<std::string> imports;
    std::string currentFunction;
    std::string currentClass;
    std::string currentNamespace;
};

// Context-aware code analyzer
class CodebaseContextAnalyzer {
public:
    CodebaseContextAnalyzer();
    ~CodebaseContextAnalyzer() = default;

    // Initialize analyzer with project root
    bool initialize(const std::string& projectRoot);

    // Analyze current scope at a specific location
    ScopeInfo analyzeCurrentScope(
        const std::string& filePath,
        int lineNumber,
        int columnNumber
    );

    // Get relevant symbols for a query
    std::vector<Symbol> getRelevantSymbols(
        const std::string& query,
        const std::string& currentFile,
        int maxResults = 10
    );

    // Get function signature
    std::string getFunctionSignature(const std::string& functionName);

    // Find all references to a symbol
    std::vector<std::pair<std::string, int>> findReferences(const std::string& symbol);

    // Get dependency graph for a file
    std::vector<std::string> getDependencies(const std::string& filePath);

    // Analyze imports in a file
    std::vector<std::string> analyzeImports(const std::string& filePath);

    // Index codebase (run periodically)
    void indexCodebase();

    // Get symbol at position
    Symbol getSymbolAtPosition(
        const std::string& filePath,
        int lineNumber,
        int columnNumber
    );

    // Semantic search
    std::vector<Symbol> semanticSearch(const std::string& query, int maxResults = 5);

    // Code pattern matching
    std::vector<std::pair<std::string, int>> findCodePatterns(
        const std::string& pattern,
        const std::string& language
    );

private:
    struct FileInfo {
        std::string path;
        std::vector<Symbol> symbols;
        std::vector<std::string> imports;
        std::chrono::system_clock::time_point lastIndexed;
    };

    // Parsing
    std::vector<Symbol> parseSymbols(const std::string& filePath);
    ScopeInfo parseScope(
        const std::string& filePath,
        int lineNumber,
        int columnNumber
    );

    // Index building
    void buildDependencyGraph();
    void indexFile(const std::string& filePath);

    // Symbol resolution
    Symbol resolveSymbol(const std::string& name, const ScopeInfo& scope);

    std::string m_projectRoot;
    std::unordered_map<std::string, FileInfo> m_fileIndex;
    std::unordered_map<std::string, Symbol> m_symbolTable;
    CodebaseAnalysis m_analysis;
    bool m_indexed;
};

} // namespace IDE
} // namespace RawrXD
