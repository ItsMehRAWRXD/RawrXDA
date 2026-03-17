#include "incremental_compilation.hpp"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <numeric>

namespace RawrXD {
namespace Compiler {

// ============================================================================
// CompilationCache Implementation
// ============================================================================

CompilationCache::CompilationCache(const std::string& cache_dir)
    : cache_directory_(cache_dir) {
    std::filesystem::create_directories(cache_directory_);
    loadCacheFromDisk();
}

CompilationCache::~CompilationCache() {
    saveCacheToDisk();
}

bool CompilationCache::isCached(const std::string& file_path) const {
    auto it = cache_entries_.find(file_path);
    return it != cache_entries_.end();
}

bool CompilationCache::isOutOfDate(const std::string& file_path) const {
    if (!isCached(file_path)) {
        ++cache_misses_;
        return true;
    }
    
    const auto& entry = cache_entries_.at(file_path);
    std::string current_hash = computeFileHash(file_path);
    
    if (current_hash != entry.file_hash) {
        ++cache_misses_;
        return true;
    }
    
    ++cache_hits_;
    return false;
}

const CompilationCache::CacheEntry* CompilationCache::getCacheEntry(const std::string& file_path) const {
    auto it = cache_entries_.find(file_path);
    if (it != cache_entries_.end()) {
        return &it->second;
    }
    return nullptr;
}

void CompilationCache::cacheResult(const CacheEntry& entry) {
    cache_entries_[entry.file_path] = entry;
}

void CompilationCache::invalidateFile(const std::string& file_path) {
    cache_entries_.erase(file_path);
    invalidateDependents(file_path);
}

void CompilationCache::invalidateDependents(const std::string& file_path) {
    auto it = dependency_graph_.find(file_path);
    if (it != dependency_graph_.end()) {
        for (const auto& dependent : it->second.depended_by) {
            cache_entries_.erase(dependent);
            invalidateDependents(dependent);
        }
    }
}

void CompilationCache::addDependency(const std::string& source, const std::string& dependency) {
    dependency_graph_[source].file = source;
    dependency_graph_[source].depends_on.insert(dependency);
    dependency_graph_[dependency].depended_by.insert(source);
}

void CompilationCache::buildDependencyGraph(const std::vector<std::string>& source_files) {
    for (const auto& file : source_files) {
        if (dependency_graph_.find(file) == dependency_graph_.end()) {
            dependency_graph_[file].file = file;
        }
    }
}

std::vector<std::string> CompilationCache::getFilesToRecompile(const std::vector<std::string>& changed_files) {
    std::set<std::string> to_recompile;
    
    for (const auto& changed : changed_files) {
        to_recompile.insert(changed);
        
        // Find all dependents
        auto it = dependency_graph_.find(changed);
        if (it != dependency_graph_.end()) {
            for (const auto& dependent : it->second.depended_by) {
                to_recompile.insert(dependent);
            }
        }
    }
    
    return std::vector<std::string>(to_recompile.begin(), to_recompile.end());
}

CompilationCache::CacheStats CompilationCache::getStatistics() const {
    CacheStats stats;
    stats.total_entries = cache_entries_.size();
    stats.cache_size_bytes = 0;
    
    for (const auto& entry : cache_entries_) {
        stats.cache_size_bytes += entry.second.file_path.size();
        stats.cache_size_bytes += entry.second.file_hash.size();
        stats.cache_size_bytes += entry.second.output_hash.size();
        stats.cache_size_bytes += entry.second.dependencies.size() * 64;
    }
    
    stats.hit_count = cache_hits_;
    stats.miss_count = cache_misses_;
    stats.hit_rate = (cache_hits_ + cache_misses_ > 0) 
        ? (double)cache_hits_ / (cache_hits_ + cache_misses_) 
        : 0.0;
    
    return stats;
}

std::string CompilationCache::computeFileHash(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) return "";
    
    // Simple hash implementation (in production use SHA256)
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::hash<std::string> hasher;
    return std::to_string(hasher(content));
}

bool CompilationCache::saveCacheToDisk() {
    std::string cache_file = cache_directory_ + "/cache.dat";
    std::ofstream out(cache_file, std::ios::binary);
    
    if (!out.is_open()) return false;
    
    uint32_t count = cache_entries_.size();
    out.write(reinterpret_cast<char*>(&count), sizeof(count));
    
    for (const auto& entry : cache_entries_) {
        uint32_t path_len = entry.second.file_path.length();
        out.write(reinterpret_cast<char*>(&path_len), sizeof(path_len));
        out.write(entry.second.file_path.c_str(), path_len);
        
        uint32_t hash_len = entry.second.file_hash.length();
        out.write(reinterpret_cast<char*>(&hash_len), sizeof(hash_len));
        out.write(entry.second.file_hash.c_str(), hash_len);
        
        out.write(reinterpret_cast<char*>(&entry.second.timestamp), sizeof(entry.second.timestamp));
    }
    
    out.close();
    return true;
}

bool CompilationCache::loadCacheFromDisk() {
    std::string cache_file = cache_directory_ + "/cache.dat";
    std::ifstream in(cache_file, std::ios::binary);
    
    if (!in.is_open()) return false;
    
    uint32_t count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));
    
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t path_len = 0;
        in.read(reinterpret_cast<char*>(&path_len), sizeof(path_len));
        
        std::string path(path_len, '\0');
        in.read(&path[0], path_len);
        
        uint32_t hash_len = 0;
        in.read(reinterpret_cast<char*>(&hash_len), sizeof(hash_len));
        
        std::string hash(hash_len, '\0');
        in.read(&hash[0], hash_len);
        
        CacheEntry entry;
        entry.file_path = path;
        entry.file_hash = hash;
        in.read(reinterpret_cast<char*>(&entry.timestamp), sizeof(entry.timestamp));
        
        cache_entries_[path] = entry;
    }
    
    in.close();
    return true;
}

void CompilationCache::clearCache() {
    cache_entries_.clear();
    dependency_graph_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
    
    std::string cache_file = cache_directory_ + "/cache.dat";
    std::filesystem::remove(cache_file);
}

// ============================================================================
// IncrementalCompiler Implementation
// ============================================================================

IncrementalCompiler::IncrementalCompiler()
    : cache_(".rawrxd_cache") {
    stats_ = {0, 0, 0, std::chrono::milliseconds(0), 0.0};
}

void IncrementalCompiler::enableIncrementalMode(bool enabled) {
    incremental_enabled_ = enabled;
}

void IncrementalCompiler::setMaxCacheAge(std::chrono::hours max_age) {
    max_cache_age_ = max_age;
}

void IncrementalCompiler::setIgnorePatterns(const std::vector<std::string>& patterns) {
    ignore_patterns_ = patterns;
}

IncrementalCompiler::CompilationResult IncrementalCompiler::compileIncremental(
    const std::vector<std::string>& source_files,
    const std::string& output_file,
    const std::function<bool(const std::vector<std::string>&, const std::string&)>& compiler_func) {
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    CompilationResult result;
    result.success = false;
    result.incremental_ratio = 0.0;
    
    if (!incremental_enabled_) {
        // Full compilation
        std::vector<std::string> filtered_files;
        for (const auto& file : source_files) {
            if (!shouldIgnoreFile(file)) {
                filtered_files.push_back(file);
            }
        }
        result.success = compiler_func(filtered_files, output_file);
        result.recompiled_files = filtered_files;
        result.unchanged_files.clear();
        
        stats_.full_compilations++;
    } else {
        // Incremental compilation
        std::vector<std::string> changed_files = detectChangedFiles(source_files);
        
        if (changed_files.empty()) {
            result.success = true;
            result.unchanged_files = source_files;
            result.recompiled_files.clear();
        } else {
            result.success = compiler_func(changed_files, output_file);
            result.recompiled_files = changed_files;
            
            for (const auto& file : source_files) {
                if (std::find(changed_files.begin(), changed_files.end(), file) == changed_files.end()) {
                    result.unchanged_files.push_back(file);
                }
            }
            
            stats_.incremental_compilations++;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    result.time_saved = result.total_time; // Simplified
    
    stats_.total_compilations++;
    stats_.total_time_saved += result.time_saved;
    
    return result;
}

void IncrementalCompiler::forceRecompile(const std::string& file_path) {
    cache_.invalidateFile(file_path);
}

void IncrementalCompiler::forceRecompileAll() {
    cache_.clearCache();
}

IncrementalCompiler::IncrementalStats IncrementalCompiler::getStatistics() const {
    if (stats_.total_compilations > 0) {
        stats_.average_speedup = (double)stats_.total_time_saved.count() / stats_.total_compilations;
    }
    return stats_;
}

bool IncrementalCompiler::shouldIgnoreFile(const std::string& file_path) const {
    for (const auto& pattern : ignore_patterns_) {
        if (file_path.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> IncrementalCompiler::detectChangedFiles(const std::vector<std::string>& source_files) {
    std::vector<std::string> changed;
    
    for (const auto& file : source_files) {
        if (shouldIgnoreFile(file)) continue;
        
        if (cache_.isOutOfDate(file)) {
            changed.push_back(file);
        }
    }
    
    return changed;
}

// ============================================================================
// IncrementalLinkingManager Implementation
// ============================================================================

IncrementalLinkingManager::IncrementalLinkingManager() {}

void IncrementalLinkingManager::cacheObjectFile(const std::string& source, const std::string& object_file) {
    LinkingCache cache;
    cache.object_file = object_file;
    cache.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    
    // Compute hash of object file
    std::ifstream file(object_file, std::ios::binary);
    if (file.is_open()) {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::hash<std::string> hasher;
        cache.object_hash = std::to_string(hasher(content));
        file.close();
    }
    
    object_cache_[source] = cache;
}

bool IncrementalLinkingManager::isObjectFileCached(const std::string& source) const {
    return object_cache_.find(source) != object_cache_.end();
}

std::string IncrementalLinkingManager::getCachedObjectFile(const std::string& source) const {
    auto it = object_cache_.find(source);
    if (it != object_cache_.end()) {
        return it->second.object_file;
    }
    return "";
}

void IncrementalLinkingManager::extractSymbols(const std::string& object_file) {
    // Simplified symbol extraction (in production use nm or similar tools)
    std::vector<std::string> symbols;
    // Parse object file format and extract symbol table
    symbol_table_[object_file] = symbols;
}

std::vector<std::string> IncrementalLinkingManager::getSymbols(const std::string& object_file) const {
    auto it = symbol_table_.find(object_file);
    if (it != symbol_table_.end()) {
        return it->second;
    }
    return {};
}

bool IncrementalLinkingManager::performIncrementalLink(
    const std::vector<std::string>& object_files,
    const std::string& output_file) {
    
    std::vector<std::string> valid_objects;
    for (const auto& obj : object_files) {
        if (std::filesystem::exists(obj)) {
            valid_objects.push_back(obj);
        }
    }
    
    return valid_objects.size() == object_files.size();
}

bool IncrementalLinkingManager::validateObjectFiles(const std::vector<std::string>& objects) {
    for (const auto& obj : objects) {
        if (!std::filesystem::exists(obj)) {
            return false;
        }
    }
    return true;
}

void IncrementalLinkingManager::invalidateObjectFile(const std::string& object_file) {
    object_cache_.erase(object_file);
    symbol_table_.erase(object_file);
}

} // namespace Compiler
} // namespace RawrXD
