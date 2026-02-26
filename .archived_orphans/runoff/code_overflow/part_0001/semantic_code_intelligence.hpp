// semantic_code_intelligence.hpp — Phase 16: Semantic Code Intelligence
// Cross-reference database, type inference engine, symbol resolution across
// compilation units, semantic indexing, and intelligent code navigation.
//
// Architecture: Builds a persistent symbol table with scope-aware resolution.
//               Supports incremental updates for live editing. Provides
//               go-to-definition, find-references, type hierarchy, and
//               call graph data to the IDE frontend.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured PatchResult returns only.
// Rule: All threading via std::mutex + std::lock_guard. No recursive locks.
#pragma once

#ifndef RAWRXD_SEMANTIC_CODE_INTELLIGENCE_HPP
#define RAWRXD_SEMANTIC_CODE_INTELLIGENCE_HPP

#include "model_memory_hotpatch.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

// ============================================================================
// Symbol Kind — Classification of code symbols
// ============================================================================
enum class SymbolKind : uint8_t {
    Unknown     = 0,
    Function    = 1,
    Method      = 2,
    Constructor = 3,
    Destructor  = 4,
    Variable    = 5,
    Parameter   = 6,
    Field       = 7,
    Property    = 8,
    Class       = 9,
    Struct      = 10,
    Enum        = 11,
    EnumValue   = 12,
    Interface   = 13,
    Namespace   = 14,
    Typedef     = 15,
    Macro       = 16,
    Template    = 17,
    Label       = 18,
    Module      = 19,
    Operator    = 20
};

// ============================================================================
// Symbol Visibility
// ============================================================================
enum class SymbolVisibility : uint8_t {
    Public      = 0,
    Protected   = 1,
    Private     = 2,
    Internal    = 3,   // File-scope / anonymous namespace
    Exported    = 4    // DLL/shared library export
};

// ============================================================================
// Type Info — Resolved type for a symbol
// ============================================================================
struct TypeInfo {
    uint64_t            typeId;
    std::string         name;              // e.g. "int", "std::string", "MyClass*"
    std::string         qualifiedName;     // e.g. "namespace::MyClass"
    bool                isConst;
    bool                isVolatile;
    bool                isPointer;
    bool                isReference;
    bool                isArray;
    uint32_t            arraySize;         // 0 = dynamic/unknown
    uint64_t            pointeeTypeId;     // For pointer/reference types
    std::vector<uint64_t> templateArgs;    // Template argument type IDs
    uint64_t            sizeBytes;         // sizeof

    TypeInfo()
        : typeId(0), isConst(false), isVolatile(false), isPointer(false)
        , isReference(false), isArray(false), arraySize(0)
        , pointeeTypeId(0), sizeBytes(0) {}

    static TypeInfo primitive(const std::string& name, uint64_t size) {
        TypeInfo ti;
        ti.name = name;
        ti.qualifiedName = name;
        ti.sizeBytes = size;
        return ti;
    }
};

// ============================================================================
// Source Location
// ============================================================================
struct SourceLocation {
    std::string         filePath;
    uint32_t            line;
    uint32_t            column;
    uint32_t            endLine;
    uint32_t            endColumn;
    uint64_t            offset;         // Byte offset in file

    SourceLocation()
        : line(0), column(0), endLine(0), endColumn(0), offset(0) {}

    static SourceLocation make(const std::string& file, uint32_t ln, uint32_t col) {
        SourceLocation loc;
        loc.filePath  = file;
        loc.line      = ln;
        loc.column    = col;
        loc.endLine   = ln;
        loc.endColumn = col;
        return loc;
    }
};

// ============================================================================
// Symbol Entry — A single symbol in the cross-reference database
// ============================================================================
struct SymbolEntry {
    uint64_t            symbolId;
    std::string         name;
    std::string         qualifiedName;       // Fully qualified with namespaces
    std::string         displayName;         // For UI display
    SymbolKind          kind;
    SymbolVisibility    visibility;
    uint64_t            typeId;              // Resolved type
    uint64_t            parentSymbolId;      // Containing scope (class, namespace, etc.)
    SourceLocation      definition;
    std::vector<SourceLocation> declarations;
    std::string         documentation;       // Extracted doc comment
    std::string         signature;           // Function signature, type declaration, etc.

    // Relationships
    std::vector<uint64_t> childSymbols;      // Members, nested types
    std::vector<uint64_t> baseTypes;         // Inheritance chain
    std::vector<uint64_t> derivedTypes;      // Types that inherit from this
    std::vector<uint64_t> implementedInterfaces;

    // Flags
    bool                isStatic;
    bool                isVirtual;
    bool                isAbstract;
    bool                isInline;
    bool                isConstexpr;
    bool                isDeprecated;
    bool                isGenerated;         // Compiler-generated

    // Metrics
    uint32_t            referenceCount;
    uint32_t            complexityCyclomatic;

    SymbolEntry()
        : symbolId(0), kind(SymbolKind::Unknown)
        , visibility(SymbolVisibility::Public), typeId(0)
        , parentSymbolId(0), isStatic(false), isVirtual(false)
        , isAbstract(false), isInline(false), isConstexpr(false)
        , isDeprecated(false), isGenerated(false)
        , referenceCount(0), complexityCyclomatic(0) {}
};

// ============================================================================
// Cross-Reference Entry — Records where a symbol is used
// ============================================================================
enum class ReferenceKind : uint8_t {
    Read        = 0,
    Write       = 1,
    Call        = 2,
    TypeRef     = 3,
    Inherit     = 4,
    Override    = 5,
    Implement   = 6,
    Import      = 7,
    Instantiate = 8,
    AddressOf   = 9
};

struct CrossReference {
    uint64_t            refId;
    uint64_t            symbolId;        // Referenced symbol
    uint64_t            fromSymbolId;    // Referencing context (function, method)
    SourceLocation      location;
    ReferenceKind       kind;
    bool                isImplicit;      // Compiler-generated reference

    CrossReference()
        : refId(0), symbolId(0), fromSymbolId(0)
        , kind(ReferenceKind::Read), isImplicit(false) {}
};

// ============================================================================
// Call Graph Entry
// ============================================================================
struct CallGraphEdge {
    uint64_t    callerId;       // Calling function symbol ID
    uint64_t    calleeId;       // Called function symbol ID
    SourceLocation callSite;
    bool        isVirtual;      // Virtual/dynamic dispatch
    bool        isIndirect;     // Function pointer call
    uint32_t    callCount;      // Estimated or profiled call count

    CallGraphEdge()
        : callerId(0), calleeId(0), isVirtual(false)
        , isIndirect(false), callCount(1) {}
};

// ============================================================================
// Scope — For symbol resolution
// ============================================================================
struct Scope {
    uint64_t            scopeId;
    std::string         name;
    uint64_t            parentScopeId;
    SymbolKind          kind;           // Namespace, Class, Function, Block
    std::vector<uint64_t> symbolIds;
    std::vector<uint64_t> childScopes;
    SourceLocation      range;

    Scope()
        : scopeId(0), parentScopeId(0), kind(SymbolKind::Unknown) {}
};

// ============================================================================
// Completion Item — For autocomplete
// ============================================================================
struct CompletionItem {
    std::string     label;
    std::string     detail;         // Type info
    std::string     documentation;
    std::string     insertText;
    SymbolKind      kind;
    int32_t         sortOrder;      // Lower = higher priority
    bool            isSnippet;

    CompletionItem()
        : kind(SymbolKind::Unknown), sortOrder(100), isSnippet(false) {}
};

// ============================================================================
// Hover Info — For tooltip display
// ============================================================================
struct HoverInfo {
    std::string     signature;
    std::string     typeInfo;
    std::string     documentation;
    std::string     definedIn;       // File path
    uint32_t        definedLine;

    HoverInfo() : definedLine(0) {}
};

// ============================================================================
// Intelligence Query Result
// ============================================================================
struct IntelligenceResult {
    bool        success;
    const char* detail;
    int64_t     queryTimeUs;

    static IntelligenceResult ok(int64_t us = 0) {
        return { true, "Query completed", us };
    }
    static IntelligenceResult error(const char* msg) {
        return { false, msg, 0 };
    }
};

// ============================================================================
// Intelligence Statistics
// ============================================================================
struct IntelligenceStats {
    std::atomic<uint64_t> totalSymbols{0};
    std::atomic<uint64_t> totalReferences{0};
    std::atomic<uint64_t> totalTypes{0};
    std::atomic<uint64_t> totalScopes{0};
    std::atomic<uint64_t> filesIndexed{0};
    std::atomic<uint64_t> queriesServed{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    std::atomic<uint64_t> indexBuildTimeUs{0};
};

// ============================================================================
// Callbacks
// ============================================================================
using IndexProgressCallback = void(*)(const char* filePath, uint32_t pct, void* userData);
using IndexCompleteCallback = void(*)(uint64_t symbolCount, uint64_t refCount, void* userData);

// ============================================================================
// SemanticCodeIntelligence — Main class
// ============================================================================
class SemanticCodeIntelligence {
public:
    static SemanticCodeIntelligence& instance();

    // ── Symbol Registration ──
    uint64_t addSymbol(const SymbolEntry& symbol);
    PatchResult updateSymbol(uint64_t symbolId, const SymbolEntry& updated);
    PatchResult removeSymbol(uint64_t symbolId);

    // ── Type Registration ──
    uint64_t addType(const TypeInfo& type);
    PatchResult updateType(uint64_t typeId, const TypeInfo& updated);
    const TypeInfo* getType(uint64_t typeId) const;

    // ── Scope Management ──
    uint64_t pushScope(const std::string& name, SymbolKind kind, uint64_t parentScope);
    PatchResult popScope(uint64_t scopeId);
    uint64_t resolveSymbolInScope(const std::string& name, uint64_t scopeId) const;

    // ── Cross-Reference ──
    PatchResult addReference(const CrossReference& ref);
    std::vector<CrossReference> getReferences(uint64_t symbolId) const;
    std::vector<CrossReference> getReferencesInFile(const std::string& filePath) const;
    uint32_t getReferenceCount(uint64_t symbolId) const;

    // ── Call Graph ──
    PatchResult addCallEdge(const CallGraphEdge& edge);
    std::vector<CallGraphEdge> getCallersOf(uint64_t functionId) const;
    std::vector<CallGraphEdge> getCalleesOf(uint64_t functionId) const;
    std::vector<uint64_t> getCallChain(uint64_t functionId, uint32_t maxDepth = 10) const;

    // ── Navigation ──
    const SymbolEntry* goToDefinition(const std::string& name, const SourceLocation& context) const;
    std::vector<SourceLocation> findAllReferences(uint64_t symbolId) const;
    std::vector<const SymbolEntry*> findImplementations(uint64_t interfaceSymbolId) const;
    std::vector<const SymbolEntry*> getTypeHierarchy(uint64_t typeSymbolId) const;
    const SymbolEntry* findSymbolAt(const std::string& file, uint32_t line, uint32_t col) const;

    // ── Autocomplete ──
    std::vector<CompletionItem> getCompletions(const std::string& prefix,
                                                uint64_t scopeId,
                                                uint32_t maxResults = 50) const;

    // ── Hover Info ──
    HoverInfo getHoverInfo(uint64_t symbolId) const;

    // ── Search ──
    std::vector<const SymbolEntry*> searchSymbols(const std::string& query,
                                                   SymbolKind filterKind = SymbolKind::Unknown,
                                                   uint32_t maxResults = 100) const;
    std::vector<const SymbolEntry*> getSymbolsInFile(const std::string& filePath) const;

    // ── Indexing ──
    PatchResult indexFile(const std::string& filePath);
    PatchResult reindexFile(const std::string& filePath);
    PatchResult removeFileIndex(const std::string& filePath);
    PatchResult rebuildIndex();

    // ── Diagnostics ──
    std::vector<std::string> findUnusedSymbols() const;
    std::vector<std::pair<uint64_t, uint64_t>> findCircularDependencies() const;

    // ── Statistics ──
    const IntelligenceStats& getStats() const { return m_stats; }
    void resetStats();

    // ── Callbacks ──
    void setProgressCallback(IndexProgressCallback cb, void* userData);
    void setCompleteCallback(IndexCompleteCallback cb, void* userData);

    // ── Serialization ──
    PatchResult saveIndex(const char* filePath) const;
    PatchResult loadIndex(const char* filePath);

private:
    SemanticCodeIntelligence();
    ~SemanticCodeIntelligence();
    SemanticCodeIntelligence(const SemanticCodeIntelligence&) = delete;
    SemanticCodeIntelligence& operator=(const SemanticCodeIntelligence&) = delete;

    // Internal helpers
    bool matchesPrefix(const std::string& name, const std::string& prefix) const;
    bool fuzzyMatch(const std::string& name, const std::string& query) const;
    void buildFileIndex(const std::string& filePath);

    // State
    mutable std::mutex                              m_mutex;
    std::atomic<uint64_t>                           m_nextSymbolId{1};
    std::atomic<uint64_t>                           m_nextTypeId{1};
    std::atomic<uint64_t>                           m_nextScopeId{1};
    std::atomic<uint64_t>                           m_nextRefId{1};

    // Symbol database
    std::unordered_map<uint64_t, SymbolEntry>       m_symbols;
    std::unordered_map<std::string, std::vector<uint64_t>> m_nameIndex;  // name → symbol IDs
    std::unordered_map<std::string, std::vector<uint64_t>> m_fileIndex;  // file → symbol IDs

    // Type database
    std::unordered_map<uint64_t, TypeInfo>          m_types;
    std::unordered_map<std::string, uint64_t>       m_typeNameIndex;

    // Scope tree
    std::unordered_map<uint64_t, Scope>             m_scopes;

    // Cross-references
    std::deque<CrossReference>                      m_references;
    std::unordered_map<uint64_t, std::vector<size_t>> m_refBySymbol;  // symbol → ref indices
    static constexpr size_t MAX_REFERENCES = 500000;

    // Call graph
    std::vector<CallGraphEdge>                      m_callGraph;
    std::unordered_map<uint64_t, std::vector<size_t>> m_callerIndex;
    std::unordered_map<uint64_t, std::vector<size_t>> m_calleeIndex;

    // Callbacks
    IndexProgressCallback   m_progressCb   = nullptr;
    void*                   m_progressData = nullptr;
    IndexCompleteCallback   m_completeCb   = nullptr;
    void*                   m_completeData = nullptr;

    // Statistics
    mutable IntelligenceStats       m_stats;
};

#endif // RAWRXD_SEMANTIC_CODE_INTELLIGENCE_HPP
