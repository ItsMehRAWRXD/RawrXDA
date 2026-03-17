#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <chrono>
#include <functional>
#include <filesystem>
#include <cstdint>

namespace RawrXD {
namespace Compiler {

// ============================================================================
// INCREMENTAL COMPILATION CACHE SYSTEM
// ============================================================================

/**
 * @class CompilationCache
 * @brief Manages incremental compilation by caching compilation results
 * 
 * Features:
 * - File-level change detection (MD5/SHA256)
 * - Dependency graph tracking
 * - Incremental recompilation
 * - Cache invalidation
 * - Persistent cache storage
 */
class CompilationCache {
public:
    struct CacheEntry {
        std::string file_path;
        std::string file_hash;
        std::int64_t timestamp;
        std::vector<std::string> dependencies;
        std::string output_hash;
        std::chrono::milliseconds compile_time;
        int optimization_level;
    };

    struct DependencyNode {
        std::string file;
        std::set<std::string> depends_on;
        std::set<std::string> depended_by;
        bool needs_recompile = true;
    };

    CompilationCache(const std::string& cache_dir = ".rawrxd_cache");
    ~CompilationCache();

    // Cache operations
    bool isCached(const std::string& file_path) const;
    bool isOutOfDate(const std::string& file_path) const;
    const CacheEntry* getCacheEntry(const std::string& file_path) const;
    
    // Store compilation result
    void cacheResult(const CacheEntry& entry);
    void invalidateFile(const std::string& file_path);
    void invalidateDependents(const std::string& file_path);
    
    // Dependency management
    void addDependency(const std::string& source, const std::string& dependency);
    void buildDependencyGraph(const std::vector<std::string>& source_files);
    std::vector<std::string> getFilesToRecompile(const std::vector<std::string>& changed_files);
    
    // Cache statistics
    struct CacheStats {
        size_t total_entries;
        size_t cache_size_bytes;
        int hit_count;
        int miss_count;
        double hit_rate;
    };
    CacheStats getStatistics() const;
    
    // Persistence
    bool saveCacheToDisk();
    bool loadCacheFromDisk();
    void clearCache();

private:
    std::string cache_directory_;
    std::map<std::string, CacheEntry> cache_entries_;
    std::map<std::string, DependencyNode> dependency_graph_;
    mutable int cache_hits_ = 0;
    mutable int cache_misses_ = 0;
    
    std::string computeFileHash(const std::string& file_path);
    void updateDependencyGraph(const std::string& file, const std::vector<std::string>& deps);
};

/**
 * @class IncrementalCompiler
 * @brief Handles incremental compilation with smart recompilation
 * 
 * Features:
 * - Detect changed files
 * - Recompile only affected files
 * - Maintain linking cache
 * - Incremental linking
 */
class IncrementalCompiler {
public:
    struct CompilationResult {
        bool success;
        std::vector<std::string> recompiled_files;
        std::vector<std::string> unchanged_files;
        std::chrono::milliseconds total_time;
        std::chrono::milliseconds time_saved;
        double incremental_ratio; // time_saved / original_time
    };

    IncrementalCompiler();
    
    // Configure
    void enableIncrementalMode(bool enabled);
    void setMaxCacheAge(std::chrono::hours max_age);
    void setIgnorePatterns(const std::vector<std::string>& patterns);
    
    // Incremental compilation
    CompilationResult compileIncremental(
        const std::vector<std::string>& source_files,
        const std::string& output_file,
        const std::function<bool(const std::vector<std::string>&, const std::string&)>& compiler_func
    );
    
    // Manual control
    void forceRecompile(const std::string& file_path);
    void forceRecompileAll();
    
    // Cache operations
    const CompilationCache& getCache() const { return cache_; }
    void clearCache() { cache_.clearCache(); }
    
    // Statistics
    struct IncrementalStats {
        int total_compilations;
        int incremental_compilations;
        int full_compilations;
        std::chrono::milliseconds total_time_saved;
        double average_speedup;
    };
    IncrementalStats getStatistics() const;

private:
    CompilationCache cache_;
    bool incremental_enabled_ = true;
    std::chrono::hours max_cache_age_{24};
    std::vector<std::string> ignore_patterns_;
    
    IncrementalStats stats_;
    
    bool shouldIgnoreFile(const std::string& file_path) const;
    std::vector<std::string> detectChangedFiles(const std::vector<std::string>& source_files);
};

/**
 * @class IncrementalLinkingManager
 * @brief Manages incremental linking by caching object files
 * 
 * Features:
 * - Object file caching
 * - Incremental relinking
 * - Symbol table management
 * - Link-time optimization hints
 */
class IncrementalLinkingManager {
public:
    struct LinkingCache {
        std::string object_file;
        std::string object_hash;
        std::vector<std::string> symbols;
        std::int64_t timestamp;
    };

    IncrementalLinkingManager();
    
    // Object file management
    void cacheObjectFile(const std::string& source, const std::string& object_file);
    bool isObjectFileCached(const std::string& source) const;
    std::string getCachedObjectFile(const std::string& source) const;
    
    // Symbol tracking
    void extractSymbols(const std::string& object_file);
    std::vector<std::string> getSymbols(const std::string& object_file) const;
    
    // Incremental linking
    bool performIncrementalLink(
        const std::vector<std::string>& object_files,
        const std::string& output_file
    );
    
    // Cache validation
    bool validateObjectFiles(const std::vector<std::string>& objects);
    void invalidateObjectFile(const std::string& object_file);

private:
    std::map<std::string, LinkingCache> object_cache_;
    std::map<std::string, std::vector<std::string>> symbol_table_;
};

} // namespace Compiler
} // namespace RawrXD
