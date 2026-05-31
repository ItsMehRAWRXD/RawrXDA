// ============================================================================
// ast_graph_engine.h — Persistent Incremental AST Graph Engine
// ============================================================================
// Replaces per-request AST recomputation with a persistent, versioned,
// incremental AST graph that serves as the single source of truth for:
//   - CompletionEngine (context-aware completions)
//   - LSP (semantic analysis)
//   - Slash commands (semantic-aware commands)
//   - Measurement (structural telemetry)
//
// Key Design:
//   - Nodes are immutable once created (copy-on-write for updates)
//   - Versioned snapshots enable time-travel debugging
//   - Incremental diff propagation (only changed subtrees re-analyzed)
//   - Graph-based context queries (not linear text buffers)
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <functional>

namespace RawrXD {
namespace AST {

// ============================================================================
// Node Types
// ============================================================================
enum class NodeType : uint8_t {
    // Structural
    Root,
    Namespace,
    Class,
    Struct,
    Function,
    Lambda,
    Block,
    
    // Declarations
    Variable,
    Parameter,
    TemplateParam,
    
    // Statements
    If,
    For,
    While,
    DoWhile,
    Switch,
    Return,
    Break,
    Continue,
    ExpressionStmt,
    
    // Expressions
    BinaryOp,
    UnaryOp,
    Call,
    MemberAccess,
    ArrayAccess,
    Literal,
    Identifier,
    This,
    
    // Types
    TypeRef,
    PointerType,
    ReferenceType,
    ArrayType,
    TemplateType,
    
    // Special
    Comment,
    Preprocessor,
    Error,          // Parse error recovery node
};

// ============================================================================
// Node ID — unique identifier for graph nodes
// ============================================================================
using NodeID = uint64_t;
constexpr NodeID INVALID_NODE_ID = 0;

// ============================================================================
// Source Location — maps AST back to source code
// ============================================================================
struct SourceLocation {
    uint32_t    fileID;         // Index into file table
    uint32_t    line;           // 1-based
    uint32_t    column;           // 0-based byte offset
    uint32_t    length;         // Byte length of token/node
    
    bool operator==(const SourceLocation& other) const {
        return fileID == other.fileID && line == other.line && 
               column == other.column && length == other.length;
    }
};

struct SourceLocationHash {
    size_t operator()(const SourceLocation& loc) const {
        return std::hash<uint64_t>{}(
            (static_cast<uint64_t>(loc.fileID) << 32) | 
            (static_cast<uint64_t>(loc.line) << 16) | 
            loc.column
        );
    }
};

// ============================================================================
// AST Node — immutable, versioned graph node
// ============================================================================
struct ASTNode {
    const NodeID            id;
    const NodeType          type;
    const SourceLocation    location;
    const uint64_t          version;        // Monotonic version for cache invalidation
    
    // Content
    std::string             text;           // Source text span
    std::string             symbol;         // Resolved symbol name (if applicable)
    
    // Graph structure (immutable - use copy-on-write for modifications)
    std::vector<NodeID>     children;       // Ordered child nodes
    NodeID                  parent;         // Parent node (INVALID_NODE_ID for root)
    std::vector<NodeID>     references;     // Cross-references (symbols referenced)
    
    // Semantic info
    NodeID                  resolvedType;     // Type reference (for expressions)
    NodeID                  scope;            // Enclosing scope node
    
    // Flags
    bool                    isComplete;     // Fully parsed (not partial)
    bool                    hasErrors;      // Contains error nodes
    bool                    isDeprecated;   // Marked deprecated
    
    // Constructor for immutable creation
    ASTNode(NodeID id_, NodeType type_, SourceLocation loc_, uint64_t ver_)
        : id(id_), type(type_), location(loc_), version(ver_)
        , parent(INVALID_NODE_ID), resolvedType(INVALID_NODE_ID)
        , scope(INVALID_NODE_ID), isComplete(false), hasErrors(false)
        , isDeprecated(false) {}
};

// ============================================================================
// Graph Version — snapshot identifier
// ============================================================================
using GraphVersion = uint64_t;

// ============================================================================
// Incremental Diff — describes changes between versions
// ============================================================================
struct GraphDiff {
    GraphVersion            fromVersion;
    GraphVersion            toVersion;
    std::vector<NodeID>     addedNodes;     // New nodes in this version
    std::vector<NodeID>     modifiedNodes;  // Changed nodes (new versions)
    std::vector<NodeID>     deletedNodes;   // Removed nodes
    std::vector<NodeID>     invalidatedCaches; // Nodes whose analysis is stale
};

// ============================================================================
// Context Fingerprint — for completion caching
// ============================================================================
struct ContextFingerprint {
    NodeID                  cursorNode;     // AST node at cursor
    NodeID                  scopeNode;      // Enclosing scope
    std::string             partialSymbol;  // Partial identifier being typed
    uint64_t                surroundingHash; // Hash of context nodes
    
    bool operator==(const ContextFingerprint& other) const {
        return cursorNode == other.cursorNode && 
               scopeNode == other.scopeNode &&
               partialSymbol == other.partialSymbol &&
               surroundingHash == other.surroundingHash;
    }
};

struct ContextFingerprintHash {
    size_t operator()(const ContextFingerprint& fp) const {
        return std::hash<uint64_t>{}(fp.cursorNode ^ fp.scopeNode ^ fp.surroundingHash);
    }
};

// ============================================================================
// Completion Cache Entry
// ============================================================================
struct CompletionCacheEntry {
    ContextFingerprint      fingerprint;
    std::vector<std::string> completions;
    uint64_t                timestamp;
    uint32_t                hitCount;
};

// ============================================================================
// AST Graph Engine — Persistent Incremental AST
// ============================================================================
class ASTGraphEngine {
public:
    ASTGraphEngine();
    ~ASTGraphEngine();

    // ---- Lifecycle ----
    bool initialize();
    void shutdown();
    
    // ---- File Management ----
    // Register a source file for tracking
    uint32_t registerFile(const std::string& path, const std::string& content);
    
    // Update file content (incremental parse)
    // Returns diff describing what changed
    GraphDiff updateFile(uint32_t fileID, const std::string& newContent);
    
    // Remove file from tracking
    void unregisterFile(uint32_t fileID);
    
    // ---- Graph Queries ----
    // Get node by ID
    const ASTNode* getNode(NodeID id) const;
    
    // Find node at source location
    NodeID findNodeAt(uint32_t fileID, uint32_t line, uint32_t column) const;
    
    // Get root node for file
    NodeID getFileRoot(uint32_t fileID) const;
    
    // Get enclosing scope for node
    NodeID getEnclosingScope(NodeID nodeID) const;
    
    // Get symbol definition
    NodeID resolveSymbol(const std::string& symbol, NodeID scopeNode) const;
    
    // Get all references to a symbol
    std::vector<NodeID> getReferences(NodeID definitionNode) const;
    
    // ---- Graph Traversal ----
    // Walk the AST (depth-first)
    void traverse(NodeID root, std::function<void(const ASTNode&)> visitor) const;
    
    // Find nodes matching predicate
    std::vector<NodeID> findNodes(NodeID root, 
                                   std::function<bool(const ASTNode&)> predicate) const;
    
    // Get path from root to node
    std::vector<NodeID> getPathToRoot(NodeID nodeID) const;
    
    // ---- Incremental Updates ----
    // Apply a diff to update the graph
    void applyDiff(const GraphDiff& diff);
    
    // Get diff between two versions
    GraphDiff getDiff(GraphVersion from, GraphVersion to) const;
    
    // ---- Completion Integration ----
    // Build context fingerprint for completion
    ContextFingerprint buildFingerprint(uint32_t fileID, uint32_t line, 
                                           uint32_t column, const std::string& partial);
    
    // Cache completion results
    void cacheCompletions(const ContextFingerprint& fingerprint, 
                          const std::vector<std::string>& completions);
    
    // Get cached completions (returns nullptr if miss)
    const std::vector<std::string>* getCachedCompletions(
        const ContextFingerprint& fingerprint);
    
    // Invalidate completions affected by diff
    void invalidateCompletionCache(const GraphDiff& diff);
    
    // ---- DAG Integration ----
    // Get current graph version (for scheduler coordination)
    GraphVersion getCurrentVersion() const { return m_currentVersion.load(); }
    
    // Wait for version (blocking until graph reaches version)
    bool awaitVersion(GraphVersion version, uint64_t timeoutMs);
    
    // ---- Statistics ----
    struct Stats {
        size_t      totalNodes;
        size_t      totalFiles;
        size_t      cacheHits;
        size_t      cacheMisses;
        size_t      incrementalUpdates;
        size_t      fullReparses;
        double      avgParseTimeMs;
        double      avgDiffTimeMs;
    };
    Stats getStats() const;
    
private:
    // ---- Node Storage ----
    std::unordered_map<NodeID, std::unique_ptr<ASTNode>> m_nodes;
    mutable std::shared_mutex m_nodeMutex;
    std::atomic<NodeID> m_nextNodeID{1};
    
    // ---- File Tracking ----
    struct FileInfo {
        std::string         path;
        std::string         content;
        NodeID              rootNode;
        GraphVersion        lastParsedVersion;
        uint64_t            lastModified;
    };
    std::unordered_map<uint32_t, FileInfo> m_files;
    std::unordered_map<std::string, uint32_t> m_pathToFileID;
    mutable std::shared_mutex m_fileMutex;
    std::atomic<uint32_t> m_nextFileID{1};
    
    // ---- Versioning ----
    std::atomic<GraphVersion> m_currentVersion{1};
    std::unordered_map<GraphVersion, GraphDiff> m_versionDiffs;
    mutable std::shared_mutex m_versionMutex;
    
    // ---- Completion Cache ----
    std::unordered_map<ContextFingerprint, CompletionCacheEntry, 
                        ContextFingerprintHash> m_completionCache;
    mutable std::shared_mutex m_cacheMutex;
    std::atomic<uint64_t> m_cacheTimestamp{0};
    
    // ---- Statistics ----
    mutable std::atomic<size_t> m_cacheHits{0};
    mutable std::atomic<size_t> m_cacheMisses{0};
    mutable std::atomic<size_t> m_incrementalUpdates{0};
    mutable std::atomic<size_t> m_fullReparses{0};
    
    // ---- Internal Methods ----
    NodeID allocateNodeID();
    uint32_t allocateFileID();
    GraphDiff computeDiff(const FileInfo& oldFile, const FileInfo& newFile);
    void pruneOldVersions();
};

// ============================================================================
// Global Instance
// ============================================================================
ASTGraphEngine& getASTGraphEngine();

} // namespace AST
} // namespace RawrXD
