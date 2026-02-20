// ============================================================================
// semantic_index.h — Enhanced SymbolIndex with Truffle/Graal-style Semantics
// ============================================================================
// Extends existing Indexer + SemanticStore with:
//   - Cross-language polyglot indexing
//   - Incremental file update tracking
//   - Dependency graph (import/include analysis)
//   - Type hierarchy (inheritance chains)
//   - Call graph (caller/callee relationships)
//   - Semantic search with embeddings
//   - C-ABI DLL plugin contract for custom analyzers
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <chrono>

// Existing context types
namespace RawrXD { namespace Context {
    struct Symbol;
    struct EmbeddingItem;
    struct SearchResult;
    class Indexer;
    class SemanticStore;
}}

namespace RawrXD {
namespace SemanticIndex {

// ============================================================================
// Symbol Kind (richer than Indexer's string-based kind)
// ============================================================================
enum class SymbolKind : uint16_t {
    Unknown         = 0,
    File            = 1,
    Module          = 2,
    Namespace       = 3,
    Package         = 4,
    Class           = 5,
    Interface       = 6,
    Struct          = 7,
    Enum            = 8,
    EnumMember      = 9,
    Function        = 10,
    Method          = 11,
    Constructor     = 12,
    Destructor      = 13,
    Property        = 14,
    Field           = 15,
    Variable        = 16,
    Constant        = 17,
    Parameter       = 18,
    TypeParameter   = 19,
    TypeAlias       = 20,
    Macro           = 21,
    Label           = 22,
    Import          = 23,
    Operator        = 24,
    Event           = 25,
    Trait           = 26,
    Concept         = 27,
};

// ============================================================================
// Visibility
// ============================================================================
enum class SymbolVisibility : uint8_t {
    Unknown     = 0,
    Public      = 1,
    Protected   = 2,
    Private     = 3,
    Internal    = 4,
};

// ============================================================================
// Source Location — precise position in a file
// ============================================================================
struct SourceLocation {
    std::string     filePath;
    int32_t         startLine = 0;
    int32_t         startCol  = 0;
    int32_t         endLine   = 0;
    int32_t         endCol    = 0;
};

// ============================================================================
// Rich Symbol — full semantic information
// ============================================================================
struct RichSymbol {
    std::string             id;             // Unique: file#kind#name#line
    std::string             name;           // Short name
    std::string             qualifiedName;  // Fully qualified (ns::class::func)
    std::string             language;       // "cpp", "python", etc.
    SymbolKind              kind = SymbolKind::Unknown;
    SymbolVisibility        visibility = SymbolVisibility::Unknown;
    
    SourceLocation          definition;     // Where defined
    std::vector<SourceLocation> declarations; // Forward declarations
    std::vector<SourceLocation> references;   // All usage sites
    
    std::string             typeSignature;  // e.g. "int(float, const string&)"
    std::string             returnType;
    std::vector<std::string> paramTypes;
    std::string             documentation;  // Doc comment
    
    std::string             parentId;       // Containing class/namespace
    std::vector<std::string> childIds;      // Members/nested
    
    std::vector<std::string> baseTypes;     // Inheritance parents
    std::vector<std::string> derivedTypes;  // Inheritance children
    std::vector<std::string> implementedInterfaces;
    
    std::vector<std::string> callers;       // Functions that call this
    std::vector<std::string> callees;       // Functions this calls
    
    std::vector<std::string> tags;          // User-defined tags
    
    uint64_t                lastModified = 0; // For incremental updates
    float                   relevanceScore = 0.0f; // For ranking
};

// ============================================================================
// Dependency Edge — import/include relationship
// ============================================================================
struct DependencyEdge {
    std::string     sourceFile;
    std::string     targetFile;
    std::string     importExpr;     // "#include <vector>" or "import os"
    int32_t         line = 0;
    bool            isTransitive = false;
};

// ============================================================================
// Type Hierarchy Node
// ============================================================================
struct TypeNode {
    std::string             symbolId;
    std::string             name;
    std::string             language;
    SymbolKind              kind = SymbolKind::Class;
    std::vector<std::string> parents;
    std::vector<std::string> children;
    std::vector<std::string> interfaces;
    int32_t                 depth = 0;      // Distance from root
};

// ============================================================================
// Call Graph Edge
// ============================================================================
struct CallEdge {
    std::string     callerId;
    std::string     calleeId;
    SourceLocation  callSite;
    bool            isDynamic = false;      // Virtual dispatch
    bool            isConditional = false;  // Inside if/switch
    int32_t         callCount = 1;          // Static heuristic
};

// ============================================================================
// Index Statistics
// ============================================================================
struct IndexStatistics {
    size_t      totalFiles      = 0;
    size_t      totalSymbols    = 0;
    size_t      totalReferences = 0;
    size_t      totalDeps       = 0;
    size_t      totalCallEdges  = 0;
    size_t      totalTypeNodes  = 0;
    size_t      languageCount   = 0;
    std::map<std::string, size_t> symbolsByLanguage;
    std::map<std::string, size_t> symbolsByKind;
    double      buildTimeMs     = 0.0;
    double      lastIncrUpdateMs = 0.0;
    uint64_t    lastBuildTimestamp = 0;
};

// ============================================================================
// Query Options
// ============================================================================
struct QueryOptions {
    int32_t                 maxResults = 100;
    bool                    includeReferences = false;
    bool                    includeCallGraph = false;
    bool                    includeTypeHierarchy = false;
    bool                    fuzzyMatch = false;
    float                   minRelevance = 0.0f;
    std::vector<std::string> languageFilter;
    std::vector<SymbolKind>  kindFilter;
    std::string             scopeFile;      // Limit to file
    std::string             scopeDirectory; // Limit to directory
};

// ============================================================================
// Query Result
// ============================================================================
struct QueryResult {
    std::vector<RichSymbol>     symbols;
    int32_t                     totalMatches = 0;
    double                      queryTimeMs = 0.0;
    std::string                 error;
    
    bool ok() const { return error.empty(); }
};

// ============================================================================
// File Change — for incremental updates
// ============================================================================
enum class FileChangeType : uint8_t {
    Created  = 0,
    Modified = 1,
    Deleted  = 2,
    Renamed  = 3,
};

struct FileChange {
    std::string     filePath;
    FileChangeType  type = FileChangeType::Modified;
    std::string     oldPath;        // For renames
    uint64_t        timestamp = 0;
};

// ============================================================================
// C-ABI Plugin Contract for Custom Analyzers
// ============================================================================
struct CRichSymbol {
    const char*     id;
    const char*     name;
    const char*     qualifiedName;
    const char*     language;
    uint16_t        kind;           // SymbolKind ordinal
    uint8_t         visibility;     // SymbolVisibility ordinal
    const char*     filePath;
    int32_t         startLine;
    int32_t         endLine;
    const char*     typeSignature;
    const char*     returnType;
    const char*     documentation;
    const char*     parentId;
};

struct CAnalyzerPluginInfo {
    const char*     name;
    const char*     version;
    const char*     author;
    const char*     supportedLanguages; // Comma-separated
};

extern "C" {
    typedef CAnalyzerPluginInfo* (*AnalyzerPlugin_GetInfo_fn)();
    typedef int     (*AnalyzerPlugin_Init_fn)(const char* configJson);
    typedef int     (*AnalyzerPlugin_AnalyzeFile_fn)(const char* filePath,
                                                      const char* content,
                                                      CRichSymbol* outSymbols,
                                                      int maxSymbols);
    typedef int     (*AnalyzerPlugin_GetDependencies_fn)(const char* filePath,
                                                          const char* content,
                                                          char* outDeps,
                                                          int outBufSize);
    typedef void    (*AnalyzerPlugin_Shutdown_fn)();
}

// ============================================================================
// Semantic Index Engine — Singleton
// ============================================================================
class SemanticIndexEngine {
public:
    static SemanticIndexEngine& Instance();
    ~SemanticIndexEngine();
    
    // ── Lifecycle ──
    void Initialize(const std::string& workspaceRoot);
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }
    
    // ── Full Build ──
    IndexStatistics BuildIndex(bool recursive = true);
    
    // ── Incremental Update ──
    void NotifyFileChange(const FileChange& change);
    void ProcessPendingChanges();
    
    // ── Symbol Queries ──
    QueryResult FindByName(const std::string& name, const QueryOptions& opts = {}) const;
    QueryResult FindByQualifiedName(const std::string& qn, const QueryOptions& opts = {}) const;
    QueryResult FindByKind(SymbolKind kind, const QueryOptions& opts = {}) const;
    QueryResult FindInFile(const std::string& filePath, const QueryOptions& opts = {}) const;
    QueryResult FindInScope(const std::string& scopeId, const QueryOptions& opts = {}) const;
    QueryResult FuzzySearch(const std::string& query, const QueryOptions& opts = {}) const;
    
    // ── Semantic Search (embedding-based) ──
    QueryResult SemanticSearch(const std::string& naturalLanguage, int topK = 10) const;
    void SetEmbeddingCallback(std::function<std::vector<float>(const std::string&)> cb);
    
    // ── Symbol Details ──
    const RichSymbol* GetSymbol(const std::string& id) const;
    std::vector<SourceLocation> GetReferences(const std::string& symbolId) const;
    std::vector<SourceLocation> GetDefinitions(const std::string& symbolId) const;
    
    // ── Dependency Graph ──
    std::vector<DependencyEdge> GetFileDependencies(const std::string& filePath) const;
    std::vector<DependencyEdge> GetFileDependents(const std::string& filePath) const;
    std::vector<std::string> GetTransitiveDependencies(const std::string& filePath) const;
    std::vector<std::string> FindCycles() const;
    
    // ── Type Hierarchy ──
    TypeNode GetTypeNode(const std::string& symbolId) const;
    std::vector<TypeNode> GetSuperTypes(const std::string& symbolId) const;
    std::vector<TypeNode> GetSubTypes(const std::string& symbolId) const;
    std::vector<TypeNode> GetTypeHierarchyTree(const std::string& symbolId) const;
    
    // ── Call Graph ──
    std::vector<CallEdge> GetCallers(const std::string& symbolId) const;
    std::vector<CallEdge> GetCallees(const std::string& symbolId) const;
    std::vector<CallEdge> GetCallChain(const std::string& from, const std::string& to,
                                        int maxDepth = 10) const;
    
    // ── Statistics ──
    IndexStatistics GetStats() const;
    
    // ── Plugin Management ──
    bool LoadAnalyzerPlugin(const std::string& dllPath);
    void UnloadAnalyzerPlugin(const std::string& name);
    void UnloadAllPlugins();
    std::vector<std::string> GetLoadedPlugins() const;
    
    // ── Serialization ──
    bool SaveIndex(const std::string& filePath) const;
    bool LoadIndex(const std::string& filePath);
    
    // ── Cross-Language Navigation ──
    struct CrossLangRef {
        std::string sourceSymbolId;
        std::string targetSymbolId;
        std::string bindingKind;    // "ffi", "pybind", "jni", "wasm", etc.
    };
    void AddCrossLanguageBinding(const CrossLangRef& ref);
    std::vector<CrossLangRef> GetCrossLanguageBindings(const std::string& symbolId) const;
    
private:
    SemanticIndexEngine() = default;
    
    // ── Internal Indexing ──
    void indexFile(const std::string& filePath);
    void indexFileWithPlugin(const std::string& filePath, const std::string& content);
    void removeFileSymbols(const std::string& filePath);
    std::string detectLanguage(const std::string& filePath) const;
    
    // ── Built-in Analyzers ──
    void analyzeCpp(const std::string& filePath, const std::string& content);
    void analyzePython(const std::string& filePath, const std::string& content);
    void analyzeJavaScript(const std::string& filePath, const std::string& content);
    void analyzeRust(const std::string& filePath, const std::string& content);
    void analyzeGo(const std::string& filePath, const std::string& content);
    void analyzeMasm(const std::string& filePath, const std::string& content);
    
    // ── Dependency Extraction ──
    void extractDependencies(const std::string& filePath, const std::string& content,
                              const std::string& language);
    
    // ── Symbol ID Generation ──
    std::string makeSymbolId(const std::string& file, SymbolKind kind,
                              const std::string& name, int line) const;
    
    // ── Fuzzy Matching ──
    float fuzzyScore(const std::string& pattern, const std::string& candidate) const;
    
    // ── Plugin State ──
    struct LoadedPlugin {
        std::string                     path;
        std::string                     name;
        std::vector<std::string>        languages;
        void*                           hModule = nullptr;
        AnalyzerPlugin_GetInfo_fn       fnGetInfo = nullptr;
        AnalyzerPlugin_Init_fn          fnInit = nullptr;
        AnalyzerPlugin_AnalyzeFile_fn   fnAnalyze = nullptr;
        AnalyzerPlugin_GetDependencies_fn fnGetDeps = nullptr;
        AnalyzerPlugin_Shutdown_fn      fnShutdown = nullptr;
    };
    
    // ── Data Stores ──
    mutable std::mutex                                          m_mutex;
    std::string                                                 m_workspaceRoot;
    std::map<std::string, RichSymbol>                           m_symbols;      // id → symbol
    std::map<std::string, std::vector<std::string>>             m_fileSymbols;  // file → symbol ids
    std::multimap<std::string, DependencyEdge>                  m_deps;         // file → deps
    std::multimap<std::string, DependencyEdge>                  m_reverseDeps;  // file → dependents
    std::map<std::string, TypeNode>                             m_typeNodes;    // symbolId → type node
    std::vector<CallEdge>                                       m_callEdges;
    std::map<std::string, std::vector<size_t>>                  m_callerIndex;  // symbolId → edge indices
    std::map<std::string, std::vector<size_t>>                  m_calleeIndex;  // symbolId → edge indices
    std::vector<CrossLangRef>                                   m_crossLangRefs;
    
    // ── File Tracking ──
    std::map<std::string, uint64_t>                             m_fileTimestamps;
    std::vector<FileChange>                                     m_pendingChanges;
    mutable std::mutex                                          m_pendingMutex;
    
    // ── Embedding Support ──
    std::function<std::vector<float>(const std::string&)>       m_embeddingFn;
    std::map<std::string, std::vector<float>>                   m_embeddings;   // symbolId → embedding
    
    // ── Plugin State ──
    std::vector<LoadedPlugin>                                   m_plugins;
    
    // ── Statistics ──
    IndexStatistics                                             m_stats;
    mutable std::mutex                                          m_statsMutex;
    
    std::atomic<bool>                                           m_initialized{false};
};

} // namespace SemanticIndex
} // namespace RawrXD
