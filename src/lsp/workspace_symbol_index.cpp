// workspace_symbol_index.cpp

#include "lsp/workspace_symbol_index.h"

#include <algorithm>
#include <regex>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>

namespace fs = std::filesystem;

namespace RawrXD::LSP {

static constexpr uint64_t FNV_OFFSET = 14695981039346656037ULL;
static constexpr uint64_t FNV_PRIME = 1099511628211ULL;

static uint64_t fnv1a(const std::string& s) {
    uint64_t hash = FNV_OFFSET;
    for (unsigned char c : s) {
        hash ^= c;
        hash *= FNV_PRIME;
    }
    return hash;
}

WorkspaceSymbolIndex::WorkspaceSymbolIndex()
    : m_totalLookups(0), m_totalLookupTimeMs(0.0), m_maxLookupTimeMs(0.0) {
}

WorkspaceSymbolIndex::~WorkspaceSymbolIndex() {
    std::unique_lock lock(m_mutex);
    m_symbols.clear();
    m_documents.clear();
    m_references.clear();
    m_documentSymbols.clear();
}

void WorkspaceSymbolIndex::addSymbol(const std::string& fqn, const SymbolInfo& info) {
    std::unique_lock lock(m_mutex);
    
    auto it = m_symbols.find(fqn);
    if (it != m_symbols.end()) {
        // Update existing symbol
        it->second = info;
        it->second.version++;
    } else {
        // Add new symbol
        SymbolInfo newInfo = info;
        newInfo.version = 1;
        m_symbols[fqn] = newInfo;
        
        // Track in document symbols
        if (!info.location.uri.empty()) {
            m_documentSymbols[info.location.uri].push_back(fqn);
        }
    }
}

void WorkspaceSymbolIndex::removeSymbol(const std::string& fqn) {
    std::unique_lock lock(m_mutex);
    
    auto it = m_symbols.find(fqn);
    if (it != m_symbols.end()) {
        // Remove from document tracking
        const auto& uri = it->second.location.uri;
        if (!uri.empty()) {
            auto& symbols = m_documentSymbols[uri];
            symbols.erase(
                std::remove(symbols.begin(), symbols.end(), fqn),
                symbols.end()
            );
        }
        
        // Remove from references
        m_references.erase(fqn);
        
        // Remove the symbol itself
        m_symbols.erase(it);
    }
}

std::optional<SymbolInfo> WorkspaceSymbolIndex::getSymbol(const std::string& fqn) const {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::shared_lock lock(m_mutex);
    
    auto it = m_symbols.find(fqn);
    if (it != m_symbols.end() && it->second.isValid()) {
        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = 
            std::chrono::duration<double, std::milli>(end - start).count();
        
        // Update metrics (lock-free increment acceptable here)
        const_cast<WorkspaceSymbolIndex*>(this)->m_totalLookups++;
        const_cast<WorkspaceSymbolIndex*>(this)->m_totalLookupTimeMs += elapsedMs;
        if (elapsedMs > const_cast<WorkspaceSymbolIndex*>(this)->m_maxLookupTimeMs) {
            const_cast<WorkspaceSymbolIndex*>(this)->m_maxLookupTimeMs = elapsedMs;
        }
        
        return it->second;
    }
    
    return std::nullopt;
}

uint32_t WorkspaceSymbolIndex::indexDocument(const std::string& uri, 
                                              const std::string& content) {
    std::unique_lock lock(m_mutex);
    
    // Clear existing symbols for this document
    if (m_documentSymbols.find(uri) != m_documentSymbols.end()) {
        auto& symbols = m_documentSymbols[uri];
        for (const auto& sym : symbols) {
            m_symbols.erase(sym);
        }
        symbols.clear();
    }
    
    // Parse new symbols
    std::vector<SymbolInfo> symbols;
    parseDocumentRegex(uri, content, symbols);
    
    // Add all symbols to index
    for (auto& sym : symbols) {
        sym.location.uri = uri;
        m_symbols[extractFqn(sym.name, sym.containerName)] = sym;
        m_documentSymbols[uri].push_back(extractFqn(sym.name, sym.containerName));
    }
    
    // Update document metadata
    DocumentMetadata meta;
    meta.uri = uri;
    meta.version++;
    meta.isDirty = false;
    meta.lastModifiedMs = 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    meta.symbolCount = symbols.size();
    for (const auto& sym : symbols) {
        meta.symbolNames.push_back(sym.name);
    }
    m_documents[uri] = meta;
    
    return static_cast<uint32_t>(symbols.size());
}

void WorkspaceSymbolIndex::clearDocument(const std::string& uri) {
    std::unique_lock lock(m_mutex);
    
    auto it = m_documentSymbols.find(uri);
    if (it != m_documentSymbols.end()) {
        for (const auto& fqn : it->second) {
            m_symbols.erase(fqn);
            m_references.erase(fqn);
        }
        m_documentSymbols.erase(it);
    }
    
    m_documents.erase(uri);
}

void WorkspaceSymbolIndex::addReference(const std::string& referencedSymbol,
                                         const SymbolReference& ref) {
    std::unique_lock lock(m_mutex);
    m_references[referencedSymbol].push_back(ref);
}

std::vector<SymbolReference> WorkspaceSymbolIndex::getReferences(
    const std::string& fqn) const {
    std::shared_lock lock(m_mutex);
    
    auto it = m_references.find(fqn);
    if (it != m_references.end()) {
        return it->second;
    }
    return {};
}

void WorkspaceSymbolIndex::updateReferencesForRename(const std::string& oldFqn,
                                                     const std::string& newFqn) {
    std::unique_lock lock(m_mutex);
    
    auto it = m_references.find(oldFqn);
    if (it != m_references.end()) {
        // Move references to new name
        m_references[newFqn] = it->second;
        m_references.erase(it);
        
        // Update symbol definition
        auto symIt = m_symbols.find(oldFqn);
        if (symIt != m_symbols.end()) {
            SymbolInfo info = symIt->second;
            m_symbols.erase(symIt);
            m_symbols[newFqn] = info;
            m_symbols[newFqn].version++;
        }
    }
}

std::vector<SymbolInfo> WorkspaceSymbolIndex::findByPrefix(
    const std::string& prefix, size_t maxResults) const {
    std::shared_lock lock(m_mutex);
    
    std::vector<SymbolInfo> results;
    results.reserve(maxResults);
    
    for (const auto& [fqn, info] : m_symbols) {
        if (results.size() >= maxResults) break;
        
        if (info.name.find(prefix) == 0 && info.isValid()) {
            results.push_back(info);
        }
    }
    
    return results;
}

std::vector<SymbolInfo> WorkspaceSymbolIndex::findByKind(
    SymbolKind kind, size_t maxResults) const {
    std::shared_lock lock(m_mutex);
    
    std::vector<SymbolInfo> results;
    results.reserve(maxResults);
    
    for (const auto& [fqn, info] : m_symbols) {
        if (results.size() >= maxResults) break;
        
        if (info.kind == kind && info.isValid()) {
            results.push_back(info);
        }
    }
    
    return results;
}

std::vector<SymbolInfo> WorkspaceSymbolIndex::findInContainer(
    const std::string& containerName) const {
    std::shared_lock lock(m_mutex);
    
    std::vector<SymbolInfo> results;
    
    for (const auto& [fqn, info] : m_symbols) {
        if (info.containerName == containerName && info.isValid()) {
            results.push_back(info);
        }
    }
    
    return results;
}

void WorkspaceSymbolIndex::documentOpened(const std::string& uri,
                                           const std::string& content) {
    indexDocument(uri, content);
}

void WorkspaceSymbolIndex::documentChanged(const std::string& uri,
                                            const std::string& content) {
    std::unique_lock lock(m_mutex);
    
    auto it = m_documents.find(uri);
    if (it != m_documents.end()) {
        it->second.isDirty = true;
    }
    
    lock.unlock();
    indexDocument(uri, content);
}

void WorkspaceSymbolIndex::documentClosed(const std::string& uri) {
    clearDocument(uri);
}

std::optional<DocumentMetadata> WorkspaceSymbolIndex::getDocumentMetadata(
    const std::string& uri) const {
    std::shared_lock lock(m_mutex);
    
    auto it = m_documents.find(uri);
    if (it != m_documents.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

WorkspaceSymbolIndex::WorkspaceStats WorkspaceSymbolIndex::getStats() const {
    std::shared_lock lock(m_mutex);
    
    WorkspaceStats stats;
    stats.totalSymbols = m_symbols.size();
    stats.totalDocuments = m_documents.size();
    
    stats.totalReferences = 0;
    for (const auto& [fqn, refs] : m_references) {
        stats.totalReferences += refs.size();
    }
    
    stats.averageSymbolsPerDocument = 0.0f;
    if (stats.totalDocuments > 0) {
        stats.averageSymbolsPerDocument = 
            static_cast<float>(stats.totalSymbols) / stats.totalDocuments;
    }
    
    stats.cacheSize = sizeof(SymbolInfo) * stats.totalSymbols;
    stats.indexingTimeMs = 0;  // Will be populated by BatchIndexer
    
    return stats;
}

void WorkspaceSymbolIndex::invalidateStale(uint32_t maxVersionAge) {
    std::unique_lock lock(m_mutex);
    
    std::vector<std::string> toRemove;
    
    for (auto& [fqn, info] : m_symbols) {
        if (info.version >= maxVersionAge) {
            toRemove.push_back(fqn);
        }
    }
    
    for (const auto& fqn : toRemove) {
        removeSymbol(fqn);
    }
}

bool WorkspaceSymbolIndex::verifyIndexIntegrity() const {
    std::shared_lock lock(m_mutex);
    
    // Check document symbols match actual symbols
    for (const auto& [uri, symbols] : m_documentSymbols) {
        for (const auto& fqn : symbols) {
            if (m_symbols.find(fqn) == m_symbols.end()) {
                return false;  // Orphaned symbol reference
            }
            const auto& sym = m_symbols.at(fqn);
            if (sym.location.uri != uri) {
                return false;  // Symbol location mismatch
            }
        }
    }
    
    // Check all symbols are in documentSymbols
    for (const auto& [fqn, info] : m_symbols) {
        if (!info.location.uri.empty()) {
            auto& symbols = m_documentSymbols.at(info.location.uri);
            if (std::find(symbols.begin(), symbols.end(), fqn) == symbols.end()) {
                return false;  // Symbol not tracked in document
            }
        }
    }
    
    return true;
}

WorkspaceSymbolIndex::PerformanceMetrics WorkspaceSymbolIndex::getPerformanceMetrics() const {
    std::shared_lock lock(m_mutex);
    
    PerformanceMetrics metrics;
    metrics.totalLookups = m_totalLookups;
    
    if (m_totalLookups > 0) {
        metrics.avgLookupTimeMs = m_totalLookupTimeMs / m_totalLookups;
        metrics.p99LookupTimeMs = m_maxLookupTimeMs;
    } else {
        metrics.avgLookupTimeMs = 0.0;
        metrics.p99LookupTimeMs = 0.0;
    }
    
    return metrics;
}

void WorkspaceSymbolIndex::clearPerformanceMetrics() {
    std::unique_lock lock(m_mutex);
    m_totalLookups = 0;
    m_totalLookupTimeMs = 0.0;
    m_maxLookupTimeMs = 0.0;
}

std::string WorkspaceSymbolIndex::extractFqn(const std::string& name,
                                              const std::string& container) const {
    if (container.empty()) {
        return name;
    }
    return container + "::" + name;
}

void WorkspaceSymbolIndex::parseDocumentRegex(const std::string& uri,
                                               const std::string& content,
                                               std::vector<SymbolInfo>& symbols) {
    // TypeScript/JavaScript class pattern
    std::regex classPattern(R"((?:export\s+)?class\s+(\w+)(?:\s*extends\s+(\w+))?)");
    
    // Function pattern
    std::regex funcPattern(R"((?:export\s+)?(?:async\s+)?function\s+(\w+))");
    
    // Variable/const pattern
    std::regex varPattern(R"((?:(?:const|let|var)\s+(\w+)))");
    
    // Interface pattern
    std::regex ifacePattern(R"((?:export\s+)?interface\s+(\w+))");
    
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    
    // Count line numbers
    uint32_t lineNumber = 0;
    std::istringstream iss(content);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Search for classes
        if (std::regex_search(line, match, classPattern)) {
            SymbolInfo info;
            info.name = match[1].str();
            info.kind = SymbolKind::Class;
            info.location.line = lineNumber;
            info.location.character = match.position(0);
            info.location.endLine = lineNumber;
            info.location.endCharacter = match.position(0) + match[0].length();
            symbols.push_back(info);
        }
        
        // Search for functions
        if (std::regex_search(line, match, funcPattern)) {
            SymbolInfo info;
            info.name = match[1].str();
            info.kind = SymbolKind::Function;
            info.location.line = lineNumber;
            info.location.character = match.position(0);
            info.location.endLine = lineNumber;
            info.location.endCharacter = match.position(0) + match[0].length();
            symbols.push_back(info);
        }
        
        // Search for interfaces
        if (std::regex_search(line, match, ifacePattern)) {
            SymbolInfo info;
            info.name = match[1].str();
            info.kind = SymbolKind::Interface;
            info.location.line = lineNumber;
            info.location.character = match.position(0);
            info.location.endLine = lineNumber;
            info.location.endCharacter = match.position(0) + match[0].length();
            symbols.push_back(info);
        }
        
        lineNumber++;
    }
}

SymbolCache::SymbolCache(size_t maxSize)
    : m_maxSize(maxSize), m_hits(0), m_misses(0), m_evictions(0) {
}

SymbolCache::~SymbolCache() {
    std::unique_lock lock(m_mutex);
    m_cache.clear();
}

std::optional<SymbolInfo> SymbolCache::get(const std::string& fqn) const {
    std::unique_lock lock(m_mutex);
    
    auto it = m_cache.find(fqn);
    if (it != m_cache.end()) {
        // Update access time and count
        it->second.accessTimeMs = 
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
        it->second.accessCount++;
        const_cast<SymbolCache*>(this)->m_hits++;
        return it->second.info;
    }
    
    const_cast<SymbolCache*>(this)->m_misses++;
    return std::nullopt;
}

void SymbolCache::put(const std::string& fqn, const SymbolInfo& info) {
    std::unique_lock lock(m_mutex);
    
    if (m_cache.size() >= m_maxSize) {
        evictLRU();
    }
    
    CacheEntry entry;
    entry.info = info;
    entry.accessTimeMs = 
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    entry.accessCount = 1;
    
    m_cache[fqn] = entry;
}

void SymbolCache::invalidate(const std::string& fqn) {
    std::unique_lock lock(m_mutex);
    m_cache.erase(fqn);
}

void SymbolCache::clear() {
    std::unique_lock lock(m_mutex);
    m_cache.clear();
    m_hits = 0;
    m_misses = 0;
    m_evictions = 0;
}

SymbolCache::CacheStats SymbolCache::getStats() const {
    std::shared_lock lock(m_mutex);
    
    CacheStats stats;
    stats.hits = m_hits;
    stats.misses = m_misses;
    stats.evictions = m_evictions;
    stats.hitRate = (m_hits + m_misses) > 0 ?
                    (float)m_hits / (m_hits + m_misses) : 0.0f;
    
    return stats;
}

void SymbolCache::evictLRU() {
    if (m_cache.empty()) return;
    
    auto lruIt = m_cache.begin();
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it) {
        if (it->second.accessTimeMs < lruIt->second.accessTimeMs) {
            lruIt = it;
        }
    }
    
    m_cache.erase(lruIt);
    m_evictions++;
}

DocumentLifecycleManager::DocumentLifecycleManager(WorkspaceSymbolIndex* index)
    : m_index(index) {
}

DocumentLifecycleManager::~DocumentLifecycleManager() {
}

void DocumentLifecycleManager::onDocumentOpen(const std::string& uri,
                                              const std::string& content) {
    std::unique_lock lock(m_mutex);
    m_trackedDocuments.insert(uri);
    lock.unlock();
    
    if (m_index) {
        m_index->documentOpened(uri, content);
    }
}

void DocumentLifecycleManager::onDocumentChange(const std::string& uri,
                                                const std::string& newContent) {
    if (m_index) {
        m_index->documentChanged(uri, newContent);
    }
}

void DocumentLifecycleManager::onDocumentClose(const std::string& uri) {
    std::unique_lock lock(m_mutex);
    m_trackedDocuments.erase(uri);
    lock.unlock();
    
    if (m_index) {
        m_index->documentClosed(uri);
    }
}

void DocumentLifecycleManager::onDocumentSave(const std::string& uri,
                                              const std::string& content) {
    if (m_index) {
        m_index->documentChanged(uri, content);
    }
}

void DocumentLifecycleManager::processBatch(
    const std::vector<std::pair<std::string, std::string>>& documents) {
    std::unique_lock lock(m_mutex);
    
    for (const auto& [uri, content] : documents) {
        m_trackedDocuments.insert(uri);
    }
    
    lock.unlock();
    
    if (m_index) {
        for (const auto& [uri, content] : documents) {
            m_index->documentOpened(uri, content);
        }
    }
}

bool DocumentLifecycleManager::isDocumentTracked(const std::string& uri) const {
    std::shared_lock lock(m_mutex);
    return m_trackedDocuments.find(uri) != m_trackedDocuments.end();
}

std::vector<std::string> DocumentLifecycleManager::getTrackedDocuments() const {
    std::shared_lock lock(m_mutex);
    
    std::vector<std::string> result;
    result.reserve(m_trackedDocuments.size());
    for (const auto& uri : m_trackedDocuments) {
        result.push_back(uri);
    }
    
    return result;
}

bool DocumentLifecycleManager::verifyConsistency() const {
    if (!m_index) return false;
    return m_index->verifyIndexIntegrity();
}

BatchIndexer::BatchIndexer(WorkspaceSymbolIndex* index)
    : m_index(index), m_isIndexing(false),
      m_documentsProcessed(0), m_documentsTotal(0), m_startTimeMs(0) {
}

BatchIndexer::~BatchIndexer() {
    cancelIndexing();
}

BatchIndexer::BatchResult BatchIndexer::indexDirectory(const std::string& path,
                                                        bool async) {
    std::unique_lock lock(m_mutex);
    
    if (m_isIndexing) {
        return {0, 0, 0, false, "Indexing already in progress"};
    }
    
    if (!m_index) {
        return {0, 0, 0, false, "Index not initialized"};
    }
    
    m_isIndexing = true;
    m_startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    lock.unlock();
    
    BatchResult result{0, 0, 0, false, ""};
    
    try {
        // Collect all source files to index
        std::vector<std::pair<std::string, std::string>> files;
        
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (!entry.is_regular_file()) continue;
            
            const auto& p = entry.path();
            std::string ext = p.extension().string();
            
            // Support common language files
            if (ext == ".ts" || ext == ".js" || ext == ".tsx" || ext == ".jsx" ||
                ext == ".cpp" || ext == ".h" || ext == ".py" || ext == ".go") {
                
                try {
                    std::ifstream file(p, std::ios::binary);
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                    files.push_back({p.string(), content});
                } catch (...) {
                    // Skip files that can't be read
                    continue;
                }
            }
        }
        
        m_documentsTotal = files.size();
        m_documentsProcessed = 0;
        
        size_t totalSymbols = 0;
        
        for (const auto& [uri, content] : files) {
            uint32_t symbols = m_index->indexDocument(uri, content);
            totalSymbols += symbols;
            m_documentsProcessed++;
        }
        
        auto endTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        result.documentsIndexed = m_documentsProcessed;
        result.symbolsIndexed = totalSymbols;
        result.elapsedMs = endTimeMs - m_startTimeMs;
        result.success = true;
        
    } catch (const std::exception& ex) {
        result.error = std::string("Exception: ") + ex.what();
    }
    
    m_isIndexing = false;
    return result;
}

BatchIndexer::ProgressInfo BatchIndexer::getProgress() const {
    std::shared_lock lock(m_mutex);
    
    ProgressInfo info;
    info.documentsProcessed = m_documentsProcessed;
    info.documentsTotal = m_documentsTotal;
    info.elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() - m_startTimeMs;
    
    if (m_documentsTotal > 0) {
        info.percentComplete = 
            (float)m_documentsProcessed / m_documentsTotal * 100.0f;
        
        if (m_documentsProcessed > 0 && info.elapsedMs > 0) {
            float docsPerMs = (float)m_documentsProcessed / info.elapsedMs;
            size_t remaining = m_documentsTotal - m_documentsProcessed;
            info.estimatedRemainingMs = (int64_t)(remaining / docsPerMs);
        }
    } else {
        info.percentComplete = 0.0f;
        info.estimatedRemainingMs = 0;
    }
    
    return info;
}

bool BatchIndexer::isIndexing() const {
    std::shared_lock lock(m_mutex);
    return m_isIndexing;
}

void BatchIndexer::cancelIndexing() {
    std::unique_lock lock(m_mutex);
    m_isIndexing = false;
}

} // namespace RawrXD::LSP
