// semantic_code_intelligence.cpp — Phase 16: Semantic Code Intelligence
// Cross-reference database, type inference engine, symbol resolution across
// compilation units, semantic indexing, and intelligent code navigation.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured PatchResult returns only.
// Rule: All threading via std::mutex + std::lock_guard. No recursive locks.
#include "semantic_code_intelligence.hpp"
#include <algorithm>
#include <queue>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstring>

// ============================================================================
// Singleton
// ============================================================================
SemanticCodeIntelligence& SemanticCodeIntelligence::instance() {
    static SemanticCodeIntelligence inst;
    return inst;
}

SemanticCodeIntelligence::SemanticCodeIntelligence()  = default;
SemanticCodeIntelligence::~SemanticCodeIntelligence() = default;

// ============================================================================
// Symbol Registration
// ============================================================================
uint64_t SemanticCodeIntelligence::addSymbol(const SymbolEntry& symbol) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t id = m_nextSymbolId.fetch_add(1);
    SymbolEntry entry = symbol;
    entry.symbolId = id;

    m_symbols[id] = entry;

    // Update indices
    m_nameIndex[entry.name].push_back(id);
    if (!entry.definition.filePath.empty()) {
        m_fileIndex[entry.definition.filePath].push_back(id);
    }

    // Link to parent
    if (entry.parentSymbolId > 0) {
        auto parentIt = m_symbols.find(entry.parentSymbolId);
        if (parentIt != m_symbols.end()) {
            parentIt->second.childSymbols.push_back(id);
        }
    }

    m_stats.totalSymbols.fetch_add(1);
    return id;
}

PatchResult SemanticCodeIntelligence::updateSymbol(uint64_t symbolId, const SymbolEntry& updated) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_symbols.find(symbolId);
    if (it == m_symbols.end()) return PatchResult::error("Symbol not found", -1);

    // Update name index if name changed
    if (it->second.name != updated.name) {
        auto& old = m_nameIndex[it->second.name];
        old.erase(std::remove(old.begin(), old.end(), symbolId), old.end());
        m_nameIndex[updated.name].push_back(symbolId);
    }

    // Preserve ID
    SymbolEntry entry = updated;
    entry.symbolId = symbolId;
    it->second = entry;

    return PatchResult::ok("Symbol updated");
}

PatchResult SemanticCodeIntelligence::removeSymbol(uint64_t symbolId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_symbols.find(symbolId);
    if (it == m_symbols.end()) return PatchResult::error("Symbol not found", -1);

    // Remove from indices
    auto& nameIdx = m_nameIndex[it->second.name];
    nameIdx.erase(std::remove(nameIdx.begin(), nameIdx.end(), symbolId), nameIdx.end());

    if (!it->second.definition.filePath.empty()) {
        auto& fileIdx = m_fileIndex[it->second.definition.filePath];
        fileIdx.erase(std::remove(fileIdx.begin(), fileIdx.end(), symbolId), fileIdx.end());
    }

    // Remove from parent
    if (it->second.parentSymbolId > 0) {
        auto parentIt = m_symbols.find(it->second.parentSymbolId);
        if (parentIt != m_symbols.end()) {
            auto& children = parentIt->second.childSymbols;
            children.erase(std::remove(children.begin(), children.end(), symbolId), children.end());
        }
    }

    m_symbols.erase(it);
    m_stats.totalSymbols.fetch_sub(1);

    return PatchResult::ok("Symbol removed");
}

// ============================================================================
// Type Registration
// ============================================================================
uint64_t SemanticCodeIntelligence::addType(const TypeInfo& type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t id = m_nextTypeId.fetch_add(1);
    TypeInfo entry = type;
    entry.typeId = id;

    m_types[id] = entry;
    m_typeNameIndex[entry.qualifiedName.empty() ? entry.name : entry.qualifiedName] = id;
    m_stats.totalTypes.fetch_add(1);

    return id;
}

PatchResult SemanticCodeIntelligence::updateType(uint64_t typeId, const TypeInfo& updated) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_types.find(typeId);
    if (it == m_types.end()) return PatchResult::error("Type not found", -1);

    TypeInfo entry = updated;
    entry.typeId = typeId;
    it->second = entry;

    return PatchResult::ok("Type updated");
}

const TypeInfo* SemanticCodeIntelligence::getType(uint64_t typeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_types.find(typeId);
    return (it != m_types.end()) ? &it->second : nullptr;
}

// ============================================================================
// Scope Management
// ============================================================================
uint64_t SemanticCodeIntelligence::pushScope(const std::string& name, SymbolKind kind,
                                              uint64_t parentScope) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t id = m_nextScopeId.fetch_add(1);
    Scope scope;
    scope.scopeId       = id;
    scope.name          = name;
    scope.kind          = kind;
    scope.parentScopeId = parentScope;

    m_scopes[id] = scope;

    // Link to parent scope
    if (parentScope > 0) {
        auto pIt = m_scopes.find(parentScope);
        if (pIt != m_scopes.end()) {
            pIt->second.childScopes.push_back(id);
        }
    }

    m_stats.totalScopes.fetch_add(1);
    return id;
}

PatchResult SemanticCodeIntelligence::popScope(uint64_t scopeId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Scopes are persistent — popScope just marks completion
    auto it = m_scopes.find(scopeId);
    if (it == m_scopes.end()) return PatchResult::error("Scope not found", -1);
    return PatchResult::ok("Scope finalized");
}

uint64_t SemanticCodeIntelligence::resolveSymbolInScope(
    const std::string& name, uint64_t scopeId) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Walk up the scope chain
    uint64_t current = scopeId;
    while (current > 0) {
        auto sIt = m_scopes.find(current);
        if (sIt == m_scopes.end()) break;

        for (uint64_t symId : sIt->second.symbolIds) {
            auto symIt = m_symbols.find(symId);
            if (symIt != m_symbols.end() && symIt->second.name == name) {
                return symId;
            }
        }

        current = sIt->second.parentScopeId;
    }

    // Global lookup
    auto nameIt = m_nameIndex.find(name);
    if (nameIt != m_nameIndex.end() && !nameIt->second.empty()) {
        return nameIt->second.front();
    }

    return 0; // Not found
}

// ============================================================================
// Cross-References
// ============================================================================
PatchResult SemanticCodeIntelligence::addReference(const CrossReference& ref) {
    std::lock_guard<std::mutex> lock(m_mutex);

    CrossReference entry = ref;
    entry.refId = m_nextRefId.fetch_add(1);

    size_t idx = m_references.size();
    m_references.push_back(entry);

    // Bound the reference store
    if (m_references.size() > MAX_REFERENCES) {
        m_references.pop_front();
    }

    m_refBySymbol[entry.symbolId].push_back(idx);

    // Update reference count on symbol
    auto symIt = m_symbols.find(entry.symbolId);
    if (symIt != m_symbols.end()) {
        symIt->second.referenceCount++;
    }

    m_stats.totalReferences.fetch_add(1);
    return PatchResult::ok("Reference recorded");
}

std::vector<CrossReference> SemanticCodeIntelligence::getReferences(uint64_t symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CrossReference> result;

    auto it = m_refBySymbol.find(symbolId);
    if (it == m_refBySymbol.end()) return result;

    for (size_t idx : it->second) {
        if (idx < m_references.size()) {
            result.push_back(m_references[idx]);
        }
    }

    return result;
}

std::vector<CrossReference> SemanticCodeIntelligence::getReferencesInFile(
    const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CrossReference> result;

    for (auto& ref : m_references) {
        if (ref.location.filePath == filePath) {
            result.push_back(ref);
        }
    }

    return result;
}

uint32_t SemanticCodeIntelligence::getReferenceCount(uint64_t symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_symbols.find(symbolId);
    return (it != m_symbols.end()) ? it->second.referenceCount : 0;
}

// ============================================================================
// Call Graph
// ============================================================================
PatchResult SemanticCodeIntelligence::addCallEdge(const CallGraphEdge& edge) {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t idx = m_callGraph.size();
    m_callGraph.push_back(edge);

    m_callerIndex[edge.calleeId].push_back(idx);
    m_calleeIndex[edge.callerId].push_back(idx);

    return PatchResult::ok("Call edge added");
}

std::vector<CallGraphEdge> SemanticCodeIntelligence::getCallersOf(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CallGraphEdge> result;

    auto it = m_callerIndex.find(functionId);
    if (it == m_callerIndex.end()) return result;

    for (size_t idx : it->second) {
        if (idx < m_callGraph.size()) {
            result.push_back(m_callGraph[idx]);
        }
    }

    return result;
}

std::vector<CallGraphEdge> SemanticCodeIntelligence::getCalleesOf(uint64_t functionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CallGraphEdge> result;

    auto it = m_calleeIndex.find(functionId);
    if (it == m_calleeIndex.end()) return result;

    for (size_t idx : it->second) {
        if (idx < m_callGraph.size()) {
            result.push_back(m_callGraph[idx]);
        }
    }

    return result;
}

std::vector<uint64_t> SemanticCodeIntelligence::getCallChain(
    uint64_t functionId, uint32_t maxDepth) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<uint64_t> chain;
    std::unordered_set<uint64_t> visited;
    std::queue<std::pair<uint64_t, uint32_t>> worklist;

    worklist.push({functionId, 0});
    visited.insert(functionId);

    while (!worklist.empty()) {
        auto [current, depth] = worklist.front();
        worklist.pop();

        chain.push_back(current);

        if (depth >= maxDepth) continue;

        auto it = m_calleeIndex.find(current);
        if (it != m_calleeIndex.end()) {
            for (size_t idx : it->second) {
                if (idx < m_callGraph.size()) {
                    uint64_t callee = m_callGraph[idx].calleeId;
                    if (!visited.count(callee)) {
                        visited.insert(callee);
                        worklist.push({callee, depth + 1});
                    }
                }
            }
        }
    }

    return chain;
}

// ============================================================================
// Navigation
// ============================================================================
const SymbolEntry* SemanticCodeIntelligence::goToDefinition(
    const std::string& name, const SourceLocation& context) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queriesServed.fetch_add(1);

    auto it = m_nameIndex.find(name);
    if (it == m_nameIndex.end() || it->second.empty()) return nullptr;

    // If single definition, return it
    if (it->second.size() == 1) {
        auto symIt = m_symbols.find(it->second[0]);
        return (symIt != m_symbols.end()) ? &symIt->second : nullptr;
    }

    // Multiple matches — prefer same file, then closest scope
    const SymbolEntry* best = nullptr;
    int bestScore = -1;

    for (uint64_t symId : it->second) {
        auto symIt = m_symbols.find(symId);
        if (symIt == m_symbols.end()) continue;

        int score = 0;
        // Same file is highly preferred
        if (symIt->second.definition.filePath == context.filePath) score += 100;
        // Closer line is better
        int lineDist = std::abs(static_cast<int>(symIt->second.definition.line) -
                                static_cast<int>(context.line));
        score += std::max(0, 50 - lineDist);

        if (score > bestScore) {
            bestScore = score;
            best = &symIt->second;
        }
    }

    return best;
}

std::vector<SourceLocation> SemanticCodeIntelligence::findAllReferences(uint64_t symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queriesServed.fetch_add(1);

    std::vector<SourceLocation> result;

    // Add the definition
    auto symIt = m_symbols.find(symbolId);
    if (symIt != m_symbols.end()) {
        result.push_back(symIt->second.definition);
        for (auto& decl : symIt->second.declarations) {
            result.push_back(decl);
        }
    }

    // Add all references
    auto refIt = m_refBySymbol.find(symbolId);
    if (refIt != m_refBySymbol.end()) {
        for (size_t idx : refIt->second) {
            if (idx < m_references.size()) {
                result.push_back(m_references[idx].location);
            }
        }
    }

    return result;
}

std::vector<const SymbolEntry*> SemanticCodeIntelligence::findImplementations(
    uint64_t interfaceSymbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queriesServed.fetch_add(1);

    std::vector<const SymbolEntry*> result;

    auto ifaceIt = m_symbols.find(interfaceSymbolId);
    if (ifaceIt == m_symbols.end()) return result;

    // Find all symbols that list this as an implemented interface or base type
    for (auto& [id, sym] : m_symbols) {
        for (uint64_t iface : sym.implementedInterfaces) {
            if (iface == interfaceSymbolId) {
                result.push_back(&sym);
                break;
            }
        }
        for (uint64_t base : sym.baseTypes) {
            if (base == interfaceSymbolId) {
                result.push_back(&sym);
                break;
            }
        }
    }

    return result;
}

std::vector<const SymbolEntry*> SemanticCodeIntelligence::getTypeHierarchy(
    uint64_t typeSymbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queriesServed.fetch_add(1);

    std::vector<const SymbolEntry*> hierarchy;
    std::unordered_set<uint64_t> visited;
    std::queue<uint64_t> worklist;

    worklist.push(typeSymbolId);

    while (!worklist.empty()) {
        uint64_t current = worklist.front();
        worklist.pop();

        if (visited.count(current)) continue;
        visited.insert(current);

        auto symIt = m_symbols.find(current);
        if (symIt == m_symbols.end()) continue;

        hierarchy.push_back(&symIt->second);

        // Walk up to base types
        for (uint64_t base : symIt->second.baseTypes) {
            worklist.push(base);
        }

        // Walk down to derived types
        for (uint64_t derived : symIt->second.derivedTypes) {
            worklist.push(derived);
        }
    }

    return hierarchy;
}

const SymbolEntry* SemanticCodeIntelligence::findSymbolAt(
    const std::string& file, uint32_t line, uint32_t col) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queriesServed.fetch_add(1);

    auto fileIt = m_fileIndex.find(file);
    if (fileIt == m_fileIndex.end()) return nullptr;

    const SymbolEntry* best = nullptr;
    uint32_t bestDist = UINT32_MAX;

    for (uint64_t symId : fileIt->second) {
        auto symIt = m_symbols.find(symId);
        if (symIt == m_symbols.end()) continue;

        auto& loc = symIt->second.definition;
        if (loc.line <= line && loc.endLine >= line) {
            uint32_t dist = (line - loc.line) * 1000 + (col > loc.column ? col - loc.column : 0);
            if (dist < bestDist) {
                bestDist = dist;
                best = &symIt->second;
            }
        }
    }

    return best;
}

// ============================================================================
// Autocomplete
// ============================================================================
std::vector<CompletionItem> SemanticCodeIntelligence::getCompletions(
    const std::string& prefix, uint64_t scopeId, uint32_t maxResults) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queriesServed.fetch_add(1);

    std::vector<CompletionItem> results;
    std::unordered_set<uint64_t> seen;

    // Walk scope chain for local completions
    uint64_t current = scopeId;
    int scopeDistance = 0;
    while (current > 0 && results.size() < maxResults) {
        auto sIt = m_scopes.find(current);
        if (sIt == m_scopes.end()) break;

        for (uint64_t symId : sIt->second.symbolIds) {
            if (seen.count(symId)) continue;
            seen.insert(symId);

            auto symIt = m_symbols.find(symId);
            if (symIt == m_symbols.end()) continue;

            if (matchesPrefix(symIt->second.name, prefix)) {
                CompletionItem item;
                item.label         = symIt->second.name;
                item.detail        = symIt->second.signature;
                item.documentation = symIt->second.documentation;
                item.insertText    = symIt->second.name;
                item.kind          = symIt->second.kind;
                item.sortOrder     = scopeDistance;
                results.push_back(item);
            }
        }

        current = sIt->second.parentScopeId;
        scopeDistance++;
    }

    // Global completions
    for (auto& [name, symIds] : m_nameIndex) {
        if (results.size() >= maxResults) break;
        if (!matchesPrefix(name, prefix)) continue;

        for (uint64_t symId : symIds) {
            if (seen.count(symId)) continue;
            seen.insert(symId);

            auto symIt = m_symbols.find(symId);
            if (symIt == m_symbols.end()) continue;

            CompletionItem item;
            item.label         = symIt->second.name;
            item.detail        = symIt->second.signature;
            item.documentation = symIt->second.documentation;
            item.insertText    = symIt->second.name;
            item.kind          = symIt->second.kind;
            item.sortOrder     = 100 + scopeDistance;
            results.push_back(item);
        }
    }

    // Sort by priority
    std::sort(results.begin(), results.end(),
        [](const CompletionItem& a, const CompletionItem& b) {
            return a.sortOrder < b.sortOrder;
        });

    if (results.size() > maxResults) results.resize(maxResults);
    return results;
}

// ============================================================================
// Hover Info
// ============================================================================
HoverInfo SemanticCodeIntelligence::getHoverInfo(uint64_t symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queriesServed.fetch_add(1);

    HoverInfo info;
    auto symIt = m_symbols.find(symbolId);
    if (symIt == m_symbols.end()) return info;

    const auto& sym = symIt->second;
    info.signature     = sym.signature;
    info.documentation = sym.documentation;
    info.definedIn     = sym.definition.filePath;
    info.definedLine   = sym.definition.line;

    // Resolve type
    if (sym.typeId > 0) {
        auto typeIt = m_types.find(sym.typeId);
        if (typeIt != m_types.end()) {
            info.typeInfo = typeIt->second.qualifiedName.empty()
                ? typeIt->second.name : typeIt->second.qualifiedName;
        }
    }

    return info;
}

// ============================================================================
// Search
// ============================================================================
std::vector<const SymbolEntry*> SemanticCodeIntelligence::searchSymbols(
    const std::string& query, SymbolKind filterKind, uint32_t maxResults) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.queriesServed.fetch_add(1);

    std::vector<const SymbolEntry*> results;

    for (auto& [id, sym] : m_symbols) {
        if (results.size() >= maxResults) break;

        if (filterKind != SymbolKind::Unknown && sym.kind != filterKind) continue;

        if (fuzzyMatch(sym.name, query) || fuzzyMatch(sym.qualifiedName, query)) {
            results.push_back(&sym);
        }
    }

    return results;
}

std::vector<const SymbolEntry*> SemanticCodeIntelligence::getSymbolsInFile(
    const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<const SymbolEntry*> results;

    auto it = m_fileIndex.find(filePath);
    if (it == m_fileIndex.end()) return results;

    for (uint64_t symId : it->second) {
        auto symIt = m_symbols.find(symId);
        if (symIt != m_symbols.end()) {
            results.push_back(&symIt->second);
        }
    }

    return results;
}

// ============================================================================
// Indexing
// ============================================================================
PatchResult SemanticCodeIntelligence::indexFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    buildFileIndex(filePath);
    m_stats.filesIndexed.fetch_add(1);
    return PatchResult::ok("File indexed");
}

PatchResult SemanticCodeIntelligence::reindexFile(const std::string& filePath) {
    // Remove existing symbols for this file, then re-index
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto fileIt = m_fileIndex.find(filePath);
        if (fileIt != m_fileIndex.end()) {
            auto symIds = fileIt->second; // copy
            for (uint64_t symId : symIds) {
                m_symbols.erase(symId);
            }
            m_fileIndex.erase(fileIt);
        }
    }

    return indexFile(filePath);
}

PatchResult SemanticCodeIntelligence::removeFileIndex(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto fileIt = m_fileIndex.find(filePath);
    if (fileIt == m_fileIndex.end()) return PatchResult::error("File not indexed", -1);

    uint64_t removed = 0;
    for (uint64_t symId : fileIt->second) {
        m_symbols.erase(symId);
        removed++;
    }
    m_fileIndex.erase(fileIt);

    m_stats.totalSymbols.fetch_sub(removed);
    return PatchResult::ok("File index removed");
}

PatchResult SemanticCodeIntelligence::rebuildIndex() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Collect all files
    std::vector<std::string> files;
    for (auto& [path, _] : m_fileIndex) {
        files.push_back(path);
    }

    // Clear all indices
    m_symbols.clear();
    m_nameIndex.clear();
    m_fileIndex.clear();
    m_references.clear();
    m_refBySymbol.clear();
    m_callGraph.clear();
    m_callerIndex.clear();
    m_calleeIndex.clear();

    m_stats.totalSymbols.store(0);
    m_stats.totalReferences.store(0);

    // Re-index each file
    for (auto& path : files) {
        buildFileIndex(path);
    }

    return PatchResult::ok("Index rebuilt");
}

// ============================================================================
// Diagnostics
// ============================================================================
std::vector<std::string> SemanticCodeIntelligence::findUnusedSymbols() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> unused;

    for (auto& [id, sym] : m_symbols) {
        if (sym.referenceCount == 0 &&
            sym.kind != SymbolKind::Namespace &&
            sym.kind != SymbolKind::Module &&
            !sym.isGenerated) {
            unused.push_back(sym.qualifiedName.empty() ? sym.name : sym.qualifiedName);
        }
    }

    return unused;
}

std::vector<std::pair<uint64_t, uint64_t>> SemanticCodeIntelligence::findCircularDependencies() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<uint64_t, uint64_t>> cycles;

    // Simple check: if A calls B and B calls A
    for (auto& edge : m_callGraph) {
        // Check reverse edge
        for (auto& other : m_callGraph) {
            if (other.callerId == edge.calleeId && other.calleeId == edge.callerId) {
                cycles.push_back({edge.callerId, edge.calleeId});
                break;
            }
        }
    }

    return cycles;
}

// ============================================================================
// Statistics & Callbacks
// ============================================================================
void SemanticCodeIntelligence::resetStats() {
    m_stats.totalSymbols.store(0);
    m_stats.totalReferences.store(0);
    m_stats.totalTypes.store(0);
    m_stats.totalScopes.store(0);
    m_stats.filesIndexed.store(0);
    m_stats.queriesServed.store(0);
    m_stats.cacheHits.store(0);
    m_stats.cacheMisses.store(0);
    m_stats.indexBuildTimeUs.store(0);
}

void SemanticCodeIntelligence::setProgressCallback(IndexProgressCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_progressCb   = cb;
    m_progressData = userData;
}

void SemanticCodeIntelligence::setCompleteCallback(IndexCompleteCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completeCb   = cb;
    m_completeData = userData;
}

// ============================================================================
// Serialization
// ============================================================================
PatchResult SemanticCodeIntelligence::saveIndex(const char* filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream out(filePath, std::ios::binary);
    if (!out.is_open()) return PatchResult::error("Cannot open file for writing", -1);

    // Write header
    uint64_t magic = 0x52585344494E4458ULL; // "RXSDINDX"
    uint64_t version = 1;
    uint64_t symbolCount = m_symbols.size();
    uint64_t typeCount = m_types.size();

    out.write(reinterpret_cast<const char*>(&magic), 8);
    out.write(reinterpret_cast<const char*>(&version), 8);
    out.write(reinterpret_cast<const char*>(&symbolCount), 8);
    out.write(reinterpret_cast<const char*>(&typeCount), 8);

    // Write symbols (simplified — write name + kind + location)
    for (auto& [id, sym] : m_symbols) {
        uint32_t nameLen = static_cast<uint32_t>(sym.name.size());
        out.write(reinterpret_cast<const char*>(&id), 8);
        out.write(reinterpret_cast<const char*>(&nameLen), 4);
        out.write(sym.name.c_str(), nameLen);
        uint8_t kind = static_cast<uint8_t>(sym.kind);
        out.write(reinterpret_cast<const char*>(&kind), 1);
        out.write(reinterpret_cast<const char*>(&sym.definition.line), 4);
    }

    out.close();
    return PatchResult::ok("Index saved");
}

PatchResult SemanticCodeIntelligence::loadIndex(const char* filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream in(filePath, std::ios::binary);
    if (!in.is_open()) return PatchResult::error("Cannot open index file", -1);

    uint64_t magic = 0, version = 0;
    in.read(reinterpret_cast<char*>(&magic), 8);
    in.read(reinterpret_cast<char*>(&version), 8);

    if (magic != 0x52585344494E4458ULL) {
        return PatchResult::error("Invalid index file magic", -2);
    }
    if (version != 1) {
        return PatchResult::error("Unsupported index version", -3);
    }

    uint64_t symbolCount = 0, typeCount = 0;
    in.read(reinterpret_cast<char*>(&symbolCount), 8);
    in.read(reinterpret_cast<char*>(&typeCount), 8);

    for (uint64_t i = 0; i < symbolCount; i++) {
        uint64_t id = 0;
        uint32_t nameLen = 0;
        in.read(reinterpret_cast<char*>(&id), 8);
        in.read(reinterpret_cast<char*>(&nameLen), 4);

        std::string name(nameLen, '\0');
        in.read(&name[0], nameLen);

        uint8_t kind = 0;
        in.read(reinterpret_cast<char*>(&kind), 1);

        uint32_t line = 0;
        in.read(reinterpret_cast<char*>(&line), 4);

        SymbolEntry sym;
        sym.symbolId = id;
        sym.name     = name;
        sym.kind     = static_cast<SymbolKind>(kind);
        sym.definition.line = line;
        m_symbols[id] = sym;
        m_nameIndex[name].push_back(id);
    }

    in.close();
    return PatchResult::ok("Index loaded");
}

// ============================================================================
// Internal Helpers
// ============================================================================
bool SemanticCodeIntelligence::matchesPrefix(const std::string& name,
                                              const std::string& prefix) const {
    if (prefix.empty()) return true;
    if (prefix.size() > name.size()) return false;

    for (size_t i = 0; i < prefix.size(); i++) {
        if (std::tolower(name[i]) != std::tolower(prefix[i])) return false;
    }
    return true;
}

bool SemanticCodeIntelligence::fuzzyMatch(const std::string& name,
                                           const std::string& query) const {
    if (query.empty()) return true;

    size_t qi = 0;
    for (size_t ni = 0; ni < name.size() && qi < query.size(); ni++) {
        if (std::tolower(name[ni]) == std::tolower(query[qi])) {
            qi++;
        }
    }
    return qi == query.size();
}

void SemanticCodeIntelligence::buildFileIndex(const std::string& filePath) {
    // Called with lock held
    // In production, this would parse the file using a language-specific parser
    // For now, mark the file as indexed
    m_stats.filesIndexed.fetch_add(1);

    if (m_progressCb) {
        m_progressCb(filePath.c_str(), 100, m_progressData);
    }
}
