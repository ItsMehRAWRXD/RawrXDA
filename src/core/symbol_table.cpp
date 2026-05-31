// symbol_table.cpp
// Out-of-line implementations for symbol_table.hpp.

#include "symbol_table.hpp"

namespace rawrxd::ast {

void SymbolTable::clear() {
    symbols_.clear();
    index_.clear();
    edges_.clear();
    from_index_.clear();
    to_index_.clear();
    indices_dirty_ = true;
}

void SymbolTable::add(const Symbol& s) {
    index_[s.name] = symbols_.size();
    symbols_.push_back(s);
    indices_dirty_ = true;
}

const Symbol* SymbolTable::find(const std::string& name) const {
    auto it = index_.find(name);
    if (it == index_.end()) return nullptr;
    return &symbols_[it->second];
}

std::vector<const Symbol*> SymbolTable::query(RawrXD::AST::NodeType type) const {
    std::vector<const Symbol*> out;
    for (auto& s : symbols_) {
        if (s.type == type) out.push_back(&s);
    }
    return out;
}

// ---- call graph (v3.3) ----

void SymbolTable::addCallEdge(const CallEdge& e) {
    edges_.push_back(e);
    indices_dirty_ = true;
}

void SymbolTable::rebuildIndices() const {
    from_index_.clear();
    to_index_.clear();
    for (size_t i = 0; i < edges_.size(); ++i) {
        from_index_[edges_[i].caller_name].push_back(i);
        to_index_[edges_[i].callee_name].push_back(i);
    }
    indices_dirty_ = false;
}

std::vector<CallEdge> SymbolTable::callsFrom(const std::string& caller_name) const {
    if (indices_dirty_) rebuildIndices();
    std::vector<CallEdge> out;
    auto it = from_index_.find(caller_name);
    if (it != from_index_.end()) {
        for (size_t idx : it->second) out.push_back(edges_[idx]);
    }
    return out;
}

std::vector<CallEdge> SymbolTable::callsTo(const std::string& callee_name) const {
    if (indices_dirty_) rebuildIndices();
    std::vector<CallEdge> out;
    auto it = to_index_.find(callee_name);
    if (it != to_index_.end()) {
        for (size_t idx : it->second) out.push_back(edges_[idx]);
    }
    return out;
}

void SymbolTable::resolveCalls() {
    for (auto& e : edges_) {
        // Try direct name match first
        const Symbol* sym = find(e.callee_name);
        if (!sym) {
            // Try stripping "self." prefix for method calls
            if (e.callee_name.rfind("self.", 0) == 0) {
                sym = find(e.callee_name.substr(5));
            }
            // Try last segment of qualified call (foo::bar → bar)
            else if (e.kind == CallKind::Qualified) {
                size_t pos = e.callee_name.rfind("::");
                if (pos != std::string::npos) {
                    sym = find(e.callee_name.substr(pos + 2));
                }
            }
        }
        if (sym) {
            e.resolved_symbol = sym;
            // If resolved, downgrade External to actual kind
            if (e.kind == CallKind::External) {
                if (e.callee_name.find(".") != std::string::npos) e.kind = CallKind::Method;
                else if (e.callee_name.find("::") != std::string::npos) e.kind = CallKind::Qualified;
                else e.kind = CallKind::Direct;
            }
        } else if (e.kind != CallKind::External) {
            // If we couldn't resolve it, mark as external
            e.kind = CallKind::External;
        }
    }
}

std::vector<const Symbol*> SymbolTable::deadSymbols() const {
    if (indices_dirty_) rebuildIndices();

    std::vector<const Symbol*> out;
    for (const auto& s : symbols_) {
        // Entrypoints: pub fns, main, and tests are never dead
        if (s.name == "main") continue;
        if (s.meta.is_pub) continue;
        if (!s.meta.attributes.empty()) {
            bool is_test = false;
            for (const auto& attr : s.meta.attributes) {
                if (attr.find("test") != std::string::npos) { is_test = true; break; }
            }
            if (is_test) continue;
        }

        // Check if anyone calls this symbol
        auto incoming = callsTo(s.name);
        if (incoming.empty()) {
            out.push_back(&s);
        }
    }
    return out;
}

} // namespace rawrxd::ast
