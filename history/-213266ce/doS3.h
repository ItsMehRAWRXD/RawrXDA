// ============================================================================
// codebase_indexer.h - Header for semantic codebase indexing and search
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

// ============================================================================
// CodeSymbol - Represents a symbol in the codebase
// ============================================================================

struct CodeSymbol {
    CodeSymbol(const std::string& name, const std::string& type, 
               const std::string& filePath, int lineNumber);
    
    std::string id;
    std::string name;
    std::string type;
    std::string filePath;
    int lineNumber;
};

// ============================================================================
// FileChangeType - Types of file system changes
// ============================================================================

enum class FileChangeType {
    MODIFIED,
    DELETED,
    CREATED
};

// ============================================================================
// EmbeddingStore - Vector database for semantic search
// ============================================================================

class EmbeddingStore {
public:
    EmbeddingStore(const std::string& dbPath);
    ~EmbeddingStore();
    
    void StoreSymbol(const CodeSymbol& symbol, const std::vector<float>& embedding);
    std::vector<CodeSymbol> SemanticSearch(const std::string& query, int maxResults = 10);
    
private:
    void InitializeDatabase();
    
    std::string dbPath;
    void* db; // sqlite3*
};

// ============================================================================
// IncrementalWatcher - File system change monitoring
// ============================================================================

class IncrementalWatcher {
public:
    IncrementalWatcher(const std::string& rootPath);
    ~IncrementalWatcher();
    
    void StartWatching();
    void StopWatching();
    
    using ChangeCallback = std::function<void(const std::string& path, FileChangeType type)>;
    void SetChangeCallback(ChangeCallback callback);
    
    bool ShouldIndexFile(const std::string& filePath);
    
private:
    void WatcherLoop();
    void ScanDirectory(const std::string& path, 
                      std::map<std::string, std::filesystem::file_time_type>& fileTimes);
    
    std::string rootPath;
    std::atomic<bool> isWatching;
    std::thread watcherThread;
    ChangeCallback changeCallback;
};

// ============================================================================
// CodebaseIndexer - Main indexing and search engine
// ============================================================================

class CodebaseIndexer {
public:
    CodebaseIndexer(const std::string& rootPath);
    
    void IndexRepository();
    std::vector<CodeSymbol> SemanticSearch(const std::string& query, int maxResults = 10);
    
private:
    void IndexDirectory(const std::string& path);
    void IndexFile(const std::string& filePath);
    std::vector<CodeSymbol> ParseSymbolsFromFile(const std::string& filePath, const std::string& content);
    std::vector<float> GenerateEmbedding(const std::string& text);
    void OnFileChanged(const std::string& path, FileChangeType type);
    
    std::string rootPath;
    EmbeddingStore embeddingStore;
    IncrementalWatcher fileWatcher;
};

// ============================================================================
// C Interface for MASM Integration
// ============================================================================

extern "C" {

// Forward declarations for C interface
struct CodebaseIndexer;
struct CodeSymbol;

// Creation/destruction
__declspec(dllexport) CodebaseIndexer* __stdcall CodebaseIndexer_Create(const char* rootPath);
__declspec(dllexport) void __stdcall CodebaseIndexer_Destroy(CodebaseIndexer* indexer);

// Core operations
__declspec(dllexport) void __stdcall CodebaseIndexer_IndexRepository(CodebaseIndexer* indexer);
__declspec(dllexport) CodeSymbol* __stdcall CodebaseIndexer_SemanticSearch(
    CodebaseIndexer* indexer, const char* query, int maxResults, int* resultCount);

} // extern "C"