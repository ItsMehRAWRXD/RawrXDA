// ============================================================================
// multi_file_transaction.h — Multi-File Agent Transaction System
// ============================================================================
// Data structures and algorithms for atomic multi-file edits with
// dependency tracking, topological ordering, and rollback support.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <mutex>
#include <functional>
#include <optional>

namespace RawrXD {
namespace Agent {

// ============================================================================
// Symbol Reference — cross-file symbol tracking
// ============================================================================

struct SymbolRef {
    std::string name;              // Fully qualified symbol name
    std::string declaredIn;        // File where symbol is defined
    int line;                      // Line number
    enum Kind {
        FUNCTION, CLASS, STRUCT, ENUM, VARIABLE, MACRO, TYPEDEF, NAMESPACE
    } kind;
};

struct SymbolDef {
    std::string name;
    std::string file;
    int startLine;
    int endLine;
    SymbolRef::Kind kind;
    std::string signature;         // Function signature or type definition
    std::vector<std::string> usedBy;  // Files that reference this symbol
};

// ============================================================================
// FileEditNode — a single file in a multi-file transaction
// ============================================================================

struct FileEditNode {
    std::filesystem::path path;
    std::string originalHash;          // SHA-256 pre-edit
    std::string proposedContent;
    std::string originalContent;       // For rollback
    std::vector<SymbolRef> dependencies;  // Cross-file symbols touched
    uint64_t originalModTime;          // mtime before edit
    bool applied;                      // Whether this edit was written to disk

    struct EditRegion {
        int startLine;
        int endLine;
        std::string oldText;
        std::string newText;
    };
    std::vector<EditRegion> regions;   // Granular edit regions
};

// ============================================================================
// Include Graph — #include relationship tracking
// ============================================================================

struct IncludeEdge {
    std::string includer;        // File that contains the #include
    std::string includee;        // File being included
    int line;                    // Line number of #include directive
    bool isSystemInclude;        // <header> vs "header"
};

// ============================================================================
// DependencyGraph — cross-file dependency analysis
// ============================================================================

struct DependencyGraph {
    std::unordered_map<std::string, std::vector<std::string>> includeGraph;
    std::unordered_map<std::string, std::vector<SymbolDef>> exportMap;
    std::vector<IncludeEdge> edges;

    // Compute topological sort of file edit order
    // Returns empty if cycle detected
    std::vector<std::string> topologicalSort() const;

    // Get all files transitively dependent on a given file
    std::vector<std::string> transitiveDependents(const std::string& file) const;

    // Check for circular dependencies
    bool hasCycle() const;

    // Add an include relationship
    void addInclude(const std::string& includer, const std::string& includee,
                    int line = 0, bool isSystem = false);

    // Build from file system (parse #include directives)
    void buildFromDirectory(const std::filesystem::path& root,
                            const std::vector<std::string>& extensions = {".h", ".hpp", ".cpp", ".c"});
};

// ============================================================================
// Include Parser — fast regex-based #include extraction
// ============================================================================

struct IncludeInfo {
    std::string header;
    int line;
    bool isSystem;  // <...> vs "..."
};

std::vector<IncludeInfo> parseIncludes(const std::string& fileContent);
std::vector<IncludeInfo> parseIncludesFromFile(const std::filesystem::path& filePath);

// ============================================================================
// Conflict Detection — three-way merge for overlapping edits
// ============================================================================

struct MergeConflict {
    std::string file;
    int startLine;
    int endLine;
    std::string agentVersion;
    std::string userVersion;
    std::string baseVersion;
    bool autoResolvable;
};

struct MergeResult {
    bool success;
    std::string mergedContent;
    std::vector<MergeConflict> conflicts;
    int conflictCount;
};

MergeResult threeWayMerge(const std::string& base,
                           const std::string& agentEdit,
                           const std::string& userEdit);

// ============================================================================
// Transaction State
// ============================================================================

enum class TransactionState : uint8_t {
    PREPARING,      // Building edit list
    VALIDATING,     // Checking dependencies, conflicts
    APPLYING,       // Writing to disk
    COMMITTED,      // All writes successful
    ROLLING_BACK,   // Undoing applied writes
    ROLLED_BACK,    // Rollback complete
    FAILED          // Unrecoverable failure
};

const char* transactionStateName(TransactionState state);

// ============================================================================
// Transaction Result
// ============================================================================

struct TransactionResult {
    bool success;
    const char* detail;
    int errorCode;
    TransactionState finalState;

    static TransactionResult ok(const char* msg = "Committed") {
        return {true, msg, 0, TransactionState::COMMITTED};
    }
    static TransactionResult error(const char* msg, int code = -1) {
        return {false, msg, code, TransactionState::FAILED};
    }
    static TransactionResult rolledBack(const char* msg) {
        return {false, msg, 0, TransactionState::ROLLED_BACK};
    }
};

// ============================================================================
// MultiFileTransaction — orchestrates atomic multi-file edits
// ============================================================================

class MultiFileTransaction {
public:
    explicit MultiFileTransaction(const std::string& description = "");
    ~MultiFileTransaction();

    // Non-copyable
    MultiFileTransaction(const MultiFileTransaction&) = delete;
    MultiFileTransaction& operator=(const MultiFileTransaction&) = delete;

    // ---- Build Phase ----
    TransactionResult addFileEdit(const std::filesystem::path& path,
                                   const std::string& newContent);
    TransactionResult addRegionEdit(const std::filesystem::path& path,
                                     int startLine, int endLine,
                                     const std::string& newText);

    // ---- Dependency Phase ----
    void setDependencyGraph(const DependencyGraph& graph);
    TransactionResult validateDependencies();

    // ---- Conflict Detection ----
    TransactionResult checkConflicts();
    std::vector<MergeConflict> getConflicts() const;

    // ---- Execution Phase ----
    TransactionResult commit();
    TransactionResult rollback();

    // ---- Query ----
    TransactionState state() const { return m_state; }
    const std::vector<FileEditNode>& edits() const { return m_edits; }
    size_t editCount() const { return m_edits.size(); }
    const std::string& description() const { return m_description; }
    const std::string& transactionId() const { return m_txId; }

    // ---- File Watcher Integration ----
    using ExternalEditCallback = std::function<void(const std::filesystem::path&)>;
    void setExternalEditCallback(ExternalEditCallback cb);
    bool hasExternalEdits() const;

private:
    // Atomic write: write to temp file, then rename
    TransactionResult atomicWrite(const std::filesystem::path& path,
                                   const std::string& content);
    std::string computeSHA256(const std::string& data);
    std::string readFileContent(const std::filesystem::path& path);
    std::string generateTransactionId();

    std::vector<FileEditNode> m_edits;
    DependencyGraph m_depGraph;
    TransactionState m_state;
    std::string m_description;
    std::string m_txId;
    mutable std::mutex m_mutex;
    ExternalEditCallback m_externalEditCb;
    std::vector<MergeConflict> m_conflicts;
};

// ============================================================================
// File Watcher — ReadDirectoryChangesW integration
// ============================================================================

class FileWatcher {
public:
    FileWatcher();
    ~FileWatcher();

    using ChangeCallback = std::function<void(const std::filesystem::path& path,
                                               const std::string& action)>;

    TransactionResult watch(const std::filesystem::path& directory,
                             ChangeCallback callback,
                             bool recursive = true);
    void stopAll();
    bool isWatching() const;

private:
    struct WatchHandle;
    std::vector<std::unique_ptr<WatchHandle>> m_handles;
    std::mutex m_mutex;
};

} // namespace Agent
} // namespace RawrXD
