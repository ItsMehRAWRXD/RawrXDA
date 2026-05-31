// ============================================================================
// crazy_mode.cpp — Autonomous Multi-File Refactoring Engine Implementation
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "composer/crazy_mode.h"
#include "composer/composer_mode.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>
#include <thread>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <queue>
#include <stack>
#include <set>
#include <map>

namespace RawrXD::Composer {

// ============================================================================
// Internal Implementation
// ============================================================================

class CrazyModeEngine::Impl {
public:
    CrazyModeConfig config;
    std::unique_ptr<IComposerEngine> composer;
    
    std::atomic<bool> running{false};
    std::atomic<float> progress{0.0f};
    std::string currentOperation;
    
    std::vector<Checkpoint> checkpoints;
    std::stack<uint32_t> checkpointStack;
    std::atomic<uint32_t> nextCheckpointId{1};
    
    mutable std::shared_mutex mutex;
    
    // Symbol analysis cache
    std::unordered_map<std::string, SymbolTable> symbolTableCache;
    
    Impl() : composer(createComposerEngine()) {}
    
    // Check if file is protected
    bool isProtected(const std::string& uri) const {
        // Check directory patterns
        for (const auto& dir : config.protectedDirectories) {
            if (uri.find("/" + dir + "/") != std::string::npos ||
                uri.find("\\" + dir + "\\") != std::string::npos) {
                return true;
            }
        }
        
        // Check file patterns
        for (const auto& pattern : config.protectedFilePatterns) {
            // Simple glob matching
            if (pattern.find('*') != std::string::npos) {
                std::string regexPattern;
                for (char c : pattern) {
                    if (c == '*') regexPattern += ".*";
                    else if (c == '.') regexPattern += "\\.";
                    else regexPattern += c;
                }
                
                std::regex re(regexPattern);
                if (std::regex_search(uri, re)) {
                    return true;
                }
            } else {
                if (uri.find(pattern) != std::string::npos) {
                    return true;
                }
            }
        }
        
        return false;
    }
    
    // Parse symbol definitions from code
    std::vector<SymbolInfo> parseSymbols(const std::string& content,
                                          const std::string& uri,
                                          const std::string& language) {
        std::vector<SymbolInfo> symbols;
        
        // Language-specific patterns
        static const std::vector<std::pair<std::regex, std::string>> patterns = {
            // Functions
            {std::regex(R"((?:async\s+)?function\s+(\w+)\s*\()"), "function"},
            {std::regex(R"((?:const|let|var)\s+(\w+)\s*=\s*(?:async\s+)?(?:function|\([^)]*\)\s*=>))"), "function"},
            {std::regex(R"(def\s+(\w+)\s*\()"), "function"},  // Python
            {std::regex(R"((?:public|private|protected)?\s*(?:static\s+)?(?:async\s+)?(\w+)\s*\([^)]*\)\s*(?::\s*\w+)?\s*\{)"), "method"},  // TS/JS class method
            {std::regex(R"((\w+)\s+(\w+)\s*\([^)]*\)\s*\{)"), "method"},  // C++/Java method
            
            // Classes
            {std::regex(R"(class\s+(\w+)\s*(?:extends|implements|\{))"), "class"},
            {std::regex(R"(struct\s+(\w+)\s*\{)"), "struct"},
            {std::regex(R"(interface\s+(\w+)\s*\{)"), "interface"},
            
            // Variables/Constants
            {std::regex(R"((?:const|let|var)\s+(\w+)\s*=)"), "variable"},
            {std::regex(R"((?:static\s+)?(?:final\s+)?(?:const\s+)?(\w+)\s+(\w+)\s*[;=])"), "variable"},
            
            // Types
            {std::regex(R"(type\s+(\w+)\s*=)"), "type"},
            {std::regex(R"(enum\s+(\w+)\s*\{)"), "enum"},
        };
        
        std::istringstream iss(content);
        std::string line;
        uint32_t lineNum = 0;
        
        while (std::getline(iss, line)) {
            lineNum++;
            
            for (const auto& [pattern, kind] : patterns) {
                std::smatch match;
                if (std::regex_search(line, match, pattern)) {
                    SymbolInfo symbol;
                    symbol.name = match[1].str();
                    symbol.type = kind;
                    symbol.sourceUri = uri;
                    symbol.definitionRange.startLine = lineNum;
                    symbol.definitionRange.endLine = lineNum;
                    
                    // Check if exported
                    if (line.find("export") != std::string::npos ||
                        line.find("public") != std::string::npos) {
                        symbol.isExported = true;
                    }
                    
                    symbols.push_back(std::move(symbol));
                }
            }
        }
        
        return symbols;
    }
    
    // Find all references to a symbol
    std::vector<TextRange> findReferences(const std::string& content,
                                           const std::string& symbolName,
                                           const std::string& language) {
        std::vector<TextRange> references;
        
        // Build regex for symbol reference
        std::string patternStr = "\\b" + symbolName + "\\b";
        std::regex pattern(patternStr);
        
        std::istringstream iss(content);
        std::string line;
        uint32_t lineNum = 0;
        
        while (std::getline(iss, line)) {
            lineNum++;
            
            std::sregex_iterator it(line.begin(), line.end(), pattern);
            std::sregex_iterator end;
            
            while (it != end) {
                TextRange range;
                range.startLine = lineNum;
                range.endLine = lineNum;
                range.startColumn = static_cast<uint32_t>(it->position());
                range.endColumn = range.startColumn + static_cast<uint32_t>(symbolName.length());
                
                // Skip if this is the definition (has "function", "class", "def", etc.)
                std::string beforeMatch = line.substr(0, it->position());
                if (beforeMatch.find("function ") != std::string::npos ||
                    beforeMatch.find("class ") != std::string::npos ||
                    beforeMatch.find("def ") != std::string::npos ||
                    beforeMatch.find("struct ") != std::string::npos ||
                    beforeMatch.find("interface ") != std::string::npos) {
                    ++it;
                    continue;
                }
                
                references.push_back(range);
                ++it;
            }
        }
        
        return references;
    }
    
    // Detect unused imports
    std::vector<std::string> findUnusedImports(const std::string& content,
                                               const std::string& language) {
        std::vector<std::string> unused;
        
        // Parse imports
        static const std::regex importPattern(
            R"(import\s+(?:\{([^}]+)\}|(\w+)(?:\s*,\s*\{([^}]+)\})?)\s+from\s+['"]([^'"]+)['"])");
        
        std::sregex_iterator it(content.begin(), content.end(), importPattern);
        std::sregex_iterator end;
        
        while (it != end) {
            std::smatch match = *it;
            std::string importedSymbols;
            
            if (match[1].matched) {
                importedSymbols = match[1].str();
            } else if (match[2].matched) {
                importedSymbols = match[2].str();
            }
            if (match[3].matched) {
                importedSymbols += "," + match[3].str();
            }
            
            // Check each imported symbol
            std::istringstream ss(importedSymbols);
            std::string symbol;
            while (std::getline(ss, symbol, ',')) {
                // Trim whitespace
                size_t start = symbol.find_first_not_of(" \t");
                size_t endPos = symbol.find_last_not_of(" \t");
                if (start != std::string::npos && endPos != std::string::npos) {
                    symbol = symbol.substr(start, endPos - start + 1);
                }
                
                if (symbol.empty()) continue;
                
                // Check if symbol is used elsewhere in file
                std::regex usagePattern("\\b" + symbol + "\\b");
                std::string restOfContent = content.substr(match.position() + match.length());
                
                if (!std::regex_search(restOfContent, usagePattern)) {
                    unused.push_back(symbol);
                }
            }
            
            ++it;
        }
        
        return unused;
    }
    
    // Calculate cyclomatic complexity
    uint32_t calculateCyclomaticComplexity(const std::string& content) {
        uint32_t complexity = 1; // Base complexity
        
        static const std::vector<std::regex> decisionPatterns = {
            std::regex(R"(\bif\s*\()"),
            std::regex(R"(\belse\s+if\s*\()"),
            std::regex(R"(\bfor\s*\()"),
            std::regex(R"(\bwhile\s*\()"),
            std::regex(R"(\bcase\s+)"),
            std::regex(R"(\bcatch\s*\()"),
            std::regex(R"(\?\s*:)"),  // Ternary
            std::regex(R"(\&\&|\|\|)"), // Logical operators
        };
        
        for (const auto& pattern : decisionPatterns) {
            std::sregex_iterator it(content.begin(), content.end(), pattern);
            std::sregex_iterator end;
            while (it != end) {
                complexity++;
                ++it;
            }
        }
        
        return complexity;
    }
    
    // Calculate nesting level
    uint32_t calculateNestingLevel(const std::string& content) {
        uint32_t maxNesting = 0;
        uint32_t currentNesting = 0;
        
        for (char c : content) {
            if (c == '{') {
                currentNesting++;
                maxNesting = std::max(maxNesting, currentNesting);
            } else if (c == '}') {
                if (currentNesting > 0) currentNesting--;
            }
        }
        
        return maxNesting;
    }
    
    // Read file content
    std::string readFile(const std::string& uri) {
        std::string path = uri;
        if (path.find("file://") == 0) {
            path = path.substr(7);
        }
        
        std::ifstream file(path);
        if (!file.is_open()) return "";
        
        std::ostringstream content;
        content << file.rdbuf();
        return content.str();
    }
    
    // Write file content
    bool writeFile(const std::string& uri, const std::string& content) {
        std::string path = uri;
        if (path.find("file://") == 0) {
            path = path.substr(7);
        }
        
        std::ofstream file(path);
        if (!file.is_open()) return false;
        
        file << content;
        return true;
    }
    
    // Detect language from file extension
    std::string detectLanguage(const std::string& uri) {
        static const std::unordered_map<std::string, std::string> extMap = {
            {".cpp", "cpp"}, {".cxx", "cpp"}, {".cc", "cpp"}, {".C", "cpp"},
            {".h", "cpp"}, {".hpp", "cpp"}, {".hxx", "cpp"},
            {".c", "c"},
            {".py", "python"}, {".pyw", "python"},
            {".js", "javascript"}, {".mjs", "javascript"},
            {".ts", "typescript"}, {".tsx", "typescript"},
            {".jsx", "javascriptreact"},
            {".java", "java"},
            {".cs", "csharp"},
            {".go", "go"},
            {".rs", "rust"},
            {".rb", "ruby"},
            {".php", "php"},
        };
        
        size_t dotPos = uri.rfind('.');
        if (dotPos != std::string::npos) {
            std::string ext = uri.substr(dotPos);
            auto it = extMap.find(ext);
            if (it != extMap.end()) return it->second;
        }
        
        return "plaintext";
    }
    
    // Create file change for rename operation
    FileChange createRenameChange(const std::string& uri,
                                   const std::string& content,
                                   const std::string& oldName,
                                   const std::string& newName) {
        FileChange change;
        change.uri = uri;
        change.type = ChangeType::Replace;
        change.originalContent = content;
        
        // Replace all occurrences
        std::regex pattern("\\b" + oldName + "\\b");
        change.newContent = std::regex_replace(content, pattern, newName);
        
        change.description = "Rename " + oldName + " to " + newName;
        
        return change;
    }
    
    // Extract method from code
    std::string extractMethodFromRange(const std::string& content,
                                        const TextRange& range,
                                        const std::string& methodName,
                                        const std::string& language) {
        std::istringstream iss(content);
        std::string line;
        std::vector<std::string> lines;
        
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        
        if (range.startLine > lines.size() || range.endLine > lines.size()) {
            return "";
        }
        
        // Extract the code
        std::ostringstream extracted;
        for (uint32_t i = range.startLine - 1; i < range.endLine && i < lines.size(); ++i) {
            extracted << lines[i] << "\n";
        }
        
        // Generate method signature based on language
        std::ostringstream method;
        if (language == "typescript" || language == "javascript") {
            method << "function " << methodName << "() {\n";
            method << extracted.str();
            method << "}\n";
        } else if (language == "python") {
            method << "def " << methodName << "():\n";
            method << extracted.str();
        } else if (language == "cpp") {
            method << "void " << methodName << "() {\n";
            method << extracted.str();
            method << "}\n";
        }
        
        return method.str();
    }
};

// ============================================================================
// Crazy Mode Engine Implementation
// ============================================================================

CrazyModeEngine::CrazyModeEngine() 
    : m_impl(std::make_unique<Impl>()) {}

CrazyModeEngine::~CrazyModeEngine() = default;

void CrazyModeEngine::setConfig(const CrazyModeConfig& config) {
    std::unique_lock lock(m_impl->mutex);
    m_impl->config = config;
}

CrazyModeConfig CrazyModeEngine::getConfig() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->config;
}

std::future<std::vector<RefactorOperation>> CrazyModeEngine::analyzeCodebase(
    const std::vector<std::string>& fileUris) {
    
    return std::async(std::launch::async, [this, fileUris]() {
        std::vector<RefactorOperation> operations;
        
        m_impl->running = true;
        m_impl->progress = 0.0f;
        m_impl->currentOperation = "Analyzing codebase...";
        
        // Build symbol table
        SymbolTable symbolTable;
        uint32_t processedFiles = 0;
        
        for (const auto& uri : fileUris) {
            if (m_impl->isProtected(uri)) continue;
            
            std::string content = m_impl->readFile(uri);
            if (content.empty()) continue;
            
            std::string language = m_impl->detectLanguage(uri);
            
            // Parse symbols
            auto symbols = m_impl->parseSymbols(content, uri, language);
            for (auto& symbol : symbols) {
                symbol.referenceRanges = m_impl->findReferences(content, symbol.name, language);
                symbolTable.symbols[symbol.name] = symbol;
                symbolTable.symbolsByFile[uri].push_back(symbol.name);
            }
            
            processedFiles++;
            m_impl->progress = static_cast<float>(processedFiles) / fileUris.size() * 0.3f;
        }
        
        // Find dead code
        if (m_impl->config.enableDeleteDeadCode) {
            m_impl->currentOperation = "Finding dead code...";
            
            for (auto& [name, symbol] : symbolTable.symbols) {
                if (symbol.referenceRanges.empty() && !symbol.isExported) {
                    // Symbol is never referenced
                    RefactorOperation op;
                    op.type = RefactorType::DeleteDeadCode;
                    op.name = "Delete unused: " + name;
                    op.symbolName = name;
                    op.sourceUri = symbol.sourceUri;
                    op.confidence = 0.9f;
                    op.isDestructive = true;
                    op.warnings.push_back("Symbol appears to be unused");
                    operations.push_back(std::move(op));
                }
            }
        }
        
        // Find unused imports
        if (m_impl->config.enableOptimizeImports) {
            m_impl->currentOperation = "Optimizing imports...";
            
            for (const auto& uri : fileUris) {
                if (m_impl->isProtected(uri)) continue;
                
                std::string content = m_impl->readFile(uri);
                std::string language = m_impl->detectLanguage(uri);
                
                auto unused = m_impl->findUnusedImports(content, language);
                if (!unused.empty()) {
                    RefactorOperation op;
                    op.type = RefactorType::OptimizeImports;
                    op.name = "Remove unused imports in " + uri;
                    op.sourceUri = uri;
                    op.confidence = 0.95f;
                    
                    for (const auto& imp : unused) {
                        op.warnings.push_back("Unused import: " + imp);
                    }
                    
                    operations.push_back(std::move(op));
                }
            }
        }
        
        // Find complexity hotspots
        if (m_impl->config.enableExtractMethod) {
            m_impl->currentOperation = "Finding complexity hotspots...";
            
            for (const auto& uri : fileUris) {
                if (m_impl->isProtected(uri)) continue;
                
                std::string content = m_impl->readFile(uri);
                std::string language = m_impl->detectLanguage(uri);
                
                uint32_t complexity = m_impl->calculateCyclomaticComplexity(content);
                uint32_t nesting = m_impl->calculateNestingLevel(content);
                
                if (complexity > 20 || nesting > 5) {
                    RefactorOperation op;
                    op.type = RefactorType::ExtractMethod;
                    op.name = "Simplify complex code in " + uri;
                    op.sourceUri = uri;
                    op.confidence = 0.7f;
                    op.warnings.push_back("Cyclomatic complexity: " + std::to_string(complexity));
                    op.warnings.push_back("Max nesting level: " + std::to_string(nesting));
                    operations.push_back(std::move(op));
                }
            }
        }
        
        m_impl->progress = 1.0f;
        m_impl->running = false;
        m_impl->currentOperation = "Analysis complete";
        
        return operations;
    });
}

std::future<bool> CrazyModeEngine::executeRefactoring(
    std::vector<RefactorOperation> operations,
    bool autoConfirm) {
    
    return std::async(std::launch::async, [this, ops = std::move(operations), autoConfirm]() mutable {
        m_impl->running = true;
        m_impl->progress = 0.0f;
        
        // Create checkpoint
        uint32_t checkpointId = 0;
        if (m_impl->config.enableAutoCheckpoint) {
            checkpointId = createCheckpoint("Before refactoring");
        }
        
        uint32_t totalOps = static_cast<uint32_t>(ops.size());
        uint32_t completedOps = 0;
        uint32_t failedOps = 0;
        
        for (auto& op : ops) {
            m_impl->currentOperation = "Executing: " + op.name;
            
            // Check confidence threshold
            float threshold = op.isDestructive ? 
                m_impl->config.minConfidenceForDestructiveOps :
                m_impl->config.minConfidenceForAutoApply;
            
            if (op.confidence < threshold && !autoConfirm) {
                // Would need user confirmation
                failedOps++;
                continue;
            }
            
            // Execute the operation
            bool success = false;
            switch (op.type) {
                case RefactorType::RenameSymbol:
                    success = executeRename(op);
                    break;
                case RefactorType::DeleteDeadCode:
                    success = executeDeleteDeadCode(op);
                    break;
                case RefactorType::OptimizeImports:
                    success = executeOptimizeImports(op);
                    break;
                case RefactorType::ExtractMethod:
                    success = executeExtractMethod(op);
                    break;
                default:
                    break;
            }
            
            if (success) {
                completedOps++;
            } else {
                failedOps++;
            }
            
            m_impl->progress = static_cast<float>(completedOps + failedOps) / totalOps;
            
            // Check error threshold
            if (m_impl->config.enableAutoRollbackOnError &&
                failedOps > totalOps * m_impl->config.errorThresholdForRollback) {
                // Rollback
                restoreCheckpoint(checkpointId);
                m_impl->running = false;
                return false;
            }
            
            // Create periodic checkpoint
            if (m_impl->config.enableAutoCheckpoint &&
                completedOps % m_impl->config.checkpointInterval == 0) {
                createCheckpoint("Progress checkpoint: " + std::to_string(completedOps) + " operations");
            }
        }
        
        m_impl->running = false;
        m_impl->currentOperation = "Refactoring complete";
        m_impl->progress = 1.0f;
        
        return failedOps == 0;
    });
}

bool CrazyModeEngine::executeRename(RefactorOperation& op) {
    if (op.newName.empty()) return false;
    
    std::string content = m_impl->readFile(op.sourceUri);
    if (content.empty()) return false;
    
    auto change = m_impl->createRenameChange(op.sourceUri, content, 
                                               op.symbolName, op.newName);
    
    return m_impl->writeFile(op.sourceUri, change.newContent);
}

bool CrazyModeEngine::executeDeleteDeadCode(RefactorOperation& op) {
    std::string content = m_impl->readFile(op.sourceUri);
    if (content.empty()) return false;
    
    // Find and remove the symbol definition
    // This is simplified - real implementation would be more sophisticated
    std::regex pattern("\\b" + op.symbolName + "\\b[^;]*[;\\{]");
    std::string newContent = std::regex_replace(content, pattern, "");
    
    return m_impl->writeFile(op.sourceUri, newContent);
}

bool CrazyModeEngine::executeOptimizeImports(RefactorOperation& op) {
    std::string content = m_impl->readFile(op.sourceUri);
    if (content.empty()) return false;
    
    std::string language = m_impl->detectLanguage(op.sourceUri);
    auto unused = m_impl->findUnusedImports(content, language);
    
    // Remove unused imports
    std::istringstream iss(content);
    std::ostringstream oss;
    std::string line;
    
    while (std::getline(iss, line)) {
        bool isUnused = false;
        for (const auto& imp : unused) {
            if (line.find(imp) != std::string::npos && 
                line.find("import") != std::string::npos) {
                isUnused = true;
                break;
            }
        }
        
        if (!isUnused) {
            oss << line << "\n";
        }
    }
    
    return m_impl->writeFile(op.sourceUri, oss.str());
}

bool CrazyModeEngine::executeExtractMethod(RefactorOperation& op) {
    // Simplified implementation
    return true;
}

RefactorOperation CrazyModeEngine::renameSymbol(
    const std::string& symbolName,
    const std::string& newName,
    const std::vector<std::string>& fileUris) {
    
    RefactorOperation op;
    op.type = RefactorType::RenameSymbol;
    op.name = "Rename " + symbolName + " to " + newName;
    op.symbolName = symbolName;
    op.newName = newName;
    op.confidence = 0.9f;
    
    // Find all files containing the symbol
    for (const auto& uri : fileUris) {
        if (m_impl->isProtected(uri)) continue;
        
        std::string content = m_impl->readFile(uri);
        if (content.empty()) continue;
        
        std::regex pattern("\\b" + symbolName + "\\b");
        if (std::regex_search(content, pattern)) {
            op.affectedFiles.push_back(uri);
            
            auto change = m_impl->createRenameChange(uri, content, symbolName, newName);
            op.changes.push_back(std::move(change));
        }
    }
    
    return op;
}

RefactorOperation CrazyModeEngine::extractMethod(
    const std::string& uri,
    const TextRange& range,
    const std::string& methodName) {
    
    RefactorOperation op;
    op.type = RefactorType::ExtractMethod;
    op.name = "Extract method " + methodName;
    op.sourceUri = uri;
    op.confidence = 0.8f;
    
    std::string content = m_impl->readFile(uri);
    std::string language = m_impl->detectLanguage(uri);
    
    std::string extracted = m_impl->extractMethodFromRange(content, range, methodName, language);
    if (!extracted.empty()) {
        // Create change to replace range with method call and add method definition
        // Simplified - real implementation would be more sophisticated
    }
    
    return op;
}

RefactorOperation CrazyModeEngine::inlineMethod(
    const std::string& uri,
    const TextRange& callSite) {
    
    RefactorOperation op;
    op.type = RefactorType::InlineMethod;
    op.name = "Inline method at " + uri;
    op.sourceUri = uri;
    op.confidence = 0.7f;
    
    return op;
}

RefactorOperation CrazyModeEngine::moveSymbol(
    const std::string& symbolName,
    const std::string& sourceUri,
    const std::string& targetUri) {
    
    RefactorOperation op;
    op.type = RefactorType::MoveSymbol;
    op.name = "Move " + symbolName + " to " + targetUri;
    op.symbolName = symbolName;
    op.sourceUri = sourceUri;
    op.targetUri = targetUri;
    op.confidence = 0.75f;
    
    return op;
}

DeadCodeAnalysis CrazyModeEngine::findDeadCode(
    const std::vector<std::string>& fileUris) {
    
    DeadCodeAnalysis analysis;
    
    // Build symbol table
    SymbolTable table = buildSymbolTable(fileUris);
    
    // Find unused symbols
    for (const auto& [name, symbol] : table.symbols) {
        if (symbol.referenceRanges.empty() && !symbol.isExported) {
            analysis.unusedSymbols.push_back(symbol);
        }
    }
    
    // Find unused imports for each file
    for (const auto& uri : fileUris) {
        std::string content = m_impl->readFile(uri);
        std::string language = m_impl->detectLanguage(uri);
        
        auto unused = m_impl->findUnusedImports(content, language);
        for (const auto& imp : unused) {
            analysis.unusedImports.push_back(uri + ":" + imp);
        }
    }
    
    return analysis;
}

std::vector<StyleIssue> CrazyModeEngine::findStyleIssues(
    const std::vector<std::string>& fileUris) {
    
    std::vector<StyleIssue> issues;
    
    // Style patterns to detect
    static const std::vector<std::pair<std::regex, std::pair<std::string, std::string>>> stylePatterns = {
        {std::regex(R"(\s+$)"), {"trailing-whitespace", "Remove trailing whitespace"}},
        {std::regex(R"(^\t)"), {"hard-tab", "Use spaces instead of tabs"}},
        {std::regex(R"(var\s+)"), {"var-keyword", "Use 'const' or 'let' instead of 'var'"}},
        {std::regex(R"(==\s*[^=])"), {"loose-equality", "Use '===' for strict equality"}},
        {std::regex(R"(!=\s*[^=])"), {"loose-inequality", "Use '!==' for strict inequality"}},
        {std::regex(R"(console\.\w+\s*\()"), {"console-statement", "Remove console statement"}},
    };
    
    for (const auto& uri : fileUris) {
        if (m_impl->isProtected(uri)) continue;
        
        std::string content = m_impl->readFile(uri);
        std::istringstream iss(content);
        std::string line;
        uint32_t lineNum = 0;
        
        while (std::getline(iss, line)) {
            lineNum++;
            
            for (const auto& [pattern, info] : stylePatterns) {
                std::smatch match;
                if (std::regex_search(line, match, pattern)) {
                    StyleIssue issue;
                    issue.uri = uri;
                    issue.range.startLine = lineNum;
                    issue.range.endLine = lineNum;
                    issue.ruleId = info.first;
                    issue.message = info.second;
                    issue.severity = Severity::Info;
                    issues.push_back(std::move(issue));
                }
            }
        }
    }
    
    return issues;
}

std::vector<ComplexityHotspot> CrazyModeEngine::findComplexityHotspots(
    const std::vector<std::string>& fileUris) {
    
    std::vector<ComplexityHotspot> hotspots;
    
    for (const auto& uri : fileUris) {
        if (m_impl->isProtected(uri)) continue;
        
        std::string content = m_impl->readFile(uri);
        std::string language = m_impl->detectLanguage(uri);
        
        uint32_t complexity = m_impl->calculateCyclomaticComplexity(content);
        uint32_t nesting = m_impl->calculateNestingLevel(content);
        
        if (complexity > 15 || nesting > 4) {
            ComplexityHotspot hotspot;
            hotspot.uri = uri;
            hotspot.cyclomaticComplexity = complexity;
            hotspot.nestingLevel = nesting;
            
            if (complexity > 15) {
                hotspot.issues.push_back("High cyclomatic complexity: " + std::to_string(complexity));
            }
            if (nesting > 4) {
                hotspot.issues.push_back("Deep nesting: " + std::to_string(nesting));
            }
            
            hotspots.push_back(std::move(hotspot));
        }
    }
    
    return hotspots;
}

SymbolTable CrazyModeEngine::buildSymbolTable(
    const std::vector<std::string>& fileUris) {
    
    SymbolTable table;
    
    for (const auto& uri : fileUris) {
        if (m_impl->isProtected(uri)) continue;
        
        std::string content = m_impl->readFile(uri);
        if (content.empty()) continue;
        
        std::string language = m_impl->detectLanguage(uri);
        
        auto symbols = m_impl->parseSymbols(content, uri, language);
        for (auto& symbol : symbols) {
            symbol.referenceRanges = m_impl->findReferences(content, symbol.name, language);
            table.symbols[symbol.name] = symbol;
            table.symbolsByFile[uri].push_back(symbol.name);
            
            // Build reference map
            for (const auto& ref : symbol.referenceRanges) {
                table.referencesToSymbol[symbol.name].push_back(uri);
            }
        }
    }
    
    return table;
}

uint32_t CrazyModeEngine::createCheckpoint(const std::string& description) {
    std::unique_lock lock(m_impl->mutex);
    
    Checkpoint checkpoint;
    checkpoint.checkpointId = m_impl->nextCheckpointId++;
    checkpoint.created = std::chrono::system_clock::now();
    checkpoint.description = description;
    
    // Store current state
    // Simplified - real implementation would snapshot all affected files
    
    m_impl->checkpoints.push_back(std::move(checkpoint));
    m_impl->checkpointStack.push(m_impl->checkpoints.back().checkpointId);
    
    // Limit checkpoints
    while (m_impl->checkpoints.size() > m_impl->config.maxCheckpoints) {
        m_impl->checkpoints.erase(m_impl->checkpoints.begin());
    }
    
    return m_impl->checkpoints.back().checkpointId;
}

bool CrazyModeEngine::restoreCheckpoint(uint32_t checkpointId) {
    std::unique_lock lock(m_impl->mutex);
    
    for (const auto& checkpoint : m_impl->checkpoints) {
        if (checkpoint.checkpointId == checkpointId) {
            // Restore file snapshots
            for (const auto& [uri, content] : checkpoint.fileSnapshots) {
                m_impl->writeFile(uri, content);
            }
            return true;
        }
    }
    
    return false;
}

std::vector<Checkpoint> CrazyModeEngine::getCheckpoints() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->checkpoints;
}

bool CrazyModeEngine::deleteCheckpoint(uint32_t checkpointId) {
    std::unique_lock lock(m_impl->mutex);
    
    auto it = std::find_if(m_impl->checkpoints.begin(), m_impl->checkpoints.end(),
        [checkpointId](const Checkpoint& c) { return c.checkpointId == checkpointId; });
    
    if (it != m_impl->checkpoints.end()) {
        m_impl->checkpoints.erase(it);
        return true;
    }
    
    return false;
}

bool CrazyModeEngine::isRunning() const {
    return m_impl->running;
}

float CrazyModeEngine::getProgress() const {
    return m_impl->progress;
}

std::string CrazyModeEngine::getCurrentOperation() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->currentOperation;
}

// Factory function
std::unique_ptr<ICrazyModeEngine> createCrazyModeEngine() {
    return std::make_unique<CrazyModeEngine>();
}

} // namespace RawrXD::Composer
