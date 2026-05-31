// ============================================================================
// ast_graph_engine.hpp — P1: Persistent Incremental AST Graph
// ============================================================================
// Eliminates AST recomputation per interaction through:
//   - Versioned node diffs (only changed nodes updated)
//   - Persistent data structure (immutable snapshots)
//   - Incremental parsing (re-parse only changed regions)
//   - Graph-based symbol relationships (not linear text)
//
// Expected impact: 30-60% reduction in IDE CPU overhead
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <optional>
#include <thread>

namespace RawrXD {
namespace AST {

// Forward declarations
class ASTNode;
class ASTGraph;
class IncrementalParser;

// ============================================================================
// Node Types
// ============================================================================
enum class NodeType : uint8_t {
    // Declarations
    TranslationUnit,
    NamespaceDecl,
    ClassDecl,
    StructDecl,
    FunctionDecl,
    VariableDecl,
    EnumDecl,
    TypedefDecl,
    UsingDecl,
    
    // Statements
    CompoundStmt,
    IfStmt,
    ForStmt,
    WhileStmt,
    DoStmt,
    SwitchStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
    ExprStmt,
    
    // Expressions
    CallExpr,
    MemberExpr,
    BinaryExpr,
    UnaryExpr,
    LiteralExpr,
    IdentifierExpr,
    TemplateExpr,
    LambdaExpr,
    
    // Types
    BuiltinType,
    PointerType,
    ReferenceType,
    ArrayType,
    FunctionType,
    
    // Other
    TemplateParam,
    Comment,
    Preprocessor,
    MacroDecl,
    
    Unknown = 255
};

// ============================================================================
// Source Location (compact, 64-bit)
// ============================================================================
struct SourceLocation {
    uint32_t line : 24;      // 16M lines max
    uint32_t column : 16;    // 64K columns max
    uint32_t file_id : 24;   // 16M files max
    
    bool operator==(const SourceLocation& other) const {
        return line == other.line && column == other.column && file_id == other.file_id;
    }

    bool operator<=(const SourceLocation& other) const {
        return !(other < *this);
    }

    bool operator<(const SourceLocation& other) const {
        if (file_id != other.file_id) return file_id < other.file_id;
        if (line != other.line) return line < other.line;
        return column < other.column;
    }
};

// ============================================================================
// Source Range
// ============================================================================
struct SourceRange {
    SourceLocation start;
    SourceLocation end;
    
    bool contains(const SourceLocation& loc) const {
        return start <= loc && loc < end;
    }
    
    bool overlaps(const SourceRange& other) const {
        return start < other.end && other.start < end;
    }
};

// ============================================================================
// Node Hash (for change detection)
// ============================================================================
using NodeHash = uint64_t;

// ============================================================================
// Rust-specific metadata (zero-overhead when unused)
// ============================================================================
struct RustMeta {
    bool is_pub = false;
    bool is_async = false;
    bool is_unsafe = false;
    bool is_const = false;
    bool is_extern = false;
    bool is_static = false;
    bool is_mut = false;
    std::string visibility;              // "pub", "pub(crate)", etc.
    std::vector<std::string> attributes; // #[derive(...)] etc.
    std::string doc;                     // accumulated /// doc comments
};

// ============================================================================
// AST Node (immutable, versioned)
// ============================================================================
class ASTNode : public std::enable_shared_from_this<ASTNode> {
public:
    using Ptr = std::shared_ptr<const ASTNode>;
    using WeakPtr = std::weak_ptr<const ASTNode>;
    
    // Construction
    ASTNode(NodeType type, SourceRange range, std::string text);
    
    // Immutable: create modified version
    Ptr withChildren(std::vector<Ptr> new_children) const;
    Ptr withParent(Ptr new_parent) const;
    Ptr withHash(NodeHash new_hash) const;
    
    // Accessors
    NodeType getType() const { return type_; }
    SourceRange getRange() const { return range_; }
    const std::string& getText() const { return text_; }
    NodeHash getHash() const { return hash_; }
    uint64_t getVersion() const { return version_; }
    
    // Graph traversal
    Ptr getParent() const { return parent_.lock(); }
    const std::vector<Ptr>& getChildren() const { return children_; }
    Ptr getChild(size_t index) const;
    size_t getChildCount() const { return children_.size(); }
    
    // Symbol relationships
    const std::vector<Ptr>& getReferences() const { return references_; }
    const std::vector<Ptr>& getDefinitions() const { return definitions_; }
    
    // Queries
    bool isDeclaration() const;
    bool isStatement() const;
    bool isExpression() const;
    bool isType() const;
    std::string getName() const;
    std::string getQualifiedName() const;
    
    // Find node at location (binary search on children)
    Ptr findNodeAt(const SourceLocation& loc) const;
    
    // Find all nodes of type
    void findNodesOfType(NodeType type, std::vector<Ptr>& results) const;
    
    // Distance in AST graph (for symbol scoring)
    size_t graphDistanceTo(Ptr other) const;

    // Rust metadata (optional, populated by Rust parser)
    RustMeta rust_meta_;

private:
    NodeType type_;
    SourceRange range_;
    std::string text_;
    NodeHash hash_;
    uint64_t version_;
    
    WeakPtr parent_;
    std::vector<Ptr> children_;
    
    // Symbol relationships (populated by semantic analysis)
    std::vector<Ptr> references_;
    std::vector<Ptr> definitions_;
    
    // Cached computations
    mutable std::optional<std::string> cached_name_;
    mutable std::optional<std::string> cached_qualified_name_;
    mutable std::shared_mutex cache_mutex_;
    
    static std::atomic<uint64_t> next_version_;
};

// ============================================================================
// File AST (root for a single file)
// ============================================================================
struct FileAST {
    std::string file_path;
    ASTNode::Ptr root;
    uint64_t version;
    std::chrono::steady_clock::time_point last_modified;
    
    // Incremental parsing state
    std::vector<std::pair<SourceRange, NodeHash>> region_hashes;
};

// ============================================================================
// Change Delta (what changed between versions)
// ============================================================================
struct ASTDelta {
    std::string file_path;
    uint64_t old_version;
    uint64_t new_version;
    
    // Changed regions
    std::vector<SourceRange> modified_regions;
    std::vector<SourceRange> inserted_regions;
    std::vector<SourceRange> deleted_regions;
    
    // Affected nodes (for cache invalidation)
    std::vector<ASTNode::Ptr> invalidated_nodes;
    std::vector<ASTNode::Ptr> new_nodes;
};

// ============================================================================
// Symbol Table Entry
// ============================================================================
struct SymbolEntry {
    std::string name;
    std::string qualified_name;
    ASTNode::Ptr declaration;
    std::vector<ASTNode::Ptr> references;
    NodeType kind;
    std::string type_signature;  // For functions/variables
};

// ============================================================================
// AST Graph Engine (singleton)
// ============================================================================
class ASTGraphEngine {
public:
    static ASTGraphEngine& instance();
    
    // Initialize with thread pool size
    void initialize(size_t num_threads = std::thread::hardware_concurrency());
    void shutdown();
    
    // File management
    void registerFile(const std::string& path, const std::string& content);
    void updateFile(const std::string& path, const std::string& new_content);
    void updateFileIncremental(const std::string& path, 
                               const std::string& new_content,
                               const SourceRange& changed_range);
    void unregisterFile(const std::string& path);
    
    // Query
    std::optional<FileAST> getFileAST(const std::string& path) const;
    ASTNode::Ptr findNodeAt(const std::string& path, const SourceLocation& loc) const;
    std::vector<ASTNode::Ptr> findSymbols(const std::string& name) const;
    std::vector<ASTNode::Ptr> findSymbolsMatching(const std::string& pattern) const;
    
    // Symbol relationships
    std::vector<ASTNode::Ptr> getReferences(const ASTNode::Ptr& declaration) const;
    std::vector<ASTNode::Ptr> getDefinitions(const ASTNode::Ptr& reference) const;
    std::vector<ASTNode::Ptr> getCallers(const ASTNode::Ptr& function) const;
    std::vector<ASTNode::Ptr> getCallees(const ASTNode::Ptr& function) const;
    
    // Context-aware queries (for completion)
    std::vector<SymbolEntry> getSymbolsInScope(const SourceLocation& loc) const;
    std::vector<SymbolEntry> getSymbolsAccessibleFrom(const ASTNode::Ptr& node) const;
    
    // Graph distance scoring (for completion ranking)
    float computeRelevanceScore(const ASTNode::Ptr& symbol, 
                                const SourceLocation& cursor) const;
    
    // Delta tracking
    std::optional<ASTDelta> getLastDelta(const std::string& path) const;
    void clearDeltas();
    
    // Statistics
    struct Stats {
        size_t files_tracked;
        size_t total_nodes;
        size_t total_symbols;
        size_t incremental_updates;
        size_t full_reparses;
        double avg_parse_time_ms;
    };
    Stats getStats() const;

private:
    ASTGraphEngine() = default;
    ~ASTGraphEngine() = default;
    
    ASTGraphEngine(const ASTGraphEngine&) = delete;
    ASTGraphEngine& operator=(const ASTGraphEngine&) = delete;
    
    // Internal
    FileAST parseFull(const std::string& path, const std::string& content);
    FileAST parseIncremental(const std::string& path, 
                             const FileAST& old_ast,
                             const std::string& new_content,
                             const SourceRange& changed_range);
    void updateSymbolTable(const std::string& path, const ASTDelta& delta);
    void computeRegionHashes(FileAST& file_ast);
    
    // Data
    mutable std::shared_mutex files_mutex_;
    std::unordered_map<std::string, FileAST> files_;
    
    mutable std::shared_mutex symbols_mutex_;
    std::unordered_map<std::string, SymbolEntry> symbols_by_qualified_name_;
    std::unordered_map<std::string, std::vector<std::string>> symbols_by_name_;
    
    mutable std::shared_mutex deltas_mutex_;
    std::unordered_map<std::string, ASTDelta> last_deltas_;
    
    // Thread pool for parsing
    std::unique_ptr<class ThreadPool> thread_pool_;
    
    // Stats
    mutable std::mutex stats_mutex_;
    Stats stats_;
};

// ============================================================================
// C API for integration
// ============================================================================
extern "C" {
    typedef void* RawrXD_ASTEngine;
    typedef void* RawrXD_ASTNode;
    
    RawrXD_ASTEngine rawrxd_ast_engine_get();
    void rawrxd_ast_engine_initialize(RawrXD_ASTEngine engine, uint32_t num_threads);
    void rawrxd_ast_engine_shutdown(RawrXD_ASTEngine engine);
    
    void rawrxd_ast_register_file(RawrXD_ASTEngine engine, const char* path, const char* content);
    void rawrxd_ast_update_file(RawrXD_ASTEngine engine, const char* path, const char* content);
    RawrXD_ASTNode rawrxd_ast_find_node_at(RawrXD_ASTEngine engine, const char* path, 
                                             uint32_t line, uint32_t column);
    
    // Returns array of symbol names (caller must free)
    char** rawrxd_ast_get_symbols_in_scope(RawrXD_ASTEngine engine, const char* path,
                                           uint32_t line, uint32_t column, size_t* count);
    void rawrxd_ast_free_string_array(char** arr, size_t count);
}

} // namespace AST
} // namespace RawrXD
