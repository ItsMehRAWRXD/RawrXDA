// symbol_table.hpp
// Thin indexing layer over AST nodes. Zero-overhead when unused.
// Provides symbol lookup, type-based queries, and parent tracking.

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include "ast_graph_engine.hpp"

namespace rawrxd::ast {

struct Symbol {
    std::string name;
    std::string file;        // human-readable file path
    RawrXD::AST::NodeType type;
    RawrXD::AST::SourceRange range;
    RawrXD::AST::RustMeta meta;
    std::string parent;      // impl/module/trait name, or empty for top-level
};

enum class CallKind {
    Direct,     // foo(...)
    Method,     // obj.method(...)
    Qualified,  // bar::baz(...)
    External    // println(...) — not found in SymbolTable
};

// Lightweight call edge — string-based, no type resolution yet.
// caller_name is the symbol name; callee_name is the raw identifier seen at call site.
struct CallEdge {
    std::string caller_name;
    std::string callee_name;
    CallKind kind{CallKind::Direct};
    const Symbol* resolved_symbol{nullptr}; // set after resolveCalls()
    size_t call_site_line{0}; // approximate line (0 if unavailable)
};

class SymbolTable {
public:
    void clear();

    void add(const Symbol& s);

    const Symbol* find(const std::string& name) const;

    std::vector<const Symbol*> query(RawrXD::AST::NodeType type) const;

    const std::vector<Symbol>& all() const { return symbols_; }

    // ---- call graph (v3.3) ----
    void addCallEdge(const CallEdge& e);
    std::vector<CallEdge> callsFrom(const std::string& caller_name) const;
    std::vector<CallEdge> callsTo(const std::string& callee_name) const;
    const std::vector<CallEdge>& allEdges() const { return edges_; }

    // Cross-file resolution: link each edge's callee_name to a Symbol* if known.
    // Call this after all symbols from all files have been added.
    void resolveCalls();

    // Dead-code detection: returns symbols with no incoming call edges
    // and no pub visibility (entrypoints are pub fns + main).
    std::vector<const Symbol*> deadSymbols() const;

private:
    std::vector<Symbol> symbols_;
    std::unordered_map<std::string, size_t> index_;
    std::vector<CallEdge> edges_;

    // O(1) call graph indices (built on first query, invalidated on add)
    mutable std::unordered_map<std::string, std::vector<size_t>> from_index_;
    mutable std::unordered_map<std::string, std::vector<size_t>> to_index_;
    mutable bool indices_dirty_{true};

    void rebuildIndices() const;
};

} // namespace rawrxd::ast
