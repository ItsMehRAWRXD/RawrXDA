#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <functional>
#include <cstring>

// Simple IDE core for TLS beaconism
class SystemsIDECore {
private:
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};
    std::unordered_map<std::string, std::string> file_cache_;
    
public:
    SystemsIDECore();
    ~SystemsIDECore();
    
    bool Initialize();
    void Shutdown();
    
    std::vector<std::string> ListDirectory(const std::string& path);
    std::string ReadFileOptimized(const std::string& path);
    bool WriteFileOptimized(const std::string& path, const std::string& content);
    std::vector<std::string> SearchFiles(const std::string& pattern, const std::string& root_path);
    
    double GetCacheHitRatio() const;
};