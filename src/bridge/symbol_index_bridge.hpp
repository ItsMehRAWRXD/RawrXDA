// ============================================================================
// symbol_index_bridge.hpp — Phase 1a: Parser → Ghost Text Bridge
// ============================================================================
// Connects rust_parser_v2 output to the completion engine.
//
// Usage:
//   SymbolIndexBridge bridge;
//   bridge.initialize();
//   bridge.indexFile("main.rs", source_code);
//   auto candidates = bridge.queryCompletions("main.rs", "pri", 42, 10);
// ============================================================================

#pragma once

#include "rust_parser.hpp"
#include "symbol_table.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <thread>
#include <atomic>

namespace rawrxd {
namespace bridge {

// ============================================================================
// Completion Types
// ============================================================================

enum class CompletionKind {
    Function,
    Variable,
    Type,
    Enum,
    Module,
    Trait,
    Macro,
    Field,
    Other
};

// Compatibility alias for AST/ghost-text bridge call sites
enum class SymbolKind {
    Unknown = 0,
    Function,
    Method,
    Variable,
    Field,
    Type,
    Enum,
    Module,
    Trait,
    Macro,
    Other
};

enum class Accessibility {
    Public = 0,
    Protected,
    Private,
    Internal,
    Unknown
};

enum class TriggerKind {
    None = 0,
    Identifier,       // user typed 2+ chars
    ScopeResolution,  // ::
    MethodCall,       // . or ->
    TypeAnnotation,   // :
    Attribute         // #[
};

struct SymbolCandidate {
    std::string name;
    std::string signature;
    std::string documentation;
    SymbolKind kind{SymbolKind::Other};
    Accessibility accessibility{Accessibility::Public};
    float relevance_score{0.0f};
    std::string source_file;
    size_t line{0};
    size_t column{0};
};

struct CompletionTrigger {
    TriggerKind kind{TriggerKind::None};
    std::string prefix;
};

// ============================================================================
// Symbol Index Bridge
// ============================================================================

class SymbolIndexBridge {
public:
    SymbolIndexBridge();
    ~SymbolIndexBridge();

    // Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // File indexing
    bool indexFile(const std::string& file_path, const std::string& source_code);
    bool indexFileAsync(const std::string& file_path, const std::string& source_code);
    
    // Query API (for CompletionEngine)
    std::vector<SymbolCandidate> queryCompletions(
        const std::string& file_path,
        const std::string& prefix,
        size_t line,
        size_t column);
    
    // Trigger detection (Phase 1b)
    CompletionTrigger detectTrigger(const std::string& source_code, size_t cursor_pos);
    
    // Project-wide queries
    std::vector<SymbolCandidate> queryProjectSymbols(const std::string& prefix);
    
    // File management
    void removeFile(const std::string& file_path);
    bool isFileIndexed(const std::string& file_path) const;
    size_t getFileVersion(const std::string& file_path) const;
    
    // Statistics
    size_t getIndexedFileCount() const;
    size_t getTotalSymbolCount() const;

private:
    // Background indexing
    struct PendingFile {
        std::string file_path;
        std::string source_code;
    };
    
    void backgroundIndexThread();
    
    // Query helpers
    std::vector<SymbolCandidate> queryFileSymbols(
        const rawrxd::ast::SymbolTable& table,
        const std::string& prefix,
        size_t line,
        size_t column);
    
    // Index management
    void addToProjectIndex(const std::string& file_path, 
                           const rawrxd::ast::SymbolTable& table);
    void removeFromProjectIndex(const std::string& file_path,
                                const rawrxd::ast::SymbolTable& table);
    
    // Helpers
    std::string buildSignature(const rawrxd::ast::Symbol& sym);
    bool isVisibleAtPosition(const rawrxd::ast::Symbol& sym, size_t line, size_t column);
    bool isPublic(const rawrxd::ast::Symbol& sym);
    float calculateRelevance(const rawrxd::ast::Symbol& sym, 
                             const std::string& prefix,
                             size_t line, size_t column);
    float calculateProjectRelevance(const rawrxd::ast::Symbol& sym,
                                    const std::string& prefix);
    SymbolKind nodeTypeToCompletionKind(RawrXD::AST::NodeType type);
    std::string extractPrefix(const std::string& source_code, size_t pos);

    // State
    bool initialized_{false};
    std::string current_file_;
    
    // File-scoped indices
    std::unordered_map<std::string, rawrxd::ast::SymbolTable> file_indices_;
    std::unordered_map<std::string, size_t> file_versions_;
    
    // Project-wide index (symbol name -> files)
    std::unordered_map<std::string, std::vector<std::string>> project_index_;
    
    // Thread safety
    mutable std::shared_mutex mutex_;
    
    // Background thread
    std::thread background_thread_;
    std::atomic<bool> background_thread_running_{false};
    std::mutex queue_mutex_;
    std::vector<PendingFile> pending_files_;
};

} // namespace bridge
} // namespace rawrxd

// ============================================================================
// C API for FFI (Phase 1c/d integration)
// ============================================================================

extern "C" {

// Opaque handle
typedef struct RawrXD_SymbolIndexBridge RawrXD_SymbolIndexBridge;

// Candidate structure for C API
struct RawrXD_SymbolCandidate {
    const char* name;
    const char* signature;
    const char* documentation;
    int kind;           // CompletionKind as int
    float relevance_score;
    const char* source_file;
    size_t line;
    size_t column;
};

struct RawrXD_CompletionTrigger {
    int kind;           // TriggerKind as int
    char prefix[64];
};

// Lifecycle
RawrXD_SymbolIndexBridge* rawrxd_symbol_index_create();
void rawrxd_symbol_index_destroy(RawrXD_SymbolIndexBridge* handle);

// Indexing
int rawrxd_symbol_index_file(RawrXD_SymbolIndexBridge* handle,
                              const char* file_path,
                              const char* source_code);

// Query
RawrXD_SymbolCandidate* rawrxd_symbol_query_completions(
    RawrXD_SymbolIndexBridge* handle,
    const char* file_path,
    const char* prefix,
    size_t line,
    size_t column,
    size_t* out_count);

void rawrxd_symbol_candidates_free(RawrXD_SymbolCandidate* candidates, size_t count);

// Trigger detection
RawrXD_CompletionTrigger rawrxd_symbol_detect_trigger(
    RawrXD_SymbolIndexBridge* handle,
    const char* source_code,
    size_t cursor_pos);

} // extern "C"
