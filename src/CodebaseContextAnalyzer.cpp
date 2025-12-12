#include "CodebaseContextAnalyzer.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace fs = std::filesystem;

namespace RawrXD {
namespace IDE {

CodebaseContextAnalyzer::CodebaseContextAnalyzer()
    : m_projectRoot(""), m_indexed(false) {
}

bool CodebaseContextAnalyzer::initialize(const std::string& projectRoot) {
    m_projectRoot = projectRoot;
    
    // Check if directory exists
    if (!fs::exists(projectRoot)) {
        return false;
    }
    
    // Initial indexing
    indexCodebase();
    return true;
}

ScopeInfo CodebaseContextAnalyzer::analyzeCurrentScope(
    const std::string& filePath, int lineNumber, int columnNumber) {
    
    return parseScope(filePath, lineNumber, columnNumber);
}

std::vector<Symbol> CodebaseContextAnalyzer::getRelevantSymbols(
    const std::string& query, const std::string& currentFile, int maxResults) {
    
    std::vector<Symbol> results;
    
    // Search through symbol table
    for (const auto& [name, symbol] : m_symbolTable) {
        if (name.find(query) != std::string::npos) {
            results.push_back(symbol);
            if ((int)results.size() >= maxResults) break;
        }
    }
    
    // Sort by relevance (same file first)
    std::sort(results.begin(), results.end(),
        [&currentFile](const Symbol& a, const Symbol& b) {
            bool aInFile = a.filePath == currentFile;
            bool bInFile = b.filePath == currentFile;
            return aInFile > bInFile;
        });
    
    return results;
}

std::string CodebaseContextAnalyzer::getFunctionSignature(const std::string& functionName) {
    auto it = m_symbolTable.find(functionName);
    if (it != m_symbolTable.end()) {
        return it->second.signature;
    }
    return "";
}

std::vector<std::pair<std::string, int>> CodebaseContextAnalyzer::findReferences(
    const std::string& symbol) {
    
    std::vector<std::pair<std::string, int>> results;
    
    auto it = m_symbolTable.find(symbol);
    if (it != m_symbolTable.end()) {
        for (const auto& user : it->second.usedBy) {
            // In real implementation, would track exact line numbers
            results.push_back({user, 0});
        }
    }
    
    return results;
}

std::vector<std::string> CodebaseContextAnalyzer::getDependencies(
    const std::string& filePath) {
    
    auto it = m_analysis.dependencies.find(filePath);
    if (it != m_analysis.dependencies.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> CodebaseContextAnalyzer::analyzeImports(
    const std::string& filePath) {
    
    auto it = m_fileIndex.find(filePath);
    if (it != m_fileIndex.end()) {
        return it->second.imports;
    }
    return {};
}

void CodebaseContextAnalyzer::indexCodebase() {
    if (m_projectRoot.empty()) return;
    
    // Walk through all source files
    for (const auto& entry : fs::recursive_directory_iterator(m_projectRoot)) {
        if (!entry.is_regular_file()) continue;
        
        std::string ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".cc" || ext == ".hpp" ||
            ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".java") {
            
            indexFile(entry.path().string());
        }
    }
    
    buildDependencyGraph();
    m_indexed = true;
}

std::vector<Symbol> CodebaseContextAnalyzer::parseSymbols(const std::string& filePath) {
    std::vector<Symbol> symbols;
    
    std::ifstream file(filePath);
    if (!file.is_open()) return symbols;
    
    std::string line;
    int lineNum = 0;
    
    // Simple regex-based parsing for demonstration
    std::regex funcPattern(R"(^(?:void|int|bool|auto|std::\w+)\s+(\w+)\s*\()");
    std::regex classPattern(R"(^class\s+(\w+))");
    std::regex varPattern(R"(^(?:int|float|double|bool|std::\w+)\s+(\w+))");
    
    while (std::getline(file, line)) {
        lineNum++;
        
        std::smatch match;
        
        // Check for function
        if (std::regex_search(line, match, funcPattern)) {
            Symbol sym;
            sym.name = match[1].str();
            sym.type = "function";
            sym.filePath = filePath;
            sym.lineNumber = lineNum;
            sym.signature = line;
            symbols.push_back(sym);
        }
        // Check for class
        else if (std::regex_search(line, match, classPattern)) {
            Symbol sym;
            sym.name = match[1].str();
            sym.type = "class";
            sym.filePath = filePath;
            sym.lineNumber = lineNum;
            symbols.push_back(sym);
        }
    }
    
    return symbols;
}

ScopeInfo CodebaseContextAnalyzer::parseScope(
    const std::string& filePath, int lineNumber, int columnNumber) {
    
    ScopeInfo info;
    
    // Read file
    std::ifstream file(filePath);
    if (!file.is_open()) return info;
    
    std::string line;
    int currentLine = 0;
    int braceDepth = 0;
    std::string currentFunction;
    std::string currentClass;
    
    while (std::getline(file, line)) {
        currentLine++;
        
        // Track brace depth for scope
        for (char c : line) {
            if (c == '{') braceDepth++;
            else if (c == '}') braceDepth--;
        }
        
        // Extract function/class names
        if (line.find("class ") != std::string::npos) {
            std::regex pattern(R"(class\s+(\w+))");
            std::smatch match;
            if (std::regex_search(line, match, pattern)) {
                currentClass = match[1].str();
            }
        }
        
        if (line.find("void ") != std::string::npos || 
            line.find("int ") != std::string::npos) {
            std::regex pattern(R"((?:void|int|bool)\s+(\w+)\s*\()");
            std::smatch match;
            if (std::regex_search(line, match, pattern)) {
                currentFunction = match[1].str();
            }
        }
        
        if (currentLine == lineNumber) {
            info.currentFunction = currentFunction;
            info.currentClass = currentClass;
            break;
        }
    }
    
    return info;
}

void CodebaseContextAnalyzer::buildDependencyGraph() {
    // Build include relationships
    for (const auto& [filepath, info] : m_fileIndex) {
        for (const auto& imp : info.imports) {
            m_analysis.dependencies[filepath].push_back(imp);
            m_analysis.reverseDependencies[imp].push_back(filepath);
        }
    }
}

void CodebaseContextAnalyzer::indexFile(const std::string& filePath) {
    FileInfo info;
    info.path = filePath;
    info.symbols = parseSymbols(filePath);
    info.lastIndexed = std::chrono::system_clock::now();
    
    // Extract imports
    std::ifstream file(filePath);
    if (file.is_open()) {
        std::string line;
        std::regex importPattern(R"(#include\s+[<\"]([^>\"]+)[>\"])");
        std::smatch match;
        
        while (std::getline(file, line)) {
            if (std::regex_search(line, match, importPattern)) {
                info.imports.push_back(match[1].str());
            }
        }
    }
    
    // Add to index
    m_fileIndex[filePath] = info;
    
    // Add symbols to symbol table
    for (const auto& sym : info.symbols) {
        m_symbolTable[sym.name] = sym;
    }
}

Symbol CodebaseContextAnalyzer::getSymbolAtPosition(
    const std::string& filePath, int lineNumber, int columnNumber) {
    
    ScopeInfo scope = parseScope(filePath, lineNumber, columnNumber);
    Symbol result;
    result.name = scope.currentFunction.empty() ? scope.currentClass : scope.currentFunction;
    
    return result;
}

std::vector<Symbol> CodebaseContextAnalyzer::semanticSearch(
    const std::string& query, int maxResults) {
    
    return getRelevantSymbols(query, "", maxResults);
}

std::vector<std::pair<std::string, int>> CodebaseContextAnalyzer::findCodePatterns(
    const std::string& pattern, const std::string& language) {
    
    std::vector<std::pair<std::string, int>> results;
    
    // Search for pattern in all files
    for (const auto& [filepath, info] : m_fileIndex) {
        // Simple substring search for now
        std::ifstream file(filepath);
        if (file.is_open()) {
            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                if (line.find(pattern) != std::string::npos) {
                    results.push_back({filepath, lineNum});
                }
            }
        }
    }
    
    return results;
}

} // namespace IDE
} // namespace RawrXD
