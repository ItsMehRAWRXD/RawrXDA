// ============================================================================
// semantic_index.cpp — Enhanced SymbolIndex with Truffle/Graal-style Semantics
// ============================================================================
// Full implementation: polyglot indexing, incremental updates, dependency graph,
// type hierarchy, call graph, semantic search, C-ABI analyzer plugins.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "context/semantic_index.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>
#include <filesystem>
#include <chrono>
#include <queue>
#include <stack>
#include <cmath>
#include <numeric>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace SemanticIndex {

// ============================================================================
// Singleton
// ============================================================================
SemanticIndexEngine& SemanticIndexEngine::Instance() {
    static SemanticIndexEngine instance;
    return instance;
}

SemanticIndexEngine::~SemanticIndexEngine() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
void SemanticIndexEngine::Initialize(const std::string& workspaceRoot) {
    if (m_initialized.load()) return;
    m_workspaceRoot = workspaceRoot;
    m_initialized.store(true);
}

void SemanticIndexEngine::Shutdown() {
    if (!m_initialized.load()) return;
    UnloadAllPlugins();
    m_initialized.store(false);
}

// ============================================================================
// Full Build
// ============================================================================
IndexStatistics SemanticIndexEngine::BuildIndex(bool recursive) {
    auto startTime = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_symbols.clear();
        m_fileSymbols.clear();
        m_deps.clear();
        m_reverseDeps.clear();
        m_typeNodes.clear();
        m_callEdges.clear();
        m_callerIndex.clear();
        m_calleeIndex.clear();
        m_fileTimestamps.clear();
    }
    
    // Walk the workspace
    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(
                     m_workspaceRoot, fs::directory_options::skip_permission_denied)) {
                if (entry.is_regular_file()) {
                    indexFile(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(m_workspaceRoot)) {
                if (entry.is_regular_file()) {
                    indexFile(entry.path().string());
                }
            }
        }
    } catch (...) {
        // Filesystem errors — continue with what we have
    }
    
    auto endTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalFiles = m_fileSymbols.size();
        m_stats.totalSymbols = m_symbols.size();
        m_stats.totalDeps = m_deps.size();
        m_stats.totalCallEdges = m_callEdges.size();
        m_stats.totalTypeNodes = m_typeNodes.size();
        m_stats.buildTimeMs = elapsed;
        m_stats.lastBuildTimestamp = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        
        m_stats.symbolsByLanguage.clear();
        m_stats.symbolsByKind.clear();
        for (const auto& [id, sym] : m_symbols) {
            m_stats.symbolsByLanguage[sym.language]++;
            m_stats.symbolsByKind[std::to_string(static_cast<uint16_t>(sym.kind))]++;
            m_stats.totalReferences += sym.references.size();
        }
        m_stats.languageCount = m_stats.symbolsByLanguage.size();
    }
    
    return GetStats();
}

// ============================================================================
// Incremental Updates
// ============================================================================
void SemanticIndexEngine::NotifyFileChange(const FileChange& change) {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pendingChanges.push_back(change);
}

void SemanticIndexEngine::ProcessPendingChanges() {
    std::vector<FileChange> changes;
    {
        std::lock_guard<std::mutex> lock(m_pendingMutex);
        changes.swap(m_pendingChanges);
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    for (const auto& change : changes) {
        switch (change.type) {
        case FileChangeType::Created:
        case FileChangeType::Modified:
            removeFileSymbols(change.filePath);
            indexFile(change.filePath);
            break;
        case FileChangeType::Deleted:
            removeFileSymbols(change.filePath);
            break;
        case FileChangeType::Renamed:
            removeFileSymbols(change.oldPath);
            indexFile(change.filePath);
            break;
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.lastIncrUpdateMs =
            std::chrono::duration<double, std::milli>(endTime - startTime).count();
        m_stats.totalSymbols = m_symbols.size();
        m_stats.totalFiles = m_fileSymbols.size();
    }
}

// ============================================================================
// Symbol Queries
// ============================================================================
QueryResult SemanticIndexEngine::FindByName(const std::string& name,
                                             const QueryOptions& opts) const {
    auto startTime = std::chrono::steady_clock::now();
    QueryResult result;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int count = 0;
    for (const auto& [id, sym] : m_symbols) {
        if (count >= opts.maxResults) break;
        
        bool match = opts.fuzzyMatch ? 
            (fuzzyScore(name, sym.name) > opts.minRelevance) :
            (sym.name == name);
        
        if (!match) continue;
        
        // Apply filters
        if (!opts.languageFilter.empty()) {
            bool langOk = false;
            for (const auto& l : opts.languageFilter) {
                if (sym.language == l) { langOk = true; break; }
            }
            if (!langOk) continue;
        }
        if (!opts.kindFilter.empty()) {
            bool kindOk = false;
            for (auto k : opts.kindFilter) {
                if (sym.kind == k) { kindOk = true; break; }
            }
            if (!kindOk) continue;
        }
        if (!opts.scopeFile.empty() && sym.definition.filePath != opts.scopeFile) continue;
        
        result.symbols.push_back(sym);
        count++;
    }
    
    result.totalMatches = count;
    auto endTime = std::chrono::steady_clock::now();
    result.queryTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}

QueryResult SemanticIndexEngine::FindByQualifiedName(const std::string& qn,
                                                      const QueryOptions& opts) const {
    auto startTime = std::chrono::steady_clock::now();
    QueryResult result;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [id, sym] : m_symbols) {
        if ((int32_t)result.symbols.size() >= opts.maxResults) break;
        if (sym.qualifiedName == qn) {
            result.symbols.push_back(sym);
        }
    }
    
    result.totalMatches = static_cast<int32_t>(result.symbols.size());
    auto endTime = std::chrono::steady_clock::now();
    result.queryTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}

QueryResult SemanticIndexEngine::FindByKind(SymbolKind kind,
                                             const QueryOptions& opts) const {
    auto startTime = std::chrono::steady_clock::now();
    QueryResult result;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [id, sym] : m_symbols) {
        if ((int32_t)result.symbols.size() >= opts.maxResults) break;
        if (sym.kind == kind) {
            if (!opts.languageFilter.empty()) {
                bool langOk = false;
                for (const auto& l : opts.languageFilter) {
                    if (sym.language == l) { langOk = true; break; }
                }
                if (!langOk) continue;
            }
            result.symbols.push_back(sym);
        }
    }
    
    result.totalMatches = static_cast<int32_t>(result.symbols.size());
    auto endTime = std::chrono::steady_clock::now();
    result.queryTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}

QueryResult SemanticIndexEngine::FindInFile(const std::string& filePath,
                                              const QueryOptions& opts) const {
    auto startTime = std::chrono::steady_clock::now();
    QueryResult result;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_fileSymbols.find(filePath);
    if (it != m_fileSymbols.end()) {
        for (const auto& symId : it->second) {
            if ((int32_t)result.symbols.size() >= opts.maxResults) break;
            auto symIt = m_symbols.find(symId);
            if (symIt != m_symbols.end()) {
                result.symbols.push_back(symIt->second);
            }
        }
    }
    
    result.totalMatches = static_cast<int32_t>(result.symbols.size());
    auto endTime = std::chrono::steady_clock::now();
    result.queryTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}

QueryResult SemanticIndexEngine::FindInScope(const std::string& scopeId,
                                               const QueryOptions& opts) const {
    auto startTime = std::chrono::steady_clock::now();
    QueryResult result;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& [id, sym] : m_symbols) {
        if ((int32_t)result.symbols.size() >= opts.maxResults) break;
        if (sym.parentId == scopeId) {
            result.symbols.push_back(sym);
        }
    }
    
    result.totalMatches = static_cast<int32_t>(result.symbols.size());
    auto endTime = std::chrono::steady_clock::now();
    result.queryTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}

QueryResult SemanticIndexEngine::FuzzySearch(const std::string& query,
                                               const QueryOptions& opts) const {
    auto startTime = std::chrono::steady_clock::now();
    QueryResult result;
    std::lock_guard<std::mutex> lock(m_mutex);
    
    struct Scored {
        RichSymbol sym;
        float score;
    };
    std::vector<Scored> scored;
    
    for (const auto& [id, sym] : m_symbols) {
        float nameScore = fuzzyScore(query, sym.name);
        float qnScore = fuzzyScore(query, sym.qualifiedName) * 0.8f;
        float best = std::max(nameScore, qnScore);
        
        if (best < opts.minRelevance && opts.minRelevance > 0.0f) continue;
        if (best < 0.1f) continue;
        
        // Apply filters
        if (!opts.languageFilter.empty()) {
            bool ok = false;
            for (const auto& l : opts.languageFilter) {
                if (sym.language == l) { ok = true; break; }
            }
            if (!ok) continue;
        }
        if (!opts.kindFilter.empty()) {
            bool ok = false;
            for (auto k : opts.kindFilter) {
                if (sym.kind == k) { ok = true; break; }
            }
            if (!ok) continue;
        }
        
        RichSymbol copy = sym;
        copy.relevanceScore = best;
        scored.push_back({copy, best});
    }
    
    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.score > b.score; });
    
    int limit = std::min(opts.maxResults, static_cast<int32_t>(scored.size()));
    for (int i = 0; i < limit; ++i) {
        result.symbols.push_back(std::move(scored[i].sym));
    }
    
    result.totalMatches = static_cast<int32_t>(scored.size());
    auto endTime = std::chrono::steady_clock::now();
    result.queryTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}

// ============================================================================
// Semantic Search (embedding-based)
// ============================================================================
QueryResult SemanticIndexEngine::SemanticSearch(const std::string& naturalLanguage,
                                                 int topK) const {
    QueryResult result;
    
    if (!m_embeddingFn) {
        result.error = "No embedding callback set";
        return result;
    }
    
    auto queryVec = m_embeddingFn(naturalLanguage);
    if (queryVec.empty()) {
        result.error = "Embedding failed for query";
        return result;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    struct Scored { std::string id; float score; };
    std::vector<Scored> scored;
    
    auto cosine = [](const std::vector<float>& a, const std::vector<float>& b) -> float {
        if (a.size() != b.size() || a.empty()) return 0.0f;
        float dot = 0, nA = 0, nB = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            dot += a[i] * b[i];
            nA += a[i] * a[i];
            nB += b[i] * b[i];
        }
        float denom = std::sqrt(nA) * std::sqrt(nB);
        return (denom > 1e-9f) ? (dot / denom) : 0.0f;
    };
    
    for (const auto& [id, emb] : m_embeddings) {
        float sim = cosine(queryVec, emb);
        if (sim > 0.1f) scored.push_back({id, sim});
    }
    
    std::sort(scored.begin(), scored.end(),
              [](const Scored& a, const Scored& b) { return a.score > b.score; });
    
    int limit = std::min(topK, static_cast<int>(scored.size()));
    for (int i = 0; i < limit; ++i) {
        auto symIt = m_symbols.find(scored[i].id);
        if (symIt != m_symbols.end()) {
            RichSymbol copy = symIt->second;
            copy.relevanceScore = scored[i].score;
            result.symbols.push_back(std::move(copy));
        }
    }
    
    result.totalMatches = static_cast<int32_t>(scored.size());
    auto endTime = std::chrono::steady_clock::now();
    result.queryTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return result;
}

void SemanticIndexEngine::SetEmbeddingCallback(
    std::function<std::vector<float>(const std::string&)> cb) {
    m_embeddingFn = std::move(cb);
}

// ============================================================================
// Symbol Details
// ============================================================================
const RichSymbol* SemanticIndexEngine::GetSymbol(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_symbols.find(id);
    return (it != m_symbols.end()) ? &it->second : nullptr;
}

std::vector<SourceLocation> SemanticIndexEngine::GetReferences(
    const std::string& symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_symbols.find(symbolId);
    return (it != m_symbols.end()) ? it->second.references : std::vector<SourceLocation>{};
}

std::vector<SourceLocation> SemanticIndexEngine::GetDefinitions(
    const std::string& symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_symbols.find(symbolId);
    if (it == m_symbols.end()) return {};
    
    std::vector<SourceLocation> locs;
    locs.push_back(it->second.definition);
    for (const auto& d : it->second.declarations) locs.push_back(d);
    return locs;
}

// ============================================================================
// Dependency Graph
// ============================================================================
std::vector<DependencyEdge> SemanticIndexEngine::GetFileDependencies(
    const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DependencyEdge> out;
    auto range = m_deps.equal_range(filePath);
    for (auto it = range.first; it != range.second; ++it) {
        out.push_back(it->second);
    }
    return out;
}

std::vector<DependencyEdge> SemanticIndexEngine::GetFileDependents(
    const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DependencyEdge> out;
    auto range = m_reverseDeps.equal_range(filePath);
    for (auto it = range.first; it != range.second; ++it) {
        out.push_back(it->second);
    }
    return out;
}

std::vector<std::string> SemanticIndexEngine::GetTransitiveDependencies(
    const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::unordered_set<std::string> visited;
    std::queue<std::string> q;
    q.push(filePath);
    visited.insert(filePath);
    
    while (!q.empty()) {
        std::string f = q.front();
        q.pop();
        auto range = m_deps.equal_range(f);
        for (auto it = range.first; it != range.second; ++it) {
            if (visited.insert(it->second.targetFile).second) {
                q.push(it->second.targetFile);
            }
        }
    }
    
    visited.erase(filePath);
    return std::vector<std::string>(visited.begin(), visited.end());
}

std::vector<std::string> SemanticIndexEngine::FindCycles() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> cycles;
    
    // Collect all files
    std::unordered_set<std::string> allFiles;
    for (const auto& [file, _] : m_deps) allFiles.insert(file);
    
    // DFS-based cycle detection
    std::unordered_set<std::string> visited, inStack;
    
    std::function<bool(const std::string&, std::vector<std::string>&)> dfs;
    dfs = [&](const std::string& node, std::vector<std::string>& path) -> bool {
        visited.insert(node);
        inStack.insert(node);
        path.push_back(node);
        
        auto range = m_deps.equal_range(node);
        for (auto it = range.first; it != range.second; ++it) {
            const std::string& target = it->second.targetFile;
            if (inStack.count(target)) {
                // Found cycle — record it
                std::string cycleStr;
                auto cit = std::find(path.begin(), path.end(), target);
                for (; cit != path.end(); ++cit) {
                    cycleStr += *cit + " -> ";
                }
                cycleStr += target;
                cycles.push_back(cycleStr);
                return true;
            }
            if (!visited.count(target)) {
                dfs(target, path);
            }
        }
        
        path.pop_back();
        inStack.erase(node);
        return false;
    };
    
    for (const auto& file : allFiles) {
        if (!visited.count(file)) {
            std::vector<std::string> path;
            dfs(file, path);
        }
    }
    
    return cycles;
}

// ============================================================================
// Type Hierarchy
// ============================================================================
TypeNode SemanticIndexEngine::GetTypeNode(const std::string& symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_typeNodes.find(symbolId);
    return (it != m_typeNodes.end()) ? it->second : TypeNode{};
}

std::vector<TypeNode> SemanticIndexEngine::GetSuperTypes(
    const std::string& symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TypeNode> result;
    auto it = m_typeNodes.find(symbolId);
    if (it == m_typeNodes.end()) return result;
    
    // BFS upward
    std::queue<std::string> q;
    std::unordered_set<std::string> visited;
    for (const auto& p : it->second.parents) q.push(p);
    
    while (!q.empty()) {
        std::string id = q.front();
        q.pop();
        if (!visited.insert(id).second) continue;
        
        auto nodeIt = m_typeNodes.find(id);
        if (nodeIt != m_typeNodes.end()) {
            result.push_back(nodeIt->second);
            for (const auto& p : nodeIt->second.parents) q.push(p);
        }
    }
    return result;
}

std::vector<TypeNode> SemanticIndexEngine::GetSubTypes(
    const std::string& symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<TypeNode> result;
    auto it = m_typeNodes.find(symbolId);
    if (it == m_typeNodes.end()) return result;
    
    // BFS downward
    std::queue<std::string> q;
    std::unordered_set<std::string> visited;
    for (const auto& c : it->second.children) q.push(c);
    
    while (!q.empty()) {
        std::string id = q.front();
        q.pop();
        if (!visited.insert(id).second) continue;
        
        auto nodeIt = m_typeNodes.find(id);
        if (nodeIt != m_typeNodes.end()) {
            result.push_back(nodeIt->second);
            for (const auto& c : nodeIt->second.children) q.push(c);
        }
    }
    return result;
}

std::vector<TypeNode> SemanticIndexEngine::GetTypeHierarchyTree(
    const std::string& symbolId) const {
    std::vector<TypeNode> result;
    auto supers = GetSuperTypes(symbolId);
    auto subs = GetSubTypes(symbolId);
    
    result.insert(result.end(), supers.begin(), supers.end());
    
    auto node = GetTypeNode(symbolId);
    if (!node.symbolId.empty()) result.push_back(node);
    
    result.insert(result.end(), subs.begin(), subs.end());
    return result;
}

// ============================================================================
// Call Graph
// ============================================================================
std::vector<CallEdge> SemanticIndexEngine::GetCallers(
    const std::string& symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CallEdge> result;
    auto it = m_calleeIndex.find(symbolId);
    if (it != m_calleeIndex.end()) {
        for (size_t idx : it->second) {
            if (idx < m_callEdges.size()) result.push_back(m_callEdges[idx]);
        }
    }
    return result;
}

std::vector<CallEdge> SemanticIndexEngine::GetCallees(
    const std::string& symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CallEdge> result;
    auto it = m_callerIndex.find(symbolId);
    if (it != m_callerIndex.end()) {
        for (size_t idx : it->second) {
            if (idx < m_callEdges.size()) result.push_back(m_callEdges[idx]);
        }
    }
    return result;
}

std::vector<CallEdge> SemanticIndexEngine::GetCallChain(const std::string& from,
                                                          const std::string& to,
                                                          int maxDepth) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CallEdge> chain;
    
    // BFS from 'from' to 'to'
    struct PathNode {
        std::string symbolId;
        std::vector<CallEdge> path;
    };
    
    std::queue<PathNode> q;
    std::unordered_set<std::string> visited;
    q.push({from, {}});
    visited.insert(from);
    
    while (!q.empty()) {
        auto node = q.front();
        q.pop();
        
        if ((int)node.path.size() >= maxDepth) continue;
        
        auto it = m_callerIndex.find(node.symbolId);
        if (it == m_callerIndex.end()) continue;
        
        for (size_t idx : it->second) {
            if (idx >= m_callEdges.size()) continue;
            const auto& edge = m_callEdges[idx];
            
            auto newPath = node.path;
            newPath.push_back(edge);
            
            if (edge.calleeId == to) return newPath;
            
            if (visited.insert(edge.calleeId).second) {
                q.push({edge.calleeId, newPath});
            }
        }
    }
    
    return chain; // Empty if no path found
}

// ============================================================================
// Statistics
// ============================================================================
IndexStatistics SemanticIndexEngine::GetStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

// ============================================================================
// Plugin Management
// ============================================================================
bool SemanticIndexEngine::LoadAnalyzerPlugin(const std::string& dllPath) {
#ifdef _WIN32
    HMODULE hMod = LoadLibraryA(dllPath.c_str());
    if (!hMod) return false;
    
    LoadedPlugin plugin;
    plugin.path = dllPath;
    plugin.hModule = hMod;
    
    plugin.fnGetInfo = reinterpret_cast<AnalyzerPlugin_GetInfo_fn>(
        GetProcAddress(hMod, "AnalyzerPlugin_GetInfo"));
    plugin.fnInit = reinterpret_cast<AnalyzerPlugin_Init_fn>(
        GetProcAddress(hMod, "AnalyzerPlugin_Init"));
    plugin.fnAnalyze = reinterpret_cast<AnalyzerPlugin_AnalyzeFile_fn>(
        GetProcAddress(hMod, "AnalyzerPlugin_AnalyzeFile"));
    plugin.fnGetDeps = reinterpret_cast<AnalyzerPlugin_GetDependencies_fn>(
        GetProcAddress(hMod, "AnalyzerPlugin_GetDependencies"));
    plugin.fnShutdown = reinterpret_cast<AnalyzerPlugin_Shutdown_fn>(
        GetProcAddress(hMod, "AnalyzerPlugin_Shutdown"));
    
    if (!plugin.fnGetInfo || !plugin.fnAnalyze) {
        FreeLibrary(hMod);
        return false;
    }
    
    auto* info = plugin.fnGetInfo();
    if (info) {
        plugin.name = info->name;
        // Parse supported languages
        if (info->supportedLanguages) {
            std::istringstream ss(info->supportedLanguages);
            std::string lang;
            while (std::getline(ss, lang, ',')) {
                while (!lang.empty() && lang[0] == ' ') lang.erase(0, 1);
                while (!lang.empty() && lang.back() == ' ') lang.pop_back();
                if (!lang.empty()) plugin.languages.push_back(lang);
            }
        }
    }
    
    if (plugin.fnInit) plugin.fnInit("{}");
    
    m_plugins.push_back(std::move(plugin));
    return true;
#else
    (void)dllPath;
    return false;
#endif
}

void SemanticIndexEngine::UnloadAnalyzerPlugin(const std::string& name) {
    auto it = std::find_if(m_plugins.begin(), m_plugins.end(),
                            [&](const LoadedPlugin& p) { return p.name == name; });
    if (it == m_plugins.end()) return;
    
    if (it->fnShutdown) it->fnShutdown();
#ifdef _WIN32
    if (it->hModule) FreeLibrary(static_cast<HMODULE>(it->hModule));
#endif
    m_plugins.erase(it);
}

void SemanticIndexEngine::UnloadAllPlugins() {
    for (auto& p : m_plugins) {
        if (p.fnShutdown) p.fnShutdown();
#ifdef _WIN32
        if (p.hModule) FreeLibrary(static_cast<HMODULE>(p.hModule));
#endif
    }
    m_plugins.clear();
}

std::vector<std::string> SemanticIndexEngine::GetLoadedPlugins() const {
    std::vector<std::string> names;
    for (const auto& p : m_plugins) names.push_back(p.name);
    return names;
}

// ============================================================================
// Serialization
// ============================================================================
bool SemanticIndexEngine::SaveIndex(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ofstream out(filePath);
    if (!out.is_open()) return false;
    
    // Simple JSON-like format
    out << "{\n\"version\": 1,\n\"symbols\": [\n";
    bool first = true;
    for (const auto& [id, sym] : m_symbols) {
        if (!first) out << ",\n";
        first = false;
        out << "{\"id\":\"" << sym.id << "\","
            << "\"name\":\"" << sym.name << "\","
            << "\"qn\":\"" << sym.qualifiedName << "\","
            << "\"lang\":\"" << sym.language << "\","
            << "\"kind\":" << static_cast<uint16_t>(sym.kind) << ","
            << "\"file\":\"" << sym.definition.filePath << "\","
            << "\"line\":" << sym.definition.startLine << ","
            << "\"sig\":\"" << sym.typeSignature << "\","
            << "\"parent\":\"" << sym.parentId << "\""
            << "}";
    }
    out << "\n],\n\"deps\": [\n";
    first = true;
    for (const auto& [file, dep] : m_deps) {
        if (!first) out << ",\n";
        first = false;
        out << "{\"src\":\"" << dep.sourceFile << "\","
            << "\"tgt\":\"" << dep.targetFile << "\","
            << "\"expr\":\"" << dep.importExpr << "\"}";
    }
    out << "\n]\n}\n";
    
    return out.good();
}

bool SemanticIndexEngine::LoadIndex(const std::string& filePath) {
    std::ifstream in(filePath);
    if (!in.is_open()) return false;
    
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    in.close();

    // Validate JSON envelope
    if (content.find("\"version\"") == std::string::npos) return false;

    // Parse symbols array
    size_t symStart = content.find("\"symbols\"");
    if (symStart == std::string::npos) return false;

    size_t arrStart = content.find('[', symStart);
    size_t arrEnd = content.find(']', arrStart);
    if (arrStart == std::string::npos || arrEnd == std::string::npos) return false;

    std::string symArray = content.substr(arrStart, arrEnd - arrStart + 1);

    // Parse each symbol entry: {"id":"...","name":"...","kind":N,...}
    auto extractStr = [](const std::string& json, size_t start,
                         const std::string& key) -> std::string {
        std::string pat = "\"" + key + "\":\"";
        auto pos = json.find(pat, start);
        if (pos == std::string::npos) return "";
        auto qStart = pos + pat.size();
        auto qEnd = json.find('"', qStart);
        if (qEnd == std::string::npos) return "";
        return json.substr(qStart, qEnd - qStart);
    };

    auto extractInt = [](const std::string& json, size_t start,
                         const std::string& key) -> int {
        std::string pat = "\"" + key + "\":";
        auto pos = json.find(pat, start);
        if (pos == std::string::npos) return 0;
        auto valStart = pos + pat.size();
        return atoi(json.c_str() + valStart);
    };

    std::lock_guard<std::mutex> lock(m_mutex);
    m_symbols.clear();
    m_deps.clear();

    size_t searchPos = 0;
    while ((searchPos = symArray.find('{', searchPos)) != std::string::npos) {
        size_t entryEnd = symArray.find('}', searchPos);
        if (entryEnd == std::string::npos) break;

        SymbolEntry sym;
        sym.id = extractStr(symArray, searchPos, "id");
        sym.name = extractStr(symArray, searchPos, "name");
        sym.kind = static_cast<SymbolKind>(extractInt(symArray, searchPos, "kind"));
        sym.file = extractStr(symArray, searchPos, "file");
        sym.line = extractInt(symArray, searchPos, "line");
        sym.typeSignature = extractStr(symArray, searchPos, "sig");
        sym.parentId = extractStr(symArray, searchPos, "parent");

        if (!sym.id.empty()) {
            m_symbols[sym.id] = sym;
        }

        searchPos = entryEnd + 1;
    }

    // Parse deps array
    size_t depStart = content.find("\"deps\"");
    if (depStart != std::string::npos) {
        size_t depArrStart = content.find('[', depStart);
        size_t depArrEnd = content.find(']', depArrStart);
        if (depArrStart != std::string::npos && depArrEnd != std::string::npos) {
            std::string depArray = content.substr(depArrStart, depArrEnd - depArrStart + 1);
            size_t dpos = 0;
            while ((dpos = depArray.find('{', dpos)) != std::string::npos) {
                size_t dEnd = depArray.find('}', dpos);
                if (dEnd == std::string::npos) break;

                DependencyEdge dep;
                dep.sourceFile = extractStr(depArray, dpos, "src");
                dep.targetFile = extractStr(depArray, dpos, "tgt");
                dep.importExpr = extractStr(depArray, dpos, "expr");

                if (!dep.sourceFile.empty()) {
                    std::string key = dep.sourceFile + "->" + dep.targetFile;
                    m_deps[key] = dep;
                }
                dpos = dEnd + 1;
            }
        }
    }

    return true;
}

// ============================================================================
// Cross-Language Navigation
// ============================================================================
void SemanticIndexEngine::AddCrossLanguageBinding(const CrossLangRef& ref) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_crossLangRefs.push_back(ref);
}

std::vector<SemanticIndexEngine::CrossLangRef> SemanticIndexEngine::GetCrossLanguageBindings(
    const std::string& symbolId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CrossLangRef> result;
    for (const auto& ref : m_crossLangRefs) {
        if (ref.sourceSymbolId == symbolId || ref.targetSymbolId == symbolId) {
            result.push_back(ref);
        }
    }
    return result;
}

// ============================================================================
// Internal: File Indexing
// ============================================================================
void SemanticIndexEngine::indexFile(const std::string& filePath) {
    std::string lang = detectLanguage(filePath);
    if (lang.empty()) return;
    
    // Read file content
    std::ifstream file(filePath);
    if (!file.is_open()) return;
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    // Try plugins first
    bool pluginHandled = false;
    for (const auto& plugin : m_plugins) {
        for (const auto& plang : plugin.languages) {
            if (plang == lang) {
                indexFileWithPlugin(filePath, content);
                pluginHandled = true;
                break;
            }
        }
        if (pluginHandled) break;
    }
    
    // Fall back to built-in analyzers
    if (!pluginHandled) {
        if (lang == "cpp" || lang == "c" || lang == "h") analyzeCpp(filePath, content);
        else if (lang == "python") analyzePython(filePath, content);
        else if (lang == "javascript" || lang == "typescript") analyzeJavaScript(filePath, content);
        else if (lang == "rust") analyzeRust(filePath, content);
        else if (lang == "go") analyzeGo(filePath, content);
        else if (lang == "masm" || lang == "asm") analyzeMasm(filePath, content);
    }
    
    // Extract dependencies
    extractDependencies(filePath, content, lang);
    
    // Update timestamp
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileTimestamps[filePath] = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    // Generate embeddings if callback available
    if (m_embeddingFn) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_fileSymbols.find(filePath);
        if (it != m_fileSymbols.end()) {
            for (const auto& symId : it->second) {
                auto symIt = m_symbols.find(symId);
                if (symIt != m_symbols.end()) {
                    std::string text = symIt->second.qualifiedName + " " +
                                       symIt->second.typeSignature + " " +
                                       symIt->second.documentation;
                    m_embeddings[symId] = m_embeddingFn(text);
                }
            }
        }
    }
}

void SemanticIndexEngine::indexFileWithPlugin(const std::string& filePath,
                                                const std::string& content) {
    for (auto& plugin : m_plugins) {
        if (!plugin.fnAnalyze) continue;
        
        CRichSymbol cSyms[512];
        int count = plugin.fnAnalyze(filePath.c_str(), content.c_str(), cSyms, 512);
        
        std::lock_guard<std::mutex> lock(m_mutex);
        for (int i = 0; i < count; ++i) {
            RichSymbol sym;
            sym.id = cSyms[i].id ? cSyms[i].id : makeSymbolId(filePath,
                static_cast<SymbolKind>(cSyms[i].kind),
                cSyms[i].name ? cSyms[i].name : "", cSyms[i].startLine);
            sym.name = cSyms[i].name ? cSyms[i].name : "";
            sym.qualifiedName = cSyms[i].qualifiedName ? cSyms[i].qualifiedName : sym.name;
            sym.language = cSyms[i].language ? cSyms[i].language : "";
            sym.kind = static_cast<SymbolKind>(cSyms[i].kind);
            sym.visibility = static_cast<SymbolVisibility>(cSyms[i].visibility);
            sym.definition.filePath = filePath;
            sym.definition.startLine = cSyms[i].startLine;
            sym.definition.endLine = cSyms[i].endLine;
            if (cSyms[i].typeSignature) sym.typeSignature = cSyms[i].typeSignature;
            if (cSyms[i].returnType) sym.returnType = cSyms[i].returnType;
            if (cSyms[i].documentation) sym.documentation = cSyms[i].documentation;
            if (cSyms[i].parentId) sym.parentId = cSyms[i].parentId;
            
            m_symbols[sym.id] = std::move(sym);
            m_fileSymbols[filePath].push_back(sym.id.empty() ? cSyms[i].id : sym.id);
        }
    }
}

void SemanticIndexEngine::removeFileSymbols(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_fileSymbols.find(filePath);
    if (it != m_fileSymbols.end()) {
        for (const auto& symId : it->second) {
            m_symbols.erase(symId);
            m_typeNodes.erase(symId);
            m_embeddings.erase(symId);
        }
        m_fileSymbols.erase(it);
    }
    
    // Remove dependency edges
    m_deps.erase(filePath);
    
    // Remove reverse deps pointing to this file
    for (auto rit = m_reverseDeps.begin(); rit != m_reverseDeps.end(); ) {
        if (rit->second.sourceFile == filePath) {
            rit = m_reverseDeps.erase(rit);
        } else {
            ++rit;
        }
    }
    
    m_fileTimestamps.erase(filePath);
}

std::string SemanticIndexEngine::detectLanguage(const std::string& filePath) const {
    auto dotPos = filePath.rfind('.');
    if (dotPos == std::string::npos) return "";
    
    std::string ext = filePath.substr(dotPos);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    static const std::unordered_map<std::string, std::string> extMap = {
        {".cpp", "cpp"}, {".cxx", "cpp"}, {".cc", "cpp"}, {".c", "c"},
        {".h", "cpp"}, {".hpp", "cpp"}, {".hxx", "cpp"}, {".hh", "cpp"}, {".inl", "cpp"},
        {".py", "python"}, {".pyw", "python"}, {".pyi", "python"},
        {".js", "javascript"}, {".mjs", "javascript"}, {".cjs", "javascript"}, {".jsx", "javascript"},
        {".ts", "typescript"}, {".tsx", "typescript"}, {".mts", "typescript"},
        {".rs", "rust"},
        {".go", "go"},
        {".asm", "masm"}, {".inc", "masm"}, {".masm", "masm"},
    };
    
    auto it = extMap.find(ext);
    return (it != extMap.end()) ? it->second : "";
}

// ============================================================================
// Built-in Analyzers (regex-based, like existing Indexer)
// ============================================================================
void SemanticIndexEngine::analyzeCpp(const std::string& filePath,
                                      const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& fileSyms = m_fileSymbols[filePath];
    
    auto lines = std::istringstream(content);
    std::string line;
    int lineNum = 0;
    std::string currentClass;
    std::string currentNamespace;
    
    while (std::getline(lines, line)) {
        lineNum++;
        
        // Namespace
        std::smatch nm;
        if (std::regex_search(line, nm, std::regex(R"(namespace\s+(\w+))"))) {
            currentNamespace = nm[1].str();
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Namespace, currentNamespace, lineNum);
            sym.name = currentNamespace;
            sym.qualifiedName = currentNamespace;
            sym.language = "cpp";
            sym.kind = SymbolKind::Namespace;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(sym.id.empty() ? currentNamespace : m_symbols.rbegin()->first);
            continue;
        }
        
        // Class/struct
        std::smatch cm;
        if (std::regex_search(line, cm,
                std::regex(R"((?:class|struct)\s+(?:\w+\s+)*(\w+)(?:\s*:\s*(.*?))?(?:\s*\{)?)"))) {
            std::string className = cm[1].str();
            std::string bases = cm.size() > 2 ? cm[2].str() : "";
            currentClass = className;
            
            std::string qn = currentNamespace.empty() ? className :
                              (currentNamespace + "::" + className);
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Class, className, lineNum);
            sym.name = className;
            sym.qualifiedName = qn;
            sym.language = "cpp";
            sym.kind = line.find("struct") != std::string::npos 
                       ? SymbolKind::Struct : SymbolKind::Class;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            sym.visibility = line.find("struct") != std::string::npos 
                             ? SymbolVisibility::Public : SymbolVisibility::Private;
            
            // Parse base classes
            if (!bases.empty()) {
                std::regex baseRe(R"(\b(\w+)\b)");
                std::sregex_iterator bit(bases.begin(), bases.end(), baseRe);
                std::sregex_iterator bend;
                for (; bit != bend; ++bit) {
                    std::string b = (*bit)[1].str();
                    if (b != "public" && b != "protected" && b != "private" && b != "virtual") {
                        sym.baseTypes.push_back(b);
                    }
                }
            }
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            
            // Create type node
            TypeNode tn;
            tn.symbolId = m_symbols.rbegin()->first;
            tn.name = className;
            tn.language = "cpp";
            tn.kind = SymbolKind::Class;
            tn.parents = m_symbols.rbegin()->second.baseTypes;
            m_typeNodes[tn.symbolId] = tn;
            continue;
        }
        
        // Function/method
        std::smatch fm;
        if (std::regex_search(line, fm,
                std::regex(R"((\w[\w:*&<> ]*?)\s+(\w+)\s*\(([^)]*)\)\s*(?:const\s*)?(?:override\s*)?(?:=\s*\w+\s*)?(?:\{|;))"))) {
            std::string retType = fm[1].str();
            std::string funcName = fm[2].str();
            std::string params = fm[3].str();
            
            // Skip common false positives
            if (funcName == "if" || funcName == "while" || funcName == "for" ||
                funcName == "switch" || funcName == "catch" || funcName == "return") continue;
            
            SymbolKind fKind = currentClass.empty() ? SymbolKind::Function : SymbolKind::Method;
            std::string qn;
            if (!currentNamespace.empty()) qn += currentNamespace + "::";
            if (!currentClass.empty()) qn += currentClass + "::";
            qn += funcName;
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, fKind, funcName, lineNum);
            sym.name = funcName;
            sym.qualifiedName = qn;
            sym.language = "cpp";
            sym.kind = fKind;
            sym.returnType = retType;
            sym.typeSignature = retType + "(" + params + ")";
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            
            if (!currentClass.empty()) {
                sym.parentId = makeSymbolId(filePath, SymbolKind::Class, currentClass, 0);
            }
            
            // Parse parameters for call graph wiring
            std::regex paramTypeRe(R"((\w[\w:*&<> ]*)\s+(\w+))");
            std::sregex_iterator pit(params.begin(), params.end(), paramTypeRe);
            std::sregex_iterator pend;
            for (; pit != pend; ++pit) {
                sym.paramTypes.push_back((*pit)[1].str());
            }
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Enum
        std::smatch em;
        if (std::regex_search(line, em, std::regex(R"(enum\s+(?:class\s+)?(\w+))"))) {
            std::string enumName = em[1].str();
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Enum, enumName, lineNum);
            sym.name = enumName;
            sym.qualifiedName = currentNamespace.empty() ? enumName :
                                 (currentNamespace + "::" + enumName);
            sym.language = "cpp";
            sym.kind = SymbolKind::Enum;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Macro
        std::smatch mm;
        if (std::regex_search(line, mm, std::regex(R"(#define\s+(\w+))"))) {
            std::string macroName = mm[1].str();
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Macro, macroName, lineNum);
            sym.name = macroName;
            sym.qualifiedName = macroName;
            sym.language = "cpp";
            sym.kind = SymbolKind::Macro;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
    }
}

void SemanticIndexEngine::analyzePython(const std::string& filePath,
                                         const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& fileSyms = m_fileSymbols[filePath];
    
    auto lines = std::istringstream(content);
    std::string line;
    int lineNum = 0;
    std::string currentClass;
    
    while (std::getline(lines, line)) {
        lineNum++;
        
        // Class
        std::smatch cm;
        if (std::regex_search(line, cm, std::regex(R"(class\s+(\w+)(?:\((.+?)\))?)"))) {
            currentClass = cm[1].str();
            std::string bases = cm.size() > 2 ? cm[2].str() : "";
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Class, currentClass, lineNum);
            sym.name = currentClass;
            sym.qualifiedName = currentClass;
            sym.language = "python";
            sym.kind = SymbolKind::Class;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            
            if (!bases.empty()) {
                std::regex baseRe(R"(\b(\w+)\b)");
                std::sregex_iterator bit(bases.begin(), bases.end(), baseRe);
                for (; bit != std::sregex_iterator{}; ++bit) {
                    sym.baseTypes.push_back((*bit)[1].str());
                }
            }
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            
            TypeNode tn;
            tn.symbolId = m_symbols.rbegin()->first;
            tn.name = currentClass;
            tn.language = "python";
            tn.kind = SymbolKind::Class;
            m_typeNodes[tn.symbolId] = tn;
            continue;
        }
        
        // Function/method
        std::smatch fm;
        if (std::regex_search(line, fm, std::regex(R"(\s*def\s+(\w+)\s*\(([^)]*)\))"))) {
            std::string funcName = fm[1].str();
            std::string params = fm[2].str();
            bool isMethod = !line.empty() && (line[0] == ' ' || line[0] == '\t');
            
            SymbolKind fKind = (isMethod && !currentClass.empty()) 
                               ? SymbolKind::Method : SymbolKind::Function;
            std::string qn = currentClass.empty() ? funcName 
                              : (currentClass + "." + funcName);
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, fKind, funcName, lineNum);
            sym.name = funcName;
            sym.qualifiedName = qn;
            sym.language = "python";
            sym.kind = fKind;
            sym.typeSignature = "def(" + params + ")";
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            
            if (isMethod && !currentClass.empty()) {
                sym.parentId = makeSymbolId(filePath, SymbolKind::Class, currentClass, 0);
            }
            
            // Check for decorators (look at previous lines)
            if (funcName == "__init__") sym.kind = SymbolKind::Constructor;
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Module-level variable
        std::smatch vm;
        if (std::regex_search(line, vm, std::regex(R"(^(\w+)\s*=\s*(.+))"))) {
            std::string varName = vm[1].str();
            if (varName[0] >= 'A' && varName[0] <= 'Z' && varName == 
                std::string(varName.size(), std::toupper(varName[0]))) {
                // ALL_CAPS = constant
                RichSymbol sym;
                sym.id = makeSymbolId(filePath, SymbolKind::Constant, varName, lineNum);
                sym.name = varName;
                sym.qualifiedName = varName;
                sym.language = "python";
                sym.kind = SymbolKind::Constant;
                sym.definition = {filePath, lineNum, 0, lineNum, 0};
                m_symbols[sym.id] = std::move(sym);
                fileSyms.push_back(m_symbols.rbegin()->first);
            }
        }
    }
}

void SemanticIndexEngine::analyzeJavaScript(const std::string& filePath,
                                              const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& fileSyms = m_fileSymbols[filePath];
    
    auto lines = std::istringstream(content);
    std::string line;
    int lineNum = 0;
    std::string currentClass;
    
    while (std::getline(lines, line)) {
        lineNum++;
        
        // Class
        std::smatch cm;
        if (std::regex_search(line, cm, std::regex(R"(class\s+(\w+)(?:\s+extends\s+(\w+))?)"))) {
            currentClass = cm[1].str();
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Class, currentClass, lineNum);
            sym.name = currentClass;
            sym.qualifiedName = currentClass;
            sym.language = filePath.find(".ts") != std::string::npos ? "typescript" : "javascript";
            sym.kind = SymbolKind::Class;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            
            if (cm.size() > 2 && cm[2].matched) {
                sym.baseTypes.push_back(cm[2].str());
            }
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Function declaration
        std::smatch fm;
        if (std::regex_search(line, fm,
                std::regex(R"((?:function|async\s+function)\s+(\w+)\s*\(([^)]*)\))"))) {
            std::string funcName = fm[1].str();
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Function, funcName, lineNum);
            sym.name = funcName;
            sym.qualifiedName = funcName;
            sym.language = filePath.find(".ts") != std::string::npos ? "typescript" : "javascript";
            sym.kind = SymbolKind::Function;
            sym.typeSignature = "function(" + fm[2].str() + ")";
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Arrow/const functions
        std::smatch am;
        if (std::regex_search(line, am,
                std::regex(R"((?:const|let|var)\s+(\w+)\s*=\s*(?:async\s+)?\()"))) {
            std::string funcName = am[1].str();
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Function, funcName, lineNum);
            sym.name = funcName;
            sym.qualifiedName = funcName;
            sym.language = filePath.find(".ts") != std::string::npos ? "typescript" : "javascript";
            sym.kind = SymbolKind::Function;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // TypeScript interface
        std::smatch im;
        if (std::regex_search(line, im, std::regex(R"(interface\s+(\w+))"))) {
            std::string ifaceName = im[1].str();
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Interface, ifaceName, lineNum);
            sym.name = ifaceName;
            sym.qualifiedName = ifaceName;
            sym.language = "typescript";
            sym.kind = SymbolKind::Interface;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // TypeScript type alias
        if (std::regex_search(line, im, std::regex(R"(type\s+(\w+)\s*=)"))) {
            std::string typeName = im[1].str();
            
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::TypeAlias, typeName, lineNum);
            sym.name = typeName;
            sym.qualifiedName = typeName;
            sym.language = "typescript";
            sym.kind = SymbolKind::TypeAlias;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
        }
    }
}

void SemanticIndexEngine::analyzeRust(const std::string& filePath,
                                        const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& fileSyms = m_fileSymbols[filePath];
    
    auto lines = std::istringstream(content);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(lines, line)) {
        lineNum++;
        
        // Struct
        std::smatch sm;
        if (std::regex_search(line, sm, std::regex(R"((?:pub\s+)?struct\s+(\w+))"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Struct, sm[1].str(), lineNum);
            sym.name = sm[1].str();
            sym.qualifiedName = sm[1].str();
            sym.language = "rust";
            sym.kind = SymbolKind::Struct;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            sym.visibility = line.find("pub") != std::string::npos 
                             ? SymbolVisibility::Public : SymbolVisibility::Private;
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Enum
        if (std::regex_search(line, sm, std::regex(R"((?:pub\s+)?enum\s+(\w+))"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Enum, sm[1].str(), lineNum);
            sym.name = sm[1].str();
            sym.qualifiedName = sm[1].str();
            sym.language = "rust";
            sym.kind = SymbolKind::Enum;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Trait
        if (std::regex_search(line, sm, std::regex(R"((?:pub\s+)?trait\s+(\w+))"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Trait, sm[1].str(), lineNum);
            sym.name = sm[1].str();
            sym.qualifiedName = sm[1].str();
            sym.language = "rust";
            sym.kind = SymbolKind::Trait;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Function
        if (std::regex_search(line, sm, 
                std::regex(R"((?:pub(?:\([^)]*\))?\s+)?(?:async\s+)?fn\s+(\w+)\s*(?:<[^>]*>)?\s*\(([^)]*)\)(?:\s*->\s*(.+?))?(?:\s*\{|;))"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Function, sm[1].str(), lineNum);
            sym.name = sm[1].str();
            sym.qualifiedName = sm[1].str();
            sym.language = "rust";
            sym.kind = SymbolKind::Function;
            sym.typeSignature = "fn(" + sm[2].str() + ")" + 
                                (sm.size() > 3 && sm[3].matched ? " -> " + sm[3].str() : "");
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            sym.visibility = line.find("pub") != std::string::npos 
                             ? SymbolVisibility::Public : SymbolVisibility::Private;
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
        }
    }
}

void SemanticIndexEngine::analyzeGo(const std::string& filePath,
                                      const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& fileSyms = m_fileSymbols[filePath];
    
    auto lines = std::istringstream(content);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(lines, line)) {
        lineNum++;
        
        // Package
        std::smatch pm;
        if (std::regex_search(line, pm, std::regex(R"(package\s+(\w+))"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Package, pm[1].str(), lineNum);
            sym.name = pm[1].str();
            sym.qualifiedName = pm[1].str();
            sym.language = "go";
            sym.kind = SymbolKind::Package;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Struct type
        std::smatch sm;
        if (std::regex_search(line, sm, std::regex(R"(type\s+(\w+)\s+struct)"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Struct, sm[1].str(), lineNum);
            sym.name = sm[1].str();
            sym.qualifiedName = sm[1].str();
            sym.language = "go";
            sym.kind = SymbolKind::Struct;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            sym.visibility = (sm[1].str()[0] >= 'A' && sm[1].str()[0] <= 'Z') 
                             ? SymbolVisibility::Public : SymbolVisibility::Private;
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Interface type
        if (std::regex_search(line, sm, std::regex(R"(type\s+(\w+)\s+interface)"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Interface, sm[1].str(), lineNum);
            sym.name = sm[1].str();
            sym.qualifiedName = sm[1].str();
            sym.language = "go";
            sym.kind = SymbolKind::Interface;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Function
        if (std::regex_search(line, sm, std::regex(R"(func\s+(?:\(\w+\s+\*?\w+\)\s+)?(\w+)\s*\()"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Function, sm[1].str(), lineNum);
            sym.name = sm[1].str();
            sym.qualifiedName = sm[1].str();
            sym.language = "go";
            sym.kind = SymbolKind::Function;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            sym.visibility = (sm[1].str()[0] >= 'A' && sm[1].str()[0] <= 'Z') 
                             ? SymbolVisibility::Public : SymbolVisibility::Private;
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
        }
    }
}

void SemanticIndexEngine::analyzeMasm(const std::string& filePath,
                                        const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& fileSyms = m_fileSymbols[filePath];
    
    auto lines = std::istringstream(content);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(lines, line)) {
        lineNum++;
        
        // Proc
        std::smatch pm;
        if (std::regex_search(line, pm, std::regex(R"((\w+)\s+proc\b)", std::regex::icase))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Function, pm[1].str(), lineNum);
            sym.name = pm[1].str();
            sym.qualifiedName = pm[1].str();
            sym.language = "masm";
            sym.kind = SymbolKind::Function;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            sym.visibility = SymbolVisibility::Public;
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Label
        if (std::regex_search(line, pm, std::regex(R"(^(\w+):)"))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Label, pm[1].str(), lineNum);
            sym.name = pm[1].str();
            sym.qualifiedName = pm[1].str();
            sym.language = "masm";
            sym.kind = SymbolKind::Label;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Macro
        if (std::regex_search(line, pm, std::regex(R"((\w+)\s+macro\b)", std::regex::icase))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Macro, pm[1].str(), lineNum);
            sym.name = pm[1].str();
            sym.qualifiedName = pm[1].str();
            sym.language = "masm";
            sym.kind = SymbolKind::Macro;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Struct
        if (std::regex_search(line, pm, std::regex(R"((\w+)\s+struct\b)", std::regex::icase))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Struct, pm[1].str(), lineNum);
            sym.name = pm[1].str();
            sym.qualifiedName = pm[1].str();
            sym.language = "masm";
            sym.kind = SymbolKind::Struct;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
            continue;
        }
        
        // Data definitions
        if (std::regex_search(line, pm, std::regex(R"((\w+)\s+(?:db|dw|dd|dq|dt|byte|word|dword|qword)\b)",
                                                    std::regex::icase))) {
            RichSymbol sym;
            sym.id = makeSymbolId(filePath, SymbolKind::Variable, pm[1].str(), lineNum);
            sym.name = pm[1].str();
            sym.qualifiedName = pm[1].str();
            sym.language = "masm";
            sym.kind = SymbolKind::Variable;
            sym.definition = {filePath, lineNum, 0, lineNum, 0};
            m_symbols[sym.id] = std::move(sym);
            fileSyms.push_back(m_symbols.rbegin()->first);
        }
    }
}

// ============================================================================
// Dependency Extraction
// ============================================================================
void SemanticIndexEngine::extractDependencies(const std::string& filePath,
                                                const std::string& content,
                                                const std::string& language) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto lines = std::istringstream(content);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(lines, line)) {
        lineNum++;
        
        std::smatch dm;
        
        if (language == "cpp" || language == "c") {
            // #include "file" or #include <file>
            if (std::regex_search(line, dm, std::regex(R"(#include\s+["<]([^">]+)[">])"))) {
                DependencyEdge dep;
                dep.sourceFile = filePath;
                dep.targetFile = dm[1].str();
                dep.importExpr = line;
                dep.line = lineNum;
                m_deps.insert({filePath, dep});
                m_reverseDeps.insert({dep.targetFile, dep});
            }
        } else if (language == "python") {
            // import X or from X import Y
            if (std::regex_search(line, dm, std::regex(R"((?:from\s+(\w[\w.]*)\s+)?import\s+(\w[\w.]*))"))) {
                DependencyEdge dep;
                dep.sourceFile = filePath;
                dep.targetFile = dm[1].matched ? dm[1].str() : dm[2].str();
                dep.importExpr = line;
                dep.line = lineNum;
                m_deps.insert({filePath, dep});
                m_reverseDeps.insert({dep.targetFile, dep});
            }
        } else if (language == "javascript" || language == "typescript") {
            // import ... from 'module' or require('module')
            if (std::regex_search(line, dm, std::regex(R"((?:import|from)\s+['"]([^'"]+)['"])"))) {
                DependencyEdge dep;
                dep.sourceFile = filePath;
                dep.targetFile = dm[1].str();
                dep.importExpr = line;
                dep.line = lineNum;
                m_deps.insert({filePath, dep});
                m_reverseDeps.insert({dep.targetFile, dep});
            } else if (std::regex_search(line, dm, std::regex(R"(require\s*\(\s*['"]([^'"]+)['"]\s*\))"))) {
                DependencyEdge dep;
                dep.sourceFile = filePath;
                dep.targetFile = dm[1].str();
                dep.importExpr = line;
                dep.line = lineNum;
                m_deps.insert({filePath, dep});
                m_reverseDeps.insert({dep.targetFile, dep});
            }
        } else if (language == "rust") {
            // use crate::module or use module;
            if (std::regex_search(line, dm, std::regex(R"(use\s+([\w:]+))"))) {
                DependencyEdge dep;
                dep.sourceFile = filePath;
                dep.targetFile = dm[1].str();
                dep.importExpr = line;
                dep.line = lineNum;
                m_deps.insert({filePath, dep});
            }
        } else if (language == "go") {
            // import "path"
            if (std::regex_search(line, dm, std::regex(R"re(import\s+"([^"]+)")re"))) {
                DependencyEdge dep;
                dep.sourceFile = filePath;
                dep.targetFile = dm[1].str();
                dep.importExpr = line;
                dep.line = lineNum;
                m_deps.insert({filePath, dep});
            }
        } else if (language == "masm" || language == "asm") {
            // include file.inc or includelib lib.lib
            if (std::regex_search(line, dm, std::regex(R"(include(?:lib)?\s+(\S+))",
                                                        std::regex::icase))) {
                DependencyEdge dep;
                dep.sourceFile = filePath;
                dep.targetFile = dm[1].str();
                dep.importExpr = line;
                dep.line = lineNum;
                m_deps.insert({filePath, dep});
            }
        }
    }
}

// ============================================================================
// Helpers
// ============================================================================
std::string SemanticIndexEngine::makeSymbolId(const std::string& file,
                                                SymbolKind kind,
                                                const std::string& name,
                                                int line) const {
    return file + "#" + std::to_string(static_cast<uint16_t>(kind)) + "#" + 
           name + "#" + std::to_string(line);
}

float SemanticIndexEngine::fuzzyScore(const std::string& pattern,
                                        const std::string& candidate) const {
    if (pattern.empty()) return 0.0f;
    if (candidate.empty()) return 0.0f;
    
    // Exact match
    if (pattern == candidate) return 1.0f;
    
    // Case-insensitive exact match
    std::string lp = pattern, lc = candidate;
    std::transform(lp.begin(), lp.end(), lp.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(lc.begin(), lc.end(), lc.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lp == lc) return 0.95f;
    
    // Prefix match
    if (lc.find(lp) == 0) return 0.9f;
    
    // Contains
    if (lc.find(lp) != std::string::npos) return 0.7f;
    
    // Subsequence match
    size_t pi = 0, ci = 0;
    int matched = 0;
    while (pi < lp.size() && ci < lc.size()) {
        if (lp[pi] == lc[ci]) {
            matched++;
            pi++;
        }
        ci++;
    }
    
    if (pi == lp.size()) {
        return 0.3f + 0.4f * (static_cast<float>(matched) / static_cast<float>(lc.size()));
    }
    
    return 0.0f;
}

} // namespace SemanticIndex
} // namespace RawrXD
