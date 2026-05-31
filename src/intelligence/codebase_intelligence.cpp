// ============================================================================
// codebase_intelligence.cpp — Context-Aware Codebase Intelligence Implementation
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "intelligence/codebase_intelligence.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <queue>
#include <stack>
#include <set>
#include <map>
#include <regex>
#include <thread>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

namespace RawrXD::Intelligence {

// ============================================================================
// Internal Implementation
// ============================================================================

class IntelligenceEngine::Impl {
public:
    IndexConfig config;
    IndexCallback callback;
    
    // Symbol storage
    std::unordered_map<std::string, SymbolDefinition> symbols;
    std::unordered_map<std::string, std::vector<std::string>> symbolsByName;
    std::unordered_map<std::string, std::vector<std::string>> symbolsByFile;
    
    // File storage
    std::unordered_map<std::string, FileInfo> files;
    
    // Reference storage
    std::unordered_map<std::string, std::vector<SymbolReference>> references;
    std::unordered_map<std::string, std::vector<std::string>> referencesToSymbol;
    
    // Dependency graph
    DependencyGraph dependencyGraph;
    
    // Index state
    std::atomic<bool> indexing{false};
    std::atomic<float> indexProgress{0.0f};
    std::atomic<uint32_t> nextSymbolId{1};
    
    mutable std::shared_mutex symbolsMutex;
    mutable std::shared_mutex filesMutex;
    mutable std::shared_mutex graphMutex;
    
    // Statistics
    IndexStats stats;
    
    Impl() = default;
    
    // Generate unique symbol ID
    std::string generateSymbolId(const std::string& name, const std::string& uri, uint32_t line) {
        return uri + ":" + std::to_string(line) + ":" + name;
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
            {".swift", "swift"},
            {".kt", "kotlin"}, {".kts", "kotlin"},
            {".scala", "scala"},
            {".lua", "lua"},
            {".r", "r"},
            {".sql", "sql"},
            {".sh", "shell"}, {".bash", "shell"},
            {".ps1", "powershell"},
            {".json", "json"},
            {".yaml", "yaml"}, {".yml", "yaml"},
            {".xml", "xml"},
            {".html", "html"}, {".htm", "html"},
            {".css", "css"}, {".scss", "scss"}, {".less", "less"},
            {".md", "markdown"},
        };
        
        size_t dotPos = uri.rfind('.');
        if (dotPos != std::string::npos) {
            std::string ext = uri.substr(dotPos);
            auto it = extMap.find(ext);
            if (it != extMap.end()) return it->second;
        }
        
        return "plaintext";
    }
    
    // Parse symbols from file content
    std::vector<SymbolDefinition> parseSymbols(const std::string& content,
                                                 const std::string& uri,
                                                 const std::string& language) {
        std::vector<SymbolDefinition> result;
        
        // Language-specific patterns
        static const std::vector<std::pair<std::regex, SymbolKind>> patterns = {
            // C++/C#/Java patterns
            {std::regex(R"((?:class|struct|interface)\s+(\w+)\s*(?::\s*[^{]*)?\{)"), SymbolKind::Class},
            {std::regex(R"(enum\s+(?:class\s+)?(\w+)\s*\{)"), SymbolKind::Enum},
            {std::regex(R"((?:typedef\s+)?using\s+(\w+)\s*=)"), SymbolKind::TypeAlias},
            
            // Function patterns
            {std::regex(R"((?:inline\s+)?(?:static\s+)?(?:explicit\s+)?(?:constexpr\s+)?(\w+)\s*\([^)]*\)\s*(?:const\s*)?(?:override\s*)?(?:final\s*)?\{)"), SymbolKind::Function},
            {std::regex(R"((\w+)\s*::\s*(\w+)\s*\([^)]*\)\s*\{)"), SymbolKind::Method},
            
            // JavaScript/TypeScript patterns
            {std::regex(R"(function\s+(\w+)\s*\()"), SymbolKind::Function},
            {std::regex(R"((?:const|let|var)\s+(\w+)\s*=\s*(?:async\s+)?(?:function|\([^)]*\)\s*=>))"), SymbolKind::Function},
            {std::regex(R"(class\s+(\w+)\s*(?:extends|implements|\{))"), SymbolKind::Class},
            {std::regex(R"(interface\s+(\w+)\s*\{)"), SymbolKind::Interface},
            {std::regex(R"(type\s+(\w+)\s*=)"), SymbolKind::TypeParameter},
            {std::regex(R"(enum\s+(\w+)\s*\{)"), SymbolKind::Enum},
            {std::regex(R"(export\s+(?:default\s+)?(?:function|class|const|let|var)\s+(\w+))"), SymbolKind::Unknown},
            
            // Python patterns
            {std::regex(R"(def\s+(\w+)\s*\()"), SymbolKind::Function},
            {std::regex(R"(class\s+(\w+)\s*[:\(])"), SymbolKind::Class},
            {std::regex(R"(@(\w+)\s*\n\s*def)"), SymbolKind::Annotation},
            
            // Go patterns
            {std::regex(R"(func\s+(?:\(\w+\s+\*?(\w+)\)\s+)?(\w+)\s*\()"), SymbolKind::Function},
            {std::regex(R"(type\s+(\w+)\s+(?:struct|interface))"), SymbolKind::Class},
            {std::regex(R"(type\s+(\w+)\s+\w+)"), SymbolKind::TypeAlias},
            
            // Rust patterns
            {std::regex(R"(fn\s+(\w+)\s*[<\(])"), SymbolKind::Function},
            {std::regex(R"(struct\s+(\w+)\s*(?:<|{))"), SymbolKind::Struct},
            {std::regex(R"(enum\s+(\w+)\s*(?:<|{))"), SymbolKind::Enum},
            {std::regex(R"(trait\s+(\w+)\s*\{)"), SymbolKind::Trait},
            {std::regex(R"(impl\s+(?:<[^>]+>\s+)?(\w+))"), SymbolKind::Class},
            
            // Variable patterns
            {std::regex(R"((?:const|let|var)\s+(\w+)\s*[=:])"), SymbolKind::Variable},
            {std::regex(R"((?:static\s+)?(?:final\s+)?(?:const\s+)?(\w+)\s+(\w+)\s*[;=])"), SymbolKind::Variable},
        };
        
        std::istringstream iss(content);
        std::string line;
        uint32_t lineNum = 0;
        
        while (std::getline(iss, line)) {
            lineNum++;
            
            for (const auto& [pattern, kind] : patterns) {
                std::smatch match;
                if (std::regex_search(line, match, pattern)) {
                    std::string name = match[1].str();
                    if (name.empty() && match.size() > 2) {
                        name = match[2].str();
                    }
                    
                    if (!name.empty() && name != "if" && name != "for" && 
                        name != "while" && name != "switch" && name != "return") {
                        
                        SymbolDefinition symbol;
                        symbol.id = generateSymbolId(name, uri, lineNum);
                        symbol.name = name;
                        symbol.kind = kind;
                        symbol.definition.uri = uri;
                        symbol.definition.line = lineNum;
                        symbol.definition.endLine = lineNum;
                        
                        // Check if exported
                        if (line.find("export") != std::string::npos ||
                            line.find("public") != std::string::npos) {
                            symbol.isExported = true;
                        }
                        
                        result.push_back(std::move(symbol));
                    }
                }
            }
        }
        
        return result;
    }
    
    // Parse imports from file content
    std::vector<FileImport> parseImports(const std::string& content,
                                          const std::string& language) {
        std::vector<FileImport> imports;
        
        static const std::vector<std::regex> patterns = {
            std::regex(R"(import\s+['"]([^'"]+)['"])"),                    // JS/TS
            std::regex(R"(import\s+\{([^}]+)\}\s+from\s+['"]([^'"]+)['"])"), // JS/TS named
            std::regex(R"(import\s+(\w+)\s+from\s+['"]([^'"]+)['"])"),     // JS/TS default
            std::regex(R"(require\s*\(\s*['"]([^'"]+)['"]\s*\))"),         // CommonJS
            std::regex(R"(from\s+['"]([^'"]+)['"]\s+import\s+(\w+))"),     // Python
            std::regex(R"(import\s+([\w.]+)"),                             // Python/Java
            std::regex(R"(#include\s*[<"]([^>"]+)[>"])"),                   // C/C++
            std::regex(R"(using\s+([\w.]+);)"),                            // C#
            std::regex(R"(use\s+([\w:]+);)"),                               // Rust
            std::regex(R"(package\s+([\w.]+))"),                           // Go/Java
        };
        
        for (const auto& pattern : patterns) {
            std::sregex_iterator it(content.begin(), content.end(), pattern);
            std::sregex_iterator end;
            
            while (it != end) {
                std::smatch match = *it;
                FileImport imp;
                
                if (match.size() > 2) {
                    // Named import with module
                    imp.module = match[2].str();
                    std::string names = match[1].str();
                    std::istringstream ss(names);
                    std::string name;
                    while (std::getline(ss, name, ',')) {
                        size_t start = name.find_first_not_of(" \t");
                        size_t endPos = name.find_last_not_of(" \t");
                        if (start != std::string::npos && endPos != std::string::npos) {
                            imp.symbols.push_back(name.substr(start, endPos - start + 1));
                        }
                    }
                } else {
                    imp.module = match[1].str();
                }
                
                imp.location.uri = "";
                imp.location.line = 0;
                
                imports.push_back(std::move(imp));
                ++it;
            }
        }
        
        return imports;
    }
    
    // Parse exports from file content
    std::vector<FileExport> parseExports(const std::string& content,
                                          const std::string& language) {
        std::vector<FileExport> exports;
        
        static const std::vector<std::regex> patterns = {
            std::regex(R"(export\s+(?:default\s+)?(?:function|class|const|let|var)\s+(\w+))"),
            std::regex(R"(export\s*\{\s*([^}]+)\s*\})"),
            std::regex(R"(export\s+default\s+(\w+))"),
        };
        
        for (const auto& pattern : patterns) {
            std::sregex_iterator it(content.begin(), content.end(), pattern);
            std::sregex_iterator end;
            
            while (it != end) {
                std::smatch match = *it;
                FileExport exp;
                exp.name = match[1].str();
                
                // Handle comma-separated exports
                if (exp.name.find(',') != std::string::npos) {
                    std::istringstream ss(exp.name);
                    std::string name;
                    while (std::getline(ss, name, ',')) {
                        size_t start = name.find_first_not_of(" \t");
                        size_t endPos = name.find_last_not_of(" \t");
                        if (start != std::string::npos && endPos != std::string::npos) {
                            FileExport subExp;
                            subExp.name = name.substr(start, endPos - start + 1);
                            exports.push_back(std::move(subExp));
                        }
                    }
                } else {
                    exports.push_back(std::move(exp));
                }
                
                ++it;
            }
        }
        
        return exports;
    }
    
    // Find references to a symbol
    std::vector<SymbolReference> findSymbolReferences(const std::string& symbolName,
                                                        const std::string& definitionUri,
                                                        const std::unordered_map<std::string, FileInfo>& files) {
        std::vector<SymbolReference> refs;
        
        std::regex pattern("\\b" + symbolName + "\\b");
        
        for (const auto& [uri, file] : files) {
            if (uri == definitionUri) continue;  // Skip definition file for now
            
            std::ifstream f(uri);
            if (!f.is_open()) continue;
            
            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            
            std::istringstream iss(content);
            std::string line;
            uint32_t lineNum = 0;
            
            while (std::getline(iss, line)) {
                lineNum++;
                
                std::sregex_iterator it(line.begin(), line.end(), pattern);
                std::sregex_iterator end;
                
                while (it != end) {
                    SymbolReference ref;
                    ref.symbolId = symbolName;
                    ref.location.uri = uri;
                    ref.location.line = lineNum;
                    ref.location.column = static_cast<uint32_t>(it->position());
                    ref.kind = ReferenceKind::Reference;
                    ref.context = line;
                    
                    refs.push_back(std::move(ref));
                    ++it;
                }
            }
        }
        
        return refs;
    }
    
    // Build dependency graph
    void buildDependencyGraph() {
        std::unique_lock lock(graphMutex);
        
        dependencyGraph = DependencyGraph();
        
        // Add nodes for all files
        for (const auto& [uri, file] : files) {
            DependencyNode node;
            node.id = uri;
            node.name = uri.substr(uri.find_last_of("/\\") + 1);
            dependencyGraph.nodes[uri] = node;
        }
        
        // Add edges
        for (const auto& [uri, file] : files) {
            for (const auto& dep : file.dependencies) {
                DependencyEdge edge;
                edge.from = uri;
                edge.to = dep;
                edge.kind = ReferenceKind::Import;
                edge.weight = 1;
                
                dependencyGraph.outgoing[uri].push_back(edge);
                dependencyGraph.incoming[dep].push_back(edge);
                
                dependencyGraph.nodes[uri].outDegree++;
                dependencyGraph.nodes[dep].inDegree++;
            }
        }
        
        // Compute topological order
        computeTopologicalOrder();
        
        // Find strongly connected components
        computeStronglyConnectedComponents();
        
        // Compute centrality measures
        computeCentrality();
    }
    
    // Compute topological order using Kahn's algorithm
    void computeTopologicalOrder() {
        std::unordered_map<std::string, uint32_t> inDegree;
        std::queue<std::string> queue;
        
        for (const auto& [id, node] : dependencyGraph.nodes) {
            inDegree[id] = node.inDegree;
            if (node.inDegree == 0) {
                queue.push(id);
            }
        }
        
        while (!queue.empty()) {
            std::string current = queue.front();
            queue.pop();
            
            dependencyGraph.topologicalOrder.push_back(current);
            
            auto it = dependencyGraph.outgoing.find(current);
            if (it != dependencyGraph.outgoing.end()) {
                for (const auto& edge : it->second) {
                    if (--inDegree[edge.to] == 0) {
                        queue.push(edge.to);
                    }
                }
            }
        }
    }
    
    // Find strongly connected components using Tarjan's algorithm
    void computeStronglyConnectedComponents() {
        std::unordered_map<std::string, uint32_t> index;
        std::unordered_map<std::string, uint32_t> lowlink;
        std::unordered_map<std::string, bool> onStack;
        std::stack<std::string> stack;
        uint32_t currentIndex = 0;
        
        std::function<void(const std::string&)> strongconnect = [&](const std::string& v) {
            index[v] = currentIndex;
            lowlink[v] = currentIndex;
            currentIndex++;
            stack.push(v);
            onStack[v] = true;
            
            auto it = dependencyGraph.outgoing.find(v);
            if (it != dependencyGraph.outgoing.end()) {
                for (const auto& edge : it->second) {
                    const std::string& w = edge.to;
                    
                    if (index.find(w) == index.end()) {
                        strongconnect(w);
                        lowlink[v] = std::min(lowlink[v], lowlink[w]);
                    } else if (onStack[w]) {
                        lowlink[v] = std::min(lowlink[v], index[w]);
                    }
                }
            }
            
            if (lowlink[v] == index[v]) {
                std::vector<std::string> component;
                std::string w;
                
                do {
                    w = stack.top();
                    stack.pop();
                    onStack[w] = false;
                    component.push_back(w);
                    dependencyGraph.componentIds[w] = 
                        static_cast<uint32_t>(dependencyGraph.stronglyConnectedComponents.size());
                } while (w != v);
                
                dependencyGraph.stronglyConnectedComponents.push_back(std::move(component));
            }
        };
        
        for (const auto& [id, node] : dependencyGraph.nodes) {
            if (index.find(id) == index.end()) {
                strongconnect(id);
            }
        }
    }
    
    // Compute centrality measures
    void computeCentrality() {
        // Simplified PageRank
        const float damping = 0.85f;
        const uint32_t iterations = 10;
        const float initialRank = 1.0f / dependencyGraph.nodes.size();
        
        std::unordered_map<std::string, float> ranks;
        for (const auto& [id, node] : dependencyGraph.nodes) {
            ranks[id] = initialRank;
        }
        
        for (uint32_t i = 0; i < iterations; ++i) {
            std::unordered_map<std::string, float> newRanks;
            
            for (const auto& [id, node] : dependencyGraph.nodes) {
                float rank = (1.0f - damping) / dependencyGraph.nodes.size();
                
                auto it = dependencyGraph.incoming.find(id);
                if (it != dependencyGraph.incoming.end()) {
                    for (const auto& edge : it->second) {
                        auto& fromNode = dependencyGraph.nodes[edge.from];
                        if (fromNode.outDegree > 0) {
                            rank += damping * ranks[edge.from] / fromNode.outDegree;
                        }
                    }
                }
                
                newRanks[id] = rank;
            }
            
            ranks = std::move(newRanks);
        }
        
        for (auto& [id, node] : dependencyGraph.nodes) {
            node.pageRank = ranks[id];
        }
    }
    
    // Calculate relevance score for context assembly
    float calculateRelevance(const std::string& symbolId,
                             const std::vector<std::string>& focusFiles,
                             const std::unordered_set<std::string>& focusSymbols) {
        float score = 0.0f;
        
        auto symIt = symbols.find(symbolId);
        if (symIt == symbols.end()) return 0.0f;
        
        const auto& symbol = symIt->second;
        
        // Direct focus
        if (focusSymbols.count(symbolId) > 0) {
            score += 1.0f;
        }
        
        // In focus files
        for (const auto& file : focusFiles) {
            if (symbol.definition.uri == file) {
                score += 0.8f;
                break;
            }
        }
        
        // Referenced by focus symbols
        for (const auto& focusSym : focusSymbols) {
            auto focusIt = symbols.find(focusSym);
            if (focusIt != symbols.end()) {
                for (const auto& dep : focusIt->second.dependencies) {
                    if (dep == symbolId) {
                        score += 0.6f;
                        break;
                    }
                }
            }
        }
        
        // PageRank score
        auto nodeIt = dependencyGraph.nodes.find(symbolId);
        if (nodeIt != dependencyGraph.nodes.end()) {
            score += nodeIt->second.pageRank * 0.3f;
        }
        
        // Reference count
        if (symbol.referenceCount > 0) {
            score += std::min(0.2f, symbol.referenceCount / 100.0f);
        }
        
        return score;
    }
    
    // Estimate token count
    uint32_t estimateTokens(const std::string& content) {
        // Rough estimate: ~4 characters per token
        return static_cast<uint32_t>(content.size() / 4);
    }
    
    // Read file content
    std::string readFile(const std::string& uri) {
        std::ifstream file(uri);
        if (!file.is_open()) return "";
        
        std::ostringstream content;
        content << file.rdbuf();
        return content.str();
    }
    
    // Check if file matches pattern
    bool matchesPattern(const std::string& path, const std::string& pattern) {
        // Simple glob matching
        if (pattern == "*") return true;
        
        std::string regexPattern;
        for (char c : pattern) {
            if (c == '*') regexPattern += ".*";
            else if (c == '?') regexPattern += ".";
            else if (c == '.') regexPattern += "\\.";
            else regexPattern += c;
        }
        
        try {
            std::regex re(regexPattern);
            return std::regex_search(path, re);
        } catch (...) {
            return false;
        }
    }
    
    // Check if file should be indexed
    bool shouldIndex(const std::string& uri) {
        // Check include patterns
        bool included = false;
        for (const auto& pattern : config.includePatterns) {
            if (matchesPattern(uri, pattern)) {
                included = true;
                break;
            }
        }
        
        if (!included) return false;
        
        // Check exclude patterns
        for (const auto& pattern : config.excludePatterns) {
            if (matchesPattern(uri, pattern)) {
                return false;
            }
        }
        
        return true;
    }
};

// ============================================================================
// Intelligence Engine Implementation
// ============================================================================

IntelligenceEngine::IntelligenceEngine() 
    : m_impl(std::make_unique<Impl>()) {}

IntelligenceEngine::~IntelligenceEngine() = default;

void IntelligenceEngine::setConfig(const IndexConfig& config) {
    std::unique_lock lock(m_impl->filesMutex);
    m_impl->config = config;
}

IndexConfig IntelligenceEngine::getConfig() const {
    std::shared_lock lock(m_impl->filesMutex);
    return m_impl->config;
}

bool IntelligenceEngine::indexWorkspace(const std::string& rootPath) {
    m_impl->indexing = true;
    m_impl->indexProgress = 0.0f;
    
    std::vector<std::string> filesToIndex;
    
    // Collect files to index
    try {
        for (const auto& entry : fs::recursive_directory_iterator(rootPath)) {
            if (entry.is_regular_file() && m_impl->shouldIndex(entry.path().string())) {
                filesToIndex.push_back(entry.path().string());
            }
        }
    } catch (...) {
        m_impl->indexing = false;
        return false;
    }
    
    uint32_t total = static_cast<uint32_t>(filesToIndex.size());
    uint32_t processed = 0;
    
    for (const auto& uri : filesToIndex) {
        if (!m_impl->indexing) break;  // Cancelled
        
        indexFile(uri);
        
        processed++;
        m_impl->indexProgress = static_cast<float>(processed) / total;
        
        if (m_impl->callback) {
            m_impl->callback(uri, true);
        }
    }
    
    // Build dependency graph
    m_impl->buildDependencyGraph();
    
    // Update stats
    m_impl->stats.totalFiles = m_impl->files.size();
    m_impl->stats.totalSymbols = m_impl->symbols.size();
    m_impl->stats.lastUpdated = std::chrono::system_clock::now();
    
    m_impl->indexing = false;
    m_impl->indexProgress = 1.0f;
    
    return true;
}

bool IntelligenceEngine::indexFile(const std::string& uri) {
    std::string content = m_impl->readFile(uri);
    if (content.empty()) return false;
    
    std::string language = m_impl->detectLanguage(uri);
    
    // Parse symbols
    auto parsedSymbols = m_impl->parseSymbols(content, uri, language);
    
    // Parse imports/exports
    auto imports = m_impl->parseImports(content, language);
    auto exports = m_impl->parseExports(content, language);
    
    // Create file info
    FileInfo fileInfo;
    fileInfo.uri = uri;
    fileInfo.language = language;
    fileInfo.lineCount = static_cast<uint32_t>(std::count(content.begin(), content.end(), '\n'));
    fileInfo.imports = imports;
    fileInfo.exports = exports;
    fileInfo.isIndexed = true;
    fileInfo.lastIndexed = std::chrono::system_clock::now();
    
    // Store symbols
    {
        std::unique_lock lock(m_impl->symbolsMutex);
        
        for (auto& symbol : parsedSymbols) {
            std::string id = symbol.id;
            m_impl->symbols[id] = std::move(symbol);
            m_impl->symbolsByName[symbol.name].push_back(id);
            m_impl->symbolsByFile[uri].push_back(id);
            
            fileInfo.definedSymbols.push_back(id);
            fileInfo.symbolCount++;
        }
    }
    
    // Resolve imports to file dependencies
    for (const auto& imp : imports) {
        // Try to resolve module to file
        std::string modulePath = imp.module;
        if (modulePath.find('.') == std::string::npos) {
            // Add common extensions
            for (const auto& ext : {".ts", ".js", ".py", ".cpp", ".h"}) {
                std::string testPath = rootPath + "/" + modulePath + ext;
                if (fs::exists(testPath)) {
                    modulePath = testPath;
                    break;
                }
                testPath = rootPath + "/" + modulePath + "/index" + ext;
                if (fs::exists(testPath)) {
                    modulePath = testPath;
                    break;
                }
            }
        }
        
        if (fs::exists(modulePath)) {
            fileInfo.dependencies.push_back(modulePath);
        }
    }
    
    fileInfo.importCount = static_cast<uint32_t>(imports.size());
    fileInfo.exportCount = static_cast<uint32_t>(exports.size());
    
    // Store file info
    {
        std::unique_lock lock(m_impl->filesMutex);
        m_impl->files[uri] = std::move(fileInfo);
    }
    
    return true;
}

bool IntelligenceEngine::removeFile(const std::string& uri) {
    std::unique_lock symLock(m_impl->symbolsMutex);
    std::unique_lock fileLock(m_impl->filesMutex);
    
    // Remove symbols
    auto it = m_impl->symbolsByFile.find(uri);
    if (it != m_impl->symbolsByFile.end()) {
        for (const auto& symbolId : it->second) {
            m_impl->symbols.erase(symbolId);
        }
        m_impl->symbolsByFile.erase(it);
    }
    
    // Remove file
    m_impl->files.erase(uri);
    
    return true;
}

bool IntelligenceEngine::updateFile(const std::string& uri) {
    removeFile(uri);
    return indexFile(uri);
}

bool IntelligenceEngine::reindexAll() {
    std::shared_lock lock(m_impl->filesMutex);
    
    std::vector<std::string> uris;
    for (const auto& [uri, file] : m_impl->files) {
        uris.push_back(uri);
    }
    
    for (const auto& uri : uris) {
        updateFile(uri);
    }
    
    m_impl->buildDependencyGraph();
    return true;
}

std::optional<SymbolDefinition> IntelligenceEngine::getSymbol(const std::string& symbolId) const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    auto it = m_impl->symbols.find(symbolId);
    if (it != m_impl->symbols.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<SymbolDefinition> IntelligenceEngine::findSymbols(const std::string& name) const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    std::vector<SymbolDefinition> result;
    
    auto it = m_impl->symbolsByName.find(name);
    if (it != m_impl->symbolsByName.end()) {
        for (const auto& id : it->second) {
            auto symIt = m_impl->symbols.find(id);
            if (symIt != m_impl->symbols.end()) {
                result.push_back(symIt->second);
            }
        }
    }
    
    return result;
}

std::vector<SymbolDefinition> IntelligenceEngine::findSymbolsByKind(SymbolKind kind) const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    std::vector<SymbolDefinition> result;
    
    for (const auto& [id, symbol] : m_impl->symbols) {
        if (symbol.kind == kind) {
            result.push_back(symbol);
        }
    }
    
    return result;
}

std::vector<SymbolReference> IntelligenceEngine::findReferences(const std::string& symbolId) const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    auto it = m_impl->references.find(symbolId);
    if (it != m_impl->references.end()) {
        return it->second;
    }
    return {};
}

std::vector<SymbolDefinition> IntelligenceEngine::findImplementations(const std::string& symbolId) const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    std::vector<SymbolDefinition> result;
    
    auto it = m_impl->symbols.find(symbolId);
    if (it != m_impl->symbols.end()) {
        for (const auto& implId : it->second.implementations) {
            auto implIt = m_impl->symbols.find(implId);
            if (implIt != m_impl->symbols.end()) {
                result.push_back(implIt->second);
            }
        }
    }
    
    return result;
}

std::optional<SymbolDefinition> IntelligenceEngine::findDefinition(const std::string& name,
                                                                     const std::string& contextUri) const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    auto it = m_impl->symbolsByName.find(name);
    if (it != m_impl->symbolsByName.end() && !it->second.empty()) {
        // Prefer definition in same file
        for (const auto& id : it->second) {
            auto symIt = m_impl->symbols.find(id);
            if (symIt != m_impl->symbols.end() && 
                symIt->second.definition.uri == contextUri) {
                return symIt->second;
            }
        }
        
        // Fall back to first definition
        auto symIt = m_impl->symbols.find(it->second[0]);
        if (symIt != m_impl->symbols.end()) {
            return symIt->second;
        }
    }
    
    return std::nullopt;
}

std::optional<FileInfo> IntelligenceEngine::getFile(const std::string& uri) const {
    std::shared_lock lock(m_impl->filesMutex);
    
    auto it = m_impl->files.find(uri);
    if (it != m_impl->files.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<FileInfo> IntelligenceEngine::getFiles(const std::string& pattern) const {
    std::shared_lock lock(m_impl->filesMutex);
    
    std::vector<FileInfo> result;
    
    for (const auto& [uri, file] : m_impl->files) {
        if (m_impl->matchesPattern(uri, pattern)) {
            result.push_back(file);
        }
    }
    
    return result;
}

std::vector<std::string> IntelligenceEngine::getDependencies(const std::string& uri) const {
    std::shared_lock lock(m_impl->filesMutex);
    
    auto it = m_impl->files.find(uri);
    if (it != m_impl->files.end()) {
        return it->second.dependencies;
    }
    return {};
}

std::vector<std::string> IntelligenceEngine::getDependents(const std::string& uri) const {
    std::shared_lock lock(m_impl->filesMutex);
    
    std::vector<std::string> result;
    
    for (const auto& [fileUri, file] : m_impl->files) {
        for (const auto& dep : file.dependencies) {
            if (dep == uri) {
                result.push_back(fileUri);
                break;
            }
        }
    }
    
    return result;
}

DependencyGraph IntelligenceEngine::getDependencyGraph() const {
    std::shared_lock lock(m_impl->graphMutex);
    return m_impl->dependencyGraph;
}

std::vector<std::string> IntelligenceEngine::getImportPath(const std::string& from,
                                                           const std::string& to) const {
    std::shared_lock lock(m_impl->graphMutex);
    
    // BFS to find shortest path
    std::unordered_map<std::string, std::string> parent;
    std::queue<std::string> queue;
    std::unordered_set<std::string> visited;
    
    queue.push(from);
    visited.insert(from);
    
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        
        if (current == to) {
            // Reconstruct path
            std::vector<std::string> path;
            std::string node = to;
            
            while (node != from) {
                path.push_back(node);
                node = parent[node];
            }
            path.push_back(from);
            
            std::reverse(path.begin(), path.end());
            return path;
        }
        
        auto it = m_impl->dependencyGraph.outgoing.find(current);
        if (it != m_impl->dependencyGraph.outgoing.end()) {
            for (const auto& edge : it->second) {
                if (visited.find(edge.to) == visited.end()) {
                    visited.insert(edge.to);
                    parent[edge.to] = current;
                    queue.push(edge.to);
                }
            }
        }
    }
    
    return {};  // No path found
}

std::vector<std::string> IntelligenceEngine::getAffectedFiles(const std::string& uri) const {
    std::shared_lock lock(m_impl->graphMutex);
    
    std::vector<std::string> result;
    std::unordered_set<std::string> visited;
    std::queue<std::string> queue;
    
    queue.push(uri);
    visited.insert(uri);
    
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        
        auto it = m_impl->dependencyGraph.incoming.find(current);
        if (it != m_impl->dependencyGraph.incoming.end()) {
            for (const auto& edge : it->second) {
                if (visited.find(edge.from) == visited.end()) {
                    visited.insert(edge.from);
                    result.push_back(edge.from);
                    queue.push(edge.from);
                }
            }
        }
    }
    
    return result;
}

std::vector<std::string> IntelligenceEngine::getRelatedFiles(const std::string& uri,
                                                              uint32_t depth) const {
    std::shared_lock lock(m_impl->graphMutex);
    
    std::vector<std::string> result;
    std::unordered_map<std::string, uint32_t> distances;
    std::queue<std::pair<std::string, uint32_t>> queue;
    
    queue.push({uri, 0});
    distances[uri] = 0;
    
    while (!queue.empty()) {
        auto [current, dist] = queue.front();
        queue.pop();
        
        if (dist > 0) {
            result.push_back(current);
        }
        
        if (dist < depth) {
            // Check dependencies
            auto outIt = m_impl->dependencyGraph.outgoing.find(current);
            if (outIt != m_impl->dependencyGraph.outgoing.end()) {
                for (const auto& edge : outIt->second) {
                    if (distances.find(edge.to) == distances.end()) {
                        distances[edge.to] = dist + 1;
                        queue.push({edge.to, dist + 1});
                    }
                }
            }
            
            // Check dependents
            auto inIt = m_impl->dependencyGraph.incoming.find(current);
            if (inIt != m_impl->dependencyGraph.incoming.end()) {
                for (const auto& edge : inIt->second) {
                    if (distances.find(edge.from) == distances.end()) {
                        distances[edge.from] = dist + 1;
                        queue.push({edge.from, dist + 1});
                    }
                }
            }
        }
    }
    
    return result;
}

SearchResults IntelligenceEngine::search(const SearchQuery& query) const {
    auto start = std::chrono::high_resolution_clock::now();
    
    SearchResults results;
    results.query = query.text;
    
    std::shared_lock lock(m_impl->symbolsMutex);
    
    // Build search pattern
    std::string pattern = query.text;
    if (query.wholeWord) {
        pattern = "\\b" + pattern + "\\b";
    }
    
    std::regex::flag_type flags = std::regex::ECMAScript;
    if (!query.caseSensitive) {
        flags |= std::regex::icase;
    }
    
    try {
        std::regex re(pattern, flags);
        
        // Search in symbols
        for (const auto& [id, symbol] : m_impl->symbols) {
            if (query.symbolKind != SymbolKind::Unknown && 
                symbol.kind != query.symbolKind) {
                continue;
            }
            
            if (std::regex_search(symbol.name, re)) {
                SearchResult result;
                result.symbolId = id;
                result.uri = symbol.definition.uri;
                result.location = symbol.definition;
                result.matchedText = symbol.name;
                result.score = 1.0f;
                
                results.results.push_back(std::move(result));
                
                if (results.results.size() >= query.maxResults) {
                    break;
                }
            }
        }
        
        // Search in file contents if not enough results
        if (results.results.size() < query.maxResults) {
            std::shared_lock fileLock(m_impl->filesMutex);
            
            for (const auto& [uri, file] : m_impl->files) {
                std::string content = m_impl->readFile(uri);
                
                std::istringstream iss(content);
                std::string line;
                uint32_t lineNum = 0;
                
                while (std::getline(iss, line)) {
                    lineNum++;
                    
                    std::smatch match;
                    if (std::regex_search(line, match, re)) {
                        SearchResult result;
                        result.uri = uri;
                        result.location.uri = uri;
                        result.location.line = lineNum;
                        result.matchedText = match.str();
                        result.score = 0.8f;
                        
                        // Get context
                        result.context = line;
                        
                        results.results.push_back(std::move(result));
                        
                        if (results.results.size() >= query.maxResults) {
                            break;
                        }
                    }
                }
                
                if (results.results.size() >= query.maxResults) {
                    break;
                }
            }
        }
    } catch (const std::regex_error&) {
        // Invalid regex, try literal search
        for (const auto& [id, symbol] : m_impl->symbols) {
            if (symbol.name.find(query.text) != std::string::npos) {
                SearchResult result;
                result.symbolId = id;
                result.uri = symbol.definition.uri;
                result.location = symbol.definition;
                result.matchedText = symbol.name;
                result.score = 1.0f;
                
                results.results.push_back(std::move(result));
            }
        }
    }
    
    results.totalMatches = static_cast<uint32_t>(results.results.size());
    results.filesSearched = static_cast<uint32_t>(m_impl->files.size());
    
    auto end = std::chrono::high_resolution_clock::now();
    results.searchTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return results;
}

SearchResults IntelligenceEngine::searchSymbols(const std::string& pattern) const {
    SearchQuery query;
    query.text = pattern;
    query.wholeWord = true;
    
    return search(query);
}

SearchResults IntelligenceEngine::searchText(const std::string& text) const {
    SearchQuery query;
    query.text = text;
    
    return search(query);
}

SearchResults IntelligenceEngine::searchSemantic(const std::string& description) const {
    // This would integrate with AI for semantic search
    // For now, fall back to text search
    return searchText(description);
}

AssembledContext IntelligenceEngine::assembleContext(const std::string& query,
                                                      const std::vector<std::string>& focusFiles,
                                                      const ContextWindow& window) const {
    auto start = std::chrono::high_resolution_clock::now();
    
    AssembledContext context;
    context.query = query;
    
    std::shared_lock symLock(m_impl->symbolsMutex);
    std::shared_lock fileLock(m_impl->filesMutex);
    
    // Collect focus symbols
    std::unordered_set<std::string> focusSymbols;
    for (const auto& uri : focusFiles) {
        auto it = m_impl->symbolsByFile.find(uri);
        if (it != m_impl->symbolsByFile.end()) {
            for (const auto& symbolId : it->second) {
                focusSymbols.insert(symbolId);
            }
        }
    }
    
    // Calculate relevance scores
    std::vector<std::pair<std::string, float>> scoredSymbols;
    for (const auto& [id, symbol] : m_impl->symbols) {
        float score = m_impl->calculateRelevance(id, focusFiles, focusSymbols);
        if (score >= window.minRelevanceScore) {
            scoredSymbols.push_back({id, score});
        }
    }
    
    // Sort by relevance
    std::sort(scoredSymbols.begin(), scoredSymbols.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Assemble context entries
    uint32_t tokensUsed = 0;
    std::unordered_set<std::string> includedFiles;
    
    for (const auto& [symbolId, score] : scoredSymbols) {
        if (tokensUsed >= window.maxTokens) break;
        if (context.entries.size() >= window.maxSymbols) break;
        
        auto symIt = m_impl->symbols.find(symbolId);
        if (symIt == m_impl->symbols.end()) continue;
        
        const auto& symbol = symIt->second;
        
        ContextEntry entry;
        entry.uri = symbol.definition.uri;
        entry.symbolId = symbolId;
        entry.relevanceScore = score;
        
        // Determine priority
        if (focusSymbols.count(symbolId) > 0) {
            entry.priority = ContextPriority::Critical;
        } else if (score > 0.7f) {
            entry.priority = ContextPriority::High;
        } else if (score > 0.4f) {
            entry.priority = ContextPriority::Medium;
        } else {
            entry.priority = ContextPriority::Low;
        }
        
        // Build content
        std::ostringstream content;
        
        if (window.includeSignatures) {
            content << "// " << symbol.qualifiedName << "\n";
            if (!symbol.signature.empty()) {
                content << symbol.signature << "\n";
            }
        }
        
        if (window.includeDocumentation && !symbol.documentation.empty()) {
            content << "// " << symbol.documentation << "\n";
        }
        
        if (window.includeImplementation) {
            std::string fileContent = m_impl->readFile(symbol.definition.uri);
            if (!fileContent.empty()) {
                // Extract implementation
                std::istringstream iss(fileContent);
                std::string line;
                uint32_t lineNum = 0;
                uint32_t linesIncluded = 0;
                
                while (std::getline(iss, line) && linesIncluded < window.maxLinesPerFile) {
                    lineNum++;
                    if (lineNum >= symbol.definition.line && 
                        lineNum <= symbol.definition.endLine) {
                        content << line << "\n";
                        linesIncluded++;
                    }
                }
            }
        }
        
        entry.content = content.str();
        entry.tokenCount = m_impl->estimateTokens(entry.content);
        
        if (tokensUsed + entry.tokenCount <= window.maxTokens) {
            context.entries.push_back(std::move(entry));
            tokensUsed += entry.tokenCount;
            includedFiles.insert(symbol.definition.uri);
        }
    }
    
    // Add file contents
    for (const auto& uri : focusFiles) {
        if (includedFiles.count(uri) == 0 && includedFiles.size() < window.maxFiles) {
            std::string content = m_impl->readFile(uri);
            if (!content.empty()) {
                ContextEntry entry;
                entry.uri = uri;
                entry.priority = ContextPriority::Critical;
                entry.content = "// File: " + uri + "\n" + content;
                entry.tokenCount = m_impl->estimateTokens(entry.content);
                entry.reason = "Focus file";
                
                if (tokensUsed + entry.tokenCount <= window.maxTokens) {
                    context.entries.push_back(std::move(entry));
                    tokensUsed += entry.tokenCount;
                    includedFiles.insert(uri);
                }
            }
        }
    }
    
    // Build assembled content
    std::ostringstream assembled;
    for (const auto& entry : context.entries) {
        assembled << entry.content << "\n\n";
    }
    context.assembledContent = assembled.str();
    
    context.totalTokens = tokensUsed;
    context.fileCount = static_cast<uint32_t>(includedFiles.size());
    context.symbolCount = static_cast<uint32_t>(context.entries.size());
    
    for (const auto& uri : includedFiles) {
        context.includedFiles.push_back(uri);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    context.assemblyTime = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    return context;
}

AssembledContext IntelligenceEngine::assembleContextForSymbol(const std::string& symbolId,
                                                                const ContextWindow& window) const {
    auto symbol = getSymbol(symbolId);
    if (!symbol) return {};
    
    return assembleContext(symbol->name, {symbol->definition.uri}, window);
}

AssembledContext IntelligenceEngine::assembleContextForFile(const std::string& uri,
                                                             const ContextWindow& window) const {
    return assembleContext(uri, {uri}, window);
}

std::vector<SymbolDefinition> IntelligenceEngine::getHotspots(uint32_t limit) const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    std::vector<SymbolDefinition> result;
    
    // Sort by importance score
    std::vector<std::pair<std::string, float>> scored;
    for (const auto& [id, symbol] : m_impl->symbols) {
        float score = symbol.importanceScore;
        if (symbol.referenceCount > 0) {
            score += std::log10(symbol.referenceCount) * 0.1f;
        }
        scored.push_back({id, score});
    }
    
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });
    
    for (size_t i = 0; i < std::min(scored.size(), static_cast<size_t>(limit)); ++i) {
        auto it = m_impl->symbols.find(scored[i].first);
        if (it != m_impl->symbols.end()) {
            result.push_back(it->second);
        }
    }
    
    return result;
}

std::vector<SymbolDefinition> IntelligenceEngine::getUnstableSymbols(uint32_t limit) const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    std::vector<SymbolDefinition> result;
    
    // Sort by stability score (ascending)
    std::vector<std::pair<std::string, float>> scored;
    for (const auto& [id, symbol] : m_impl->symbols) {
        scored.push_back({id, symbol.stabilityScore});
    }
    
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    
    for (size_t i = 0; i < std::min(scored.size(), static_cast<size_t>(limit)); ++i) {
        auto it = m_impl->symbols.find(scored[i].first);
        if (it != m_impl->symbols.end()) {
            result.push_back(it->second);
        }
    }
    
    return result;
}

std::vector<SymbolDefinition> IntelligenceEngine::getOrphanedSymbols() const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    std::vector<SymbolDefinition> result;
    
    for (const auto& [id, symbol] : m_impl->symbols) {
        if (symbol.referenceCount == 0 && !symbol.isExported) {
            result.push_back(symbol);
        }
    }
    
    return result;
}

std::vector<std::string> IntelligenceEngine::getCircularDependencies() const {
    std::shared_lock lock(m_impl->graphMutex);
    
    std::vector<std::string> result;
    
    for (const auto& component : m_impl->dependencyGraph.stronglyConnectedComponents) {
        if (component.size() > 1) {
            for (const auto& uri : component) {
                result.push_back(uri);
            }
        }
    }
    
    return result;
}

std::vector<std::string> IntelligenceEngine::getUnusedExports() const {
    std::shared_lock lock(m_impl->symbolsMutex);
    
    std::vector<std::string> result;
    
    for (const auto& [id, symbol] : m_impl->symbols) {
        if (symbol.isExported && symbol.referenceCount == 0) {
            result.push_back(id);
        }
    }
    
    return result;
}

IndexStats IntelligenceEngine::getStats() const {
    return m_impl->stats;
}

bool IntelligenceEngine::isIndexed(const std::string& uri) const {
    std::shared_lock lock(m_impl->filesMutex);
    
    auto it = m_impl->files.find(uri);
    return it != m_impl->files.end() && it->second.isIndexed;
}

float IntelligenceEngine::getIndexProgress() const {
    return m_impl->indexProgress;
}

void IntelligenceEngine::setIndexCallback(IndexCallback callback) {
    m_impl->callback = std::move(callback);
}

// Factory function
std::unique_ptr<IIntelligenceEngine> createIntelligenceEngine() {
    return std::make_unique<IntelligenceEngine>();
}

} // namespace RawrXD::Intelligence
