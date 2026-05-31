// workspace_symbol_index.h — in-process workspace symbol table (FQN map, refs, regex ingest).

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <cstdint>

namespace RawrXD::LSP {

struct Location {
    std::string uri;
    uint32_t line;
    uint32_t character;
    uint32_t endLine;
    uint32_t endCharacter;

    bool isValid() const {
        return !uri.empty() && endLine >= line;
    }
};

enum class SymbolKind : uint8_t {
    Variable = 1,
    Function = 2,
    Class = 3,
    Interface = 4,
    Struct = 5,
    Union = 6,
    Enum = 7,
    EnumMember = 8,
    Method = 9,
    Property = 10,
    Constructor = 11,
    Operator = 12,
    Null = 0,
};

struct SymbolInfo {
    std::string name;
    std::string containerName;
    Location location;
    SymbolKind kind;
    uint8_t reserved[7];
    uint32_t scopeDepth;
    uint32_t version;
    
    SymbolInfo() : kind(SymbolKind::Null), scopeDepth(0), version(0) {}
    
    bool isValid() const {
        return kind != SymbolKind::Null && location.isValid();
    }
};

struct SymbolReference {
    std::string referencedSymbol;
    std::string referencingFile;
    Location location;
    bool isDefinition;
    
    SymbolReference() : isDefinition(false) {}
};

// ---------------------------------------------------------------------------
// Document Metadata for Lifecycle Tracking
// ---------------------------------------------------------------------------

struct DocumentMetadata {
    std::string uri;
    uint32_t version;
    int64_t lastModifiedMs;
    bool isDirty;
    size_t symbolCount;
    std::vector<std::string> symbolNames;
    
    DocumentMetadata() : version(0), lastModifiedMs(0), isDirty(false), symbolCount(0) {}
};

class WorkspaceSymbolIndex {
public:
    WorkspaceSymbolIndex();
    ~WorkspaceSymbolIndex();

    void addSymbol(const std::string& fqn, const SymbolInfo& info);
    void removeSymbol(const std::string& fqn);
    std::optional<SymbolInfo> getSymbol(const std::string& fqn) const;

    uint32_t indexDocument(const std::string& uri, const std::string& content);
    void clearDocument(const std::string& uri);

    void addReference(const std::string& referencedSymbol, const SymbolReference& ref);
    std::vector<SymbolReference> getReferences(const std::string& fqn) const;
    void updateReferencesForRename(const std::string& oldFqn, const std::string& newFqn);

    std::vector<SymbolInfo> findByPrefix(const std::string& prefix, size_t maxResults = 100) const;
    std::vector<SymbolInfo> findByKind(SymbolKind kind, size_t maxResults = 1000) const;
    std::vector<SymbolInfo> findInContainer(const std::string& containerName) const;

    void documentOpened(const std::string& uri, const std::string& content);
    void documentChanged(const std::string& uri, const std::string& content);
    void documentClosed(const std::string& uri);
    std::optional<DocumentMetadata> getDocumentMetadata(const std::string& uri) const;

    struct WorkspaceStats {
        size_t totalSymbols;
        size_t totalDocuments;
        size_t totalReferences;
        size_t cacheSize;
        int64_t indexingTimeMs;
        float averageSymbolsPerDocument;
    };
    
    WorkspaceStats getStats() const;
    void invalidateStale(uint32_t maxVersionAge);
    bool verifyIndexIntegrity() const;

    struct PerformanceMetrics {
        double avgLookupTimeMs;
        double p99LookupTimeMs;
        size_t totalLookups;
    };
    
    PerformanceMetrics getPerformanceMetrics() const;
    void clearPerformanceMetrics();

private:
    // === Internal Storage ===
    
    // Main symbol storage: FQN -> SymbolInfo
    std::unordered_map<std::string, SymbolInfo> m_symbols;
    
    // Document tracking
    std::unordered_map<std::string, DocumentMetadata> m_documents;
    
    // Cross-file references: fqn -> vector of references
    std::unordered_map<std::string, std::vector<SymbolReference>> m_references;
    
    // Symbol names by document (for efficient document deletion)
    std::unordered_map<std::string, std::vector<std::string>> m_documentSymbols;
    
    // === Synchronization ===
    mutable std::shared_mutex m_mutex;
    
    // === Metrics ===
    mutable size_t m_totalLookups = 0;
    mutable double m_totalLookupTimeMs = 0.0;
    mutable double m_maxLookupTimeMs = 0.0;
    
    // === Helper Methods ===
    
    std::string extractFqn(const std::string& name, const std::string& container) const;
    void parseDocumentRegex(const std::string& uri, const std::string& content,
                            std::vector<SymbolInfo>& symbols);
};

class SymbolCache {
public:
    explicit SymbolCache(size_t maxSize = 10000);
    ~SymbolCache();

    std::optional<SymbolInfo> get(const std::string& fqn) const;
    void put(const std::string& fqn, const SymbolInfo& info);
    void invalidate(const std::string& fqn);
    void clear();
    
    // Statistics
    struct CacheStats {
        size_t hits;
        size_t misses;
        size_t evictions;
        float hitRate;
    };
    
    CacheStats getStats() const;

private:
    struct CacheEntry {
        SymbolInfo info;
        int64_t accessTimeMs;
        uint32_t accessCount;
    };
    
    mutable std::shared_mutex m_mutex;
    std::unordered_map<std::string, CacheEntry> m_cache;
    size_t m_maxSize;
    
    mutable size_t m_hits = 0;
    mutable size_t m_misses = 0;
    mutable size_t m_evictions = 0;
    
    void evictLRU();
};

class DocumentLifecycleManager {
public:
    DocumentLifecycleManager(WorkspaceSymbolIndex* index);
    ~DocumentLifecycleManager();

    void onDocumentOpen(const std::string& uri, const std::string& content);
    void onDocumentChange(const std::string& uri, const std::string& newContent);
    void onDocumentClose(const std::string& uri);
    void onDocumentSave(const std::string& uri, const std::string& content);
    void processBatch(const std::vector<std::pair<std::string, std::string>>& documents);
    bool isDocumentTracked(const std::string& uri) const;
    std::vector<std::string> getTrackedDocuments() const;
    bool verifyConsistency() const;

private:
    WorkspaceSymbolIndex* m_index;
    mutable std::shared_mutex m_mutex;
    std::unordered_set<std::string> m_trackedDocuments;
};

class BatchIndexer {
public:
    explicit BatchIndexer(WorkspaceSymbolIndex* index);
    ~BatchIndexer();

    struct BatchResult {
        size_t documentsIndexed;
        size_t symbolsIndexed;
        int64_t elapsedMs;
        bool success;
        std::string error;
    };
    
    BatchResult indexDirectory(const std::string& path, bool async = false);

    struct ProgressInfo {
        size_t documentsProcessed;
        size_t documentsTotal;
        float percentComplete;
        int64_t elapsedMs;
        int64_t estimatedRemainingMs;
    };
    
    ProgressInfo getProgress() const;
    bool isIndexing() const;
    void cancelIndexing();

private:
    WorkspaceSymbolIndex* m_index;
    mutable std::shared_mutex m_mutex;
    
    bool m_isIndexing = false;
    size_t m_documentsProcessed = 0;
    size_t m_documentsTotal = 0;
    int64_t m_startTimeMs = 0;
};

} // namespace RawrXD::LSP
