// ============================================================================
// codebase_intelligence.h — Context-Aware Codebase Intelligence Engine
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================
// Reverse-engineered Cody/Sourcegraph codebase understanding
// Provides semantic search, dependency analysis, and intelligent context assembly
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <optional>
#include <functional>

namespace RawrXD::Intelligence {

// ============================================================================
// Core Types
// ============================================================================

enum class SymbolKind {
    Unknown,
    File,
    Module,
    Namespace,
    Package,
    Class,
    Struct,
    Interface,
    Enum,
    Function,
    Method,
    Property,
    Field,
    Variable,
    Constant,
    TypeParameter,
    Constructor,
    Destructor,
    Operator,
    Macro,
    Annotation,
    Trait,
    Union
};

enum class ReferenceKind {
    Definition,
    Declaration,
    Reference,
    Implementation,
    TypeReference,
    Import,
    Export,
    Call,
    Inheritance,
    Override,
    Extension
};

enum class ContextPriority {
    Critical,    // Must include (direct dependencies)
    High,        // Strongly related (same module, frequent co-usage)
    Medium,      // Moderately related (shared dependencies)
    Low,         // Weakly related (transitive dependencies)
    Background   // Optional context (related symbols)
};

// ============================================================================
// Symbol Representation
// ============================================================================

struct SourceLocation {
    std::string uri;
    uint32_t line = 0;
    uint32_t column = 0;
    uint32_t endLine = 0;
    uint32_t endColumn = 0;
    
    bool operator==(const SourceLocation& other) const {
        return uri == other.uri && line == other.line && column == other.column;
    }
    
    bool isValid() const { return !uri.empty() && line > 0; }
};

struct SymbolReference {
    std::string symbolId;
    SourceLocation location;
    ReferenceKind kind = ReferenceKind::Reference;
    std::string context;  // Surrounding code snippet
    uint32_t usageCount = 0;
    float relevanceScore = 0.0f;
};

struct SymbolDefinition {
    std::string id;
    std::string name;
    std::string qualifiedName;
    std::string signature;
    std::string documentation;
    SymbolKind kind = SymbolKind::Unknown;
    
    SourceLocation definition;
    std::vector<SourceLocation> declarations;
    
    // Type information
    std::string type;
    std::string returnType;
    std::vector<std::string> parameters;
    std::vector<std::string> typeParameters;
    
    // Relationships
    std::vector<std::string> dependencies;      // What this symbol uses
    std::vector<std::string> dependents;        // What uses this symbol
    std::vector<std::string> implementations;   // Implementations of this interface
    std::vector<std::string> inheritedFrom;     // Parent classes/interfaces
    std::vector<std::string> inheritedBy;       // Child classes
    
    // Metadata
    std::string visibility;  // public, private, protected
    bool isStatic = false;
    bool isAbstract = false;
    bool isVirtual = false;
    bool isDeprecated = false;
    bool isExported = false;
    bool isTest = false;
    
    // Metrics
    uint32_t linesOfCode = 0;
    uint32_t cyclomaticComplexity = 0;
    uint32_t referenceCount = 0;
    float importanceScore = 0.0f;
    float stabilityScore = 0.0f;
    
    // Timestamps
    std::chrono::system_clock::time_point lastModified;
    std::chrono::system_clock::time_point lastAccessed;
};

// ============================================================================
// File Intelligence
// ============================================================================

struct FileImport {
    std::string module;
    std::string alias;
    std::vector<std::string> symbols;
    bool isDefault = false;
    bool isNamespace = false;
    SourceLocation location;
};

struct FileExport {
    std::string symbolId;
    std::string name;
    bool isDefault = false;
    bool isReexport = false;
    SourceLocation location;
};

struct FileInfo {
    std::string uri;
    std::string language;
    std::string moduleName;
    
    // Content hash for change detection
    std::string contentHash;
    uint64_t fileSize = 0;
    uint32_t lineCount = 0;
    
    // Symbols
    std::vector<std::string> definedSymbols;
    std::vector<std::string> referencedSymbols;
    
    // Imports/Exports
    std::vector<FileImport> imports;
    std::vector<FileExport> exports;
    
    // Dependencies
    std::vector<std::string> dependencies;     // Files this file imports
    std::vector<std::string> dependents;       // Files that import this file
    
    // Metrics
    uint32_t symbolCount = 0;
    uint32_t importCount = 0;
    uint32_t exportCount = 0;
    float complexityScore = 0.0f;
    float maintainabilityIndex = 0.0f;
    
    // Status
    bool isIndexed = false;
    bool hasErrors = false;
    std::chrono::system_clock::time_point lastIndexed;
    std::chrono::system_clock::time_point lastModified;
};

// ============================================================================
// Dependency Graph
// ============================================================================

struct DependencyEdge {
    std::string from;
    std::string to;
    ReferenceKind kind;
    uint32_t weight = 1;
    std::vector<SourceLocation> locations;
};

struct DependencyNode {
    std::string id;
    std::string name;
    SymbolKind kind;
    uint32_t inDegree = 0;   // How many depend on this
    uint32_t outDegree = 0;  // How many this depends on
    
    // Centrality measures
    float betweennessCentrality = 0.0f;
    float closenessCentrality = 0.0f;
    float pageRank = 0.0f;
    
    // Stability
    float stabilityScore = 0.0f;
    float changeFrequency = 0.0f;
};

struct DependencyGraph {
    std::unordered_map<std::string, DependencyNode> nodes;
    std::unordered_map<std::string, std::vector<DependencyEdge>> outgoing;
    std::unordered_map<std::string, std::vector<DependencyEdge>> incoming;
    
    // Cached computations
    std::vector<std::string> topologicalOrder;
    std::vector<std::vector<std::string>> stronglyConnectedComponents;
    std::unordered_map<std::string, uint32_t> componentIds;
};

// ============================================================================
// Context Assembly
// ============================================================================

struct ContextWindow {
    uint32_t maxTokens = 8000;
    uint32_t maxFiles = 20;
    uint32_t maxSymbols = 100;
    uint32_t maxLinesPerFile = 100;
    float minRelevanceScore = 0.1f;
    bool includeImports = true;
    bool includeTests = false;
    bool includeGenerated = false;
    bool includeDocumentation = true;
    bool includeSignatures = true;
    bool includeImplementation = false;
};

struct ContextEntry {
    std::string uri;
    std::string symbolId;
    std::string content;
    ContextPriority priority;
    float relevanceScore;
    uint32_t tokenCount;
    std::string reason;  // Why this was included
};

struct AssembledContext {
    std::string query;
    std::vector<ContextEntry> entries;
    
    uint32_t totalTokens = 0;
    uint32_t fileCount = 0;
    uint32_t symbolCount = 0;
    
    std::string assembledContent;
    std::unordered_map<std::string, std::string> fileContents;
    std::unordered_map<std::string, std::string> symbolSignatures;
    
    // Metadata
    std::chrono::milliseconds assemblyTime{0};
    std::vector<std::string> includedFiles;
    std::vector<std::string> excludedFiles;
    std::vector<std::string> warnings;
};

// ============================================================================
// Semantic Search
// ============================================================================

struct SearchQuery {
    std::string text;
    std::string pattern;          // Regex pattern
    std::string symbolName;
    SymbolKind symbolKind = SymbolKind::Unknown;
    std::string language;
    std::vector<std::string> filePatterns;
    std::vector<std::string> excludePatterns;
    
    bool caseSensitive = false;
    bool wholeWord = true;
    bool regexMode = false;
    bool fuzzyMatch = false;
    float fuzzyThreshold = 0.7f;
    
    uint32_t maxResults = 100;
    uint32_t contextLines = 3;
};

struct SearchResult {
    std::string symbolId;
    std::string uri;
    SourceLocation location;
    std::string matchedText;
    std::string context;
    float score;
    std::vector<std::string> highlights;
};

struct SearchResults {
    std::vector<SearchResult> results;
    uint32_t totalMatches = 0;
    uint32_t filesSearched = 0;
    std::chrono::milliseconds searchTime{0};
    std::string query;
};

// ============================================================================
// Intelligence Index
// ============================================================================

struct IndexStats {
    uint64_t totalFiles = 0;
    uint64_t totalSymbols = 0;
    uint64_t totalReferences = 0;
    uint64_t totalImports = 0;
    uint64_t totalExports = 0;
    
    uint64_t indexSizeBytes = 0;
    std::chrono::milliseconds lastIndexTime{0};
    std::chrono::system_clock::time_point lastUpdated;
    
    std::unordered_map<std::string, uint64_t> filesByLanguage;
    std::unordered_map<std::string, uint64_t> symbolsByKind;
};

struct IndexConfig {
    std::vector<std::string> includePatterns = {"**/*.cpp", "**/*.h", "**/*.py", "**/*.ts", "**/*.js"};
    std::vector<std::string> excludePatterns = {"**/node_modules/**", "**/build/**", "**/.git/**"};
    
    uint32_t maxFileSize = 10 * 1024 * 1024;  // 10MB
    uint32_t maxFiles = 100000;
    uint32_t maxSymbolsPerFile = 10000;
    
    bool indexTests = true;
    bool indexGenerated = false;
    bool indexDocumentation = true;
    bool indexComments = false;
    
    bool computeMetrics = true;
    bool computeCentrality = true;
    bool computeStability = true;
    
    uint32_t updateIntervalMs = 5000;
    bool watchForChanges = true;
};

// ============================================================================
// Intelligence Engine Interface
// ============================================================================

class IIntelligenceEngine {
public:
    virtual ~IIntelligenceEngine() = default;
    
    // Configuration
    virtual void setConfig(const IndexConfig& config) = 0;
    virtual IndexConfig getConfig() const = 0;
    
    // Indexing
    virtual bool indexWorkspace(const std::string& rootPath) = 0;
    virtual bool indexFile(const std::string& uri) = 0;
    virtual bool removeFile(const std::string& uri) = 0;
    virtual bool updateFile(const std::string& uri) = 0;
    virtual bool reindexAll() = 0;
    
    // Symbol queries
    virtual std::optional<SymbolDefinition> getSymbol(const std::string& symbolId) const = 0;
    virtual std::vector<SymbolDefinition> findSymbols(const std::string& name) const = 0;
    virtual std::vector<SymbolDefinition> findSymbolsByKind(SymbolKind kind) const = 0;
    virtual std::vector<SymbolReference> findReferences(const std::string& symbolId) const = 0;
    virtual std::vector<SymbolDefinition> findImplementations(const std::string& symbolId) const = 0;
    virtual std::optional<SymbolDefinition> findDefinition(const std::string& name, 
                                                           const std::string& contextUri) const = 0;
    
    // File queries
    virtual std::optional<FileInfo> getFile(const std::string& uri) const = 0;
    virtual std::vector<FileInfo> getFiles(const std::string& pattern = "*") const = 0;
    virtual std::vector<std::string> getDependencies(const std::string& uri) const = 0;
    virtual std::vector<std::string> getDependents(const std::string& uri) const = 0;
    
    // Dependency analysis
    virtual DependencyGraph getDependencyGraph() const = 0;
    virtual std::vector<std::string> getImportPath(const std::string& from, 
                                                    const std::string& to) const = 0;
    virtual std::vector<std::string> getAffectedFiles(const std::string& uri) const = 0;
    virtual std::vector<std::string> getRelatedFiles(const std::string& uri, 
                                                       uint32_t depth = 2) const = 0;
    
    // Semantic search
    virtual SearchResults search(const SearchQuery& query) const = 0;
    virtual SearchResults searchSymbols(const std::string& pattern) const = 0;
    virtual SearchResults searchText(const std::string& text) const = 0;
    virtual SearchResults searchSemantic(const std::string& description) const = 0;
    
    // Context assembly
    virtual AssembledContext assembleContext(const std::string& query,
                                               const std::vector<std::string>& focusFiles,
                                               const ContextWindow& window) const = 0;
    
    virtual AssembledContext assembleContextForSymbol(const std::string& symbolId,
                                                        const ContextWindow& window) const = 0;
    
    virtual AssembledContext assembleContextForFile(const std::string& uri,
                                                      const ContextWindow& window) const = 0;
    
    // Intelligence queries
    virtual std::vector<SymbolDefinition> getHotspots(uint32_t limit = 20) const = 0;
    virtual std::vector<SymbolDefinition> getUnstableSymbols(uint32_t limit = 20) const = 0;
    virtual std::vector<SymbolDefinition> getOrphanedSymbols() const = 0;
    virtual std::vector<std::string> getCircularDependencies() const = 0;
    virtual std::vector<std::string> getUnusedExports() const = 0;
    
    // Statistics
    virtual IndexStats getStats() const = 0;
    virtual bool isIndexed(const std::string& uri) const = 0;
    virtual float getIndexProgress() const = 0;
    
    // Event handling
    using IndexCallback = std::function<void(const std::string& uri, bool success)>;
    virtual void setIndexCallback(IndexCallback callback) = 0;
};

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<IIntelligenceEngine> createIntelligenceEngine();

} // namespace RawrXD::Intelligence
