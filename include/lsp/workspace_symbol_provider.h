#pragma once
/**
 * @file workspace_symbol_provider.h
 * @brief Workspace-wide symbol search and indexing
 * Batch 3 - Item 36: Workspace symbol provider
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <future>

namespace RawrXD::LSP {

enum class SymbolKind {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26
};

struct SymbolLocation {
    std::string uri;
    uint32_t startLine;
    uint32_t startColumn;
    uint32_t endLine;
    uint32_t endColumn;
};

struct WorkspaceSymbol {
    std::string name;
    SymbolKind kind;
    std::string containerName;
    SymbolLocation location;
    std::string detail;
    float score;
};

struct SymbolReference {
    std::string uri;
    uint32_t line;
    uint32_t column;
    uint32_t length;
    bool isWrite;
};

class SymbolIndex {
public:
    SymbolIndex();
    ~SymbolIndex();

    // Indexing
    void addSymbol(const WorkspaceSymbol& symbol);
    void removeSymbol(const std::string& uri, const std::string& name);
    void clearFile(const std::string& uri);
    void clearAll();

    // Search
    std::vector<WorkspaceSymbol> query(const std::string& pattern, size_t limit = 100);
    std::vector<WorkspaceSymbol> queryExact(const std::string& name);
    std::vector<WorkspaceSymbol> queryByKind(SymbolKind kind);
    std::vector<WorkspaceSymbol> queryInContainer(const std::string& containerName);

    // References
    void addReference(const std::string& symbolName, const SymbolReference& ref);
    std::vector<SymbolReference> getReferences(const std::string& symbolName);

    // Status
    size_t getSymbolCount() const;
    size_t getFileCount() const;

private:
    struct SymbolData {
        WorkspaceSymbol symbol;
        std::vector<SymbolReference> references;
    };

    mutable std::mutex m_mutex;
    std::map<std::string, std::map<std::string, SymbolData>> m_symbols; // uri -> name -> data
    std::map<SymbolKind, std::vector<std::string>> m_byKind;
    std::map<std::string, std::vector<std::string>> m_byContainer;

    float calculateScore(const std::string& pattern, const WorkspaceSymbol& symbol);
};

class WorkspaceSymbolProvider {
public:
    WorkspaceSymbolProvider();
    ~WorkspaceSymbolProvider();

    // Lifecycle
    void initialize(const std::string& workspaceRoot);
    void shutdown();

    // Indexing
    void indexFile(const std::string& uri, const std::string& content);
    void indexFiles(const std::vector<std::string>& uris);
    void reindexFile(const std::string& uri);
    void removeFile(const std::string& uri);

    // Async indexing
    std::future<void> indexWorkspaceAsync();
    void pauseIndexing();
    void resumeIndexing();
    bool isIndexing() const;
    float getIndexingProgress() const;

    // Queries
    std::vector<WorkspaceSymbol> symbol(const std::string& query, size_t limit = 100);
    std::optional<WorkspaceSymbol> resolveSymbol(const std::string& uri, const std::string& name);

    // References
    std::vector<SymbolReference> references(const std::string& uri,
                                               uint32_t line,
                                               uint32_t column,
                                               bool includeDeclaration = true);

    // Navigation
    std::optional<WorkspaceSymbol> getSymbolAtPosition(const std::string& uri,
                                                        uint32_t line,
                                                        uint32_t column);
    std::vector<WorkspaceSymbol> getSymbolsInDocument(const std::string& uri);

    // Events
    using IndexProgressCallback = std::function<void(const std::string& file, size_t current, size_t total)>;
    void onIndexProgress(IndexProgressCallback callback);

private:
    std::unique_ptr<SymbolIndex> m_index;
    std::string m_workspaceRoot;
    std::atomic<bool> m_indexing{false};
    std::atomic<bool> m_paused{false};
    std::atomic<float> m_progress{0.0f};
    IndexProgressCallback m_progressCallback;
    mutable std::mutex m_mutex;

    void doIndexWorkspace();
    std::vector<WorkspaceSymbol> parseSymbols(const std::string& uri, const std::string& content);
};

// Global provider
WorkspaceSymbolProvider& getWorkspaceSymbolProvider();

// Utility
std::string symbolKindToString(SymbolKind kind);
SymbolKind stringToSymbolKind(const std::string& str);

} // namespace RawrXD::LSP
