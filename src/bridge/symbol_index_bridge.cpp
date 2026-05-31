// ============================================================================
// symbol_index_bridge.cpp — Phase 1a: Parser → Ghost Text Bridge
// ============================================================================
// Connects rust_parser_v2 output to the completion engine.
//
// Architecture:
//   rust_parser_v2.cpp  →  SymbolTable  →  SymbolIndexBridge  →  CompletionEngine
//        (parse)              (index)         (query API)          (ghost text)
//
// Key Design:
//   - File-scoped symbol index (fast lookup)
//   - Project-scoped cross-file resolution (workspace-wide)
//   - Incremental updates (only re-parse changed files)
//   - Thread-safe for background parsing
// ============================================================================

#include "symbol_index_bridge.hpp"
#include "rust_parser.hpp"
#include "symbol_table.hpp"
#include <algorithm>
#include <filesystem>

namespace rawrxd {
namespace bridge {

namespace fs = std::filesystem;

// ============================================================================
// SymbolIndexBridge Implementation
// ============================================================================

SymbolIndexBridge::SymbolIndexBridge() = default;
SymbolIndexBridge::~SymbolIndexBridge() = default;

bool SymbolIndexBridge::initialize() {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    
    file_indices_.clear();
    project_index_.clear();
    file_versions_.clear();
    
    initialized_ = true;
    return true;
}

void SymbolIndexBridge::shutdown() {
    std::lock_guard<std::shared_mutex> lock(mutex_);
    
    file_indices_.clear();
    project_index_.clear();
    file_versions_.clear();
    
    initialized_ = false;
}

// ============================================================================
// File Indexing
// ============================================================================

bool SymbolIndexBridge::indexFile(const std::string& file_path, 
                                   const std::string& source_code) {
    if (!initialized_) return false;

    rawrxd::ast::rust::RustParser parser;
    rawrxd::ast::SymbolTable table;
    auto parse_result = parser.parse(source_code, file_path, &table);
    if (!parse_result.success) {
        return false;
    }
    
    // Update file-scoped index
    {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        
        // Remove old index for this file
        auto it = file_indices_.find(file_path);
        if (it != file_indices_.end()) {
            removeFromProjectIndex(file_path, it->second);
        }
        
        // Store new index
        file_indices_[file_path] = std::move(table);
        file_versions_[file_path]++;
        
        // Add to project index
        addToProjectIndex(file_path, file_indices_[file_path]);
    }
    
    return true;
}

bool SymbolIndexBridge::indexFileAsync(const std::string& file_path,
                                        const std::string& source_code) {
    // Queue for background processing
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    pending_files_.push_back({file_path, source_code});
    
    // Start background thread if not running
    if (!background_thread_running_) {
        background_thread_running_ = true;
        background_thread_ = std::thread(&SymbolIndexBridge::backgroundIndexThread, this);
    }
    
    return true;
}

void SymbolIndexBridge::backgroundIndexThread() {
    while (background_thread_running_) {
        std::vector<PendingFile> batch;
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            batch = std::move(pending_files_);
            pending_files_.clear();
        }
        
        for (const auto& pf : batch) {
            indexFile(pf.file_path, pf.source_code);
        }
        
        // Sleep if no work
        if (batch.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

// ============================================================================
// Query API (for CompletionEngine)
// ============================================================================

std::vector<SymbolCandidate> SymbolIndexBridge::queryCompletions(
    const std::string& file_path,
    const std::string& prefix,
    size_t line,
    size_t column) {
    
    std::shared_lock<std::shared_mutex> lock(mutex_);
    
    std::vector<SymbolCandidate> results;
    
    // 1. Query file-scoped symbols
    auto file_it = file_indices_.find(file_path);
    if (file_it != file_indices_.end()) {
        auto file_results = queryFileSymbols(file_it->second, prefix, line, column);
        results.insert(results.end(), file_results.begin(), file_results.end());
    }
    
    // 2. Query project-scoped symbols (for cross-file resolution)
    auto project_results = queryProjectSymbols(prefix);
    results.insert(results.end(), project_results.begin(), project_results.end());
    
    // 3. Sort by relevance
    std::sort(results.begin(), results.end(), 
        [](const SymbolCandidate& a, const SymbolCandidate& b) {
            return a.relevance_score > b.relevance_score;
        });
    
    // 4. Deduplicate
    auto last = std::unique(results.begin(), results.end(),
        [](const SymbolCandidate& a, const SymbolCandidate& b) {
            return a.name == b.name && a.signature == b.signature;
        });
    results.erase(last, results.end());
    
    return results;
}

std::vector<SymbolCandidate> SymbolIndexBridge::queryFileSymbols(
    const rawrxd::ast::SymbolTable& table,
    const std::string& prefix,
    size_t line,
    size_t column) {
    
    std::vector<SymbolCandidate> results;
    
    for (const auto& sym : table.all()) {
        // Filter by prefix
        if (sym.name.find(prefix) != 0) continue;
        
        // Check scope visibility
        if (!isVisibleAtPosition(sym, line, column)) continue;
        
        SymbolCandidate cand;
        cand.name = sym.name;
        cand.signature = buildSignature(sym);
        cand.documentation = sym.meta.doc;
        cand.kind = nodeTypeToCompletionKind(sym.type);
        cand.relevance_score = calculateRelevance(sym, prefix, line, column);
        cand.source_file = sym.file;
        cand.line = sym.range.start.line;
        cand.column = sym.range.start.column;
        
        results.push_back(cand);
    }
    
    return results;
}

std::vector<SymbolCandidate> SymbolIndexBridge::queryProjectSymbols(
    const std::string& prefix) {
    
    std::vector<SymbolCandidate> results;
    
    for (const auto& [file_path, table] : file_indices_) {
        for (const auto& sym : table.all()) {
            // Only public symbols from other files
            if (file_path != current_file_ && !isPublic(sym)) continue;
            
            if (sym.name.find(prefix) != 0) continue;
            
            SymbolCandidate cand;
            cand.name = sym.name;
            cand.signature = buildSignature(sym);
            cand.documentation = sym.meta.doc;
            cand.kind = nodeTypeToCompletionKind(sym.type);
            cand.relevance_score = calculateProjectRelevance(sym, prefix);
            cand.source_file = sym.file;
            cand.line = sym.range.start.line;
            cand.column = sym.range.start.column;
            
            results.push_back(cand);
        }
    }
    
    return results;
}

// ============================================================================
// Trigger Detection (Phase 1b)
// ============================================================================

CompletionTrigger SymbolIndexBridge::detectTrigger(
    const std::string& source_code,
    size_t cursor_pos) {
    
    if (cursor_pos == 0 || cursor_pos > source_code.size()) {
        return CompletionTrigger{TriggerKind::None, ""};
    }
    
    // Look back from cursor
    size_t lookback = std::min(cursor_pos, size_t(10));
    std::string_view context(source_code.c_str() + cursor_pos - lookback, lookback);
    
    // Check for trigger characters
    if (lookback >= 2) {
        char c1 = source_code[cursor_pos - 1];
        char c2 = source_code[cursor_pos - 2];
        
        // Scope resolution: ::
        if (c1 == ':' && c2 == ':') {
            std::string prefix = extractPrefix(source_code, cursor_pos - 2);
            return CompletionTrigger{TriggerKind::ScopeResolution, prefix};
        }
        
        // Method call: .
        if (c1 == '.') {
            std::string prefix = extractPrefix(source_code, cursor_pos - 1);
            return CompletionTrigger{TriggerKind::MethodCall, prefix};
        }
        
        // Arrow: ->
        if (c1 == '>' && c2 == '-') {
            std::string prefix = extractPrefix(source_code, cursor_pos - 2);
            return CompletionTrigger{TriggerKind::MethodCall, prefix};
        }
    }
    
    // Identifier start: alphanumeric or _
    char c = source_code[cursor_pos - 1];
    if (std::isalnum(c) || c == '_') {
        std::string prefix = extractPrefix(source_code, cursor_pos);
        if (prefix.length() >= 2) { // Minimum 2 chars for completion
            return CompletionTrigger{TriggerKind::Identifier, prefix};
        }
    }
    
    return CompletionTrigger{TriggerKind::None, ""};
}

// ============================================================================
// Helpers
// ============================================================================

std::string SymbolIndexBridge::buildSignature(const rawrxd::ast::Symbol& sym) {
    return sym.name;
}

bool SymbolIndexBridge::isVisibleAtPosition(
    const rawrxd::ast::Symbol& sym,
    size_t line, size_t column) {
    
    // Check if symbol is defined before cursor position
    if (sym.range.start.line > line) return false;
    if (sym.range.start.line == line && sym.range.start.column > column) return false;
    
    // Check access modifiers
    if (sym.meta.visibility == "private") {
        // Private symbols only visible within same module/class
        // (simplified - would need full scope chain)
        return true; // Assume same scope for now
    }
    
    return true;
}

bool SymbolIndexBridge::isPublic(const rawrxd::ast::Symbol& sym) {
    return sym.meta.visibility == "pub" || 
           sym.meta.visibility == "pub(crate)" ||
           sym.name == "main";
}

float SymbolIndexBridge::calculateRelevance(
    const rawrxd::ast::Symbol& sym,
    const std::string& prefix,
    size_t line, size_t column) {
    
    float score = 0.0f;
    
    // Exact prefix match
    if (sym.name == prefix) score += 100.0f;
    else if (sym.name.find(prefix) == 0) score += 50.0f;
    else score += 10.0f; // Substring match
    
    // Prefer symbols defined closer to cursor
    size_t distance = (line > sym.range.start.line) 
        ? line - sym.range.start.line 
        : 0;
    score -= static_cast<float>(distance) * 0.5f;
    
    // Prefer functions over other types
    if (sym.type == RawrXD::AST::NodeType::FunctionDecl) score += 20.0f;
    
    // Prefer public symbols
    if (isPublic(sym)) score += 10.0f;
    
    return std::max(0.0f, score);
}

float SymbolIndexBridge::calculateProjectRelevance(
    const rawrxd::ast::Symbol& sym,
    const std::string& prefix) {
    
    float score = 0.0f;
    
    // Exact match bonus
    if (sym.name == prefix) score += 80.0f;
    else if (sym.name.find(prefix) == 0) score += 40.0f;
    else score += 5.0f;
    
    // Prefer public functions
    if (sym.type == RawrXD::AST::NodeType::FunctionDecl && isPublic(sym)) {
        score += 15.0f;
    }
    
    return score;
}

SymbolKind SymbolIndexBridge::nodeTypeToCompletionKind(
    RawrXD::AST::NodeType type) {
    
    switch (type) {
        case RawrXD::AST::NodeType::FunctionDecl:
            return SymbolKind::Function;
        case RawrXD::AST::NodeType::VariableDecl:
            return SymbolKind::Variable;
        case RawrXD::AST::NodeType::ClassDecl:
        case RawrXD::AST::NodeType::StructDecl:
            return SymbolKind::Type;
        case RawrXD::AST::NodeType::EnumDecl:
            return SymbolKind::Enum;
        case RawrXD::AST::NodeType::NamespaceDecl:
            return SymbolKind::Module;
        default:
            return SymbolKind::Other;
    }
}

std::string SymbolIndexBridge::extractPrefix(
    const std::string& source_code, size_t pos) {
    
    if (pos == 0) return "";
    
    size_t start = pos;
    while (start > 0) {
        char c = source_code[start - 1];
        if (std::isalnum(c) || c == '_') {
            start--;
        } else {
            break;
        }
    }
    
    return source_code.substr(start, pos - start);
}

// ============================================================================
// Project Index Management
// ============================================================================

void SymbolIndexBridge::addToProjectIndex(
    const std::string& file_path,
    const rawrxd::ast::SymbolTable& table) {
    
    for (const auto& sym : table.all()) {
        if (isPublic(sym)) {
            project_index_[sym.name].push_back(file_path);
        }
    }
}

void SymbolIndexBridge::removeFromProjectIndex(
    const std::string& file_path,
    const rawrxd::ast::SymbolTable& table) {
    
    for (const auto& sym : table.all()) {
        auto it = project_index_.find(sym.name);
        if (it != project_index_.end()) {
            auto& files = it->second;
            files.erase(
                std::remove(files.begin(), files.end(), file_path),
                files.end()
            );
            if (files.empty()) {
                project_index_.erase(it);
            }
        }
    }
}

// ============================================================================
// C API for FFI
// ============================================================================

extern "C" {

RawrXD_SymbolIndexBridge* rawrxd_symbol_index_create() {
    auto* bridge = new SymbolIndexBridge();
    if (!bridge->initialize()) {
        delete bridge;
        return nullptr;
    }
    return reinterpret_cast<RawrXD_SymbolIndexBridge*>(bridge);
}

void rawrxd_symbol_index_destroy(RawrXD_SymbolIndexBridge* handle) {
    if (handle) {
        auto* bridge = reinterpret_cast<SymbolIndexBridge*>(handle);
        bridge->shutdown();
        delete bridge;
    }
}

int rawrxd_symbol_index_file(RawrXD_SymbolIndexBridge* handle,
                              const char* file_path,
                              const char* source_code) {
    if (!handle || !file_path || !source_code) return 0;
    
    auto* bridge = reinterpret_cast<SymbolIndexBridge*>(handle);
    return bridge->indexFile(file_path, source_code) ? 1 : 0;
}

RawrXD_SymbolCandidate* rawrxd_symbol_query_completions(
    RawrXD_SymbolIndexBridge* handle,
    const char* file_path,
    const char* prefix,
    size_t line,
    size_t column,
    size_t* out_count) {
    
    if (!handle || !file_path || !prefix || !out_count) return nullptr;
    
    auto* bridge = reinterpret_cast<SymbolIndexBridge*>(handle);
    auto results = bridge->queryCompletions(file_path, prefix, line, column);
    
    *out_count = results.size();
    if (results.empty()) return nullptr;
    
    // Allocate result array (caller must free)
    auto* array = new RawrXD_SymbolCandidate[results.size()];
    for (size_t i = 0; i < results.size(); ++i) {
        array[i].name = strdup(results[i].name.c_str());
        array[i].signature = strdup(results[i].signature.c_str());
        array[i].documentation = strdup(results[i].documentation.c_str());
        array[i].kind = static_cast<int>(results[i].kind);
        array[i].relevance_score = results[i].relevance_score;
        array[i].source_file = strdup(results[i].source_file.c_str());
        array[i].line = results[i].line;
        array[i].column = results[i].column;
    }
    
    return array;
}

void rawrxd_symbol_candidates_free(RawrXD_SymbolCandidate* candidates, size_t count) {
    if (!candidates) return;
    
    for (size_t i = 0; i < count; ++i) {
        free(const_cast<char*>(candidates[i].name));
        free(const_cast<char*>(candidates[i].signature));
        free(const_cast<char*>(candidates[i].documentation));
        free(const_cast<char*>(candidates[i].source_file));
    }
    delete[] candidates;
}

RawrXD_CompletionTrigger rawrxd_symbol_detect_trigger(
    RawrXD_SymbolIndexBridge* handle,
    const char* source_code,
    size_t cursor_pos) {
    
    if (!handle || !source_code) {
        return RawrXD_CompletionTrigger{0, ""};
    }
    
    auto* bridge = reinterpret_cast<SymbolIndexBridge*>(handle);
    auto trigger = bridge->detectTrigger(source_code, cursor_pos);
    
    RawrXD_CompletionTrigger result;
    result.kind = static_cast<int>(trigger.kind);
    
    // Copy prefix (limited length)
    strncpy(result.prefix, trigger.prefix.c_str(), sizeof(result.prefix) - 1);
    result.prefix[sizeof(result.prefix) - 1] = '\0';
    
    return result;
}

} // extern "C"

} // namespace bridge
} // namespace rawrxd
