#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#else
#include <cpuid.h>
#endif

struct PerfCounters {
    uint64_t cycles_start = 0;
    uint64_t instructions_retired = 0;
    uint64_t cache_references = 0;
    uint64_t cache_misses = 0;
    uint64_t branch_misses = 0;
};

class SystemsIDECore {
private:
    std::unordered_map<std::string, std::string> file_cache_;
    PerfCounters perf_counters_;
    uint64_t cache_hits_ = 0;
    uint64_t cache_requests_ = 0;

public:
    bool Initialize() {
        ReadPerfCounters();
        return true;
    }

    void Shutdown() {
        file_cache_.clear();
    }

    std::vector<std::string> ListDirectory(const std::string& path) {
        std::vector<std::string> files;
        
#ifdef _WIN32
        WIN32_FIND_DATAA find_data;
        std::string search_path = path + "\\*";
        HANDLE handle = FindFirstFileA(search_path.c_str(), &find_data);
        
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
                    std::string entry = find_data.cFileName;
                    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        entry += "/";
                    }
                    files.push_back(entry);
                }
            } while (FindNextFileA(handle, &find_data));
            FindClose(handle);
        }
#endif
        return files;
    }

    std::string ReadFileOptimized(const std::string& path) {
        cache_requests_++;
        
        auto it = file_cache_.find(path);
        if (it != file_cache_.end()) {
            cache_hits_++;
            return it->second;
        }

#ifdef _WIN32
        HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                                 nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        
        if (file == INVALID_HANDLE_VALUE) {
            return "";
        }

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(file, &file_size)) {
            CloseHandle(file);
            return "";
        }

        std::string content(file_size.QuadPart, '\0');
        DWORD bytes_read;
        
        if (!ReadFile(file, &content[0], file_size.QuadPart, &bytes_read, nullptr)) {
            CloseHandle(file);
            return "";
        }
        
        CloseHandle(file);
        
        if (content.size() < 1024 * 1024) { // Cache files under 1MB
            file_cache_[path] = content;
        }
        
        return content;
#else
        return "";
#endif
    }

    std::vector<std::string> SearchFiles(const std::string& pattern, const std::string& root) {
        std::vector<std::string> results;
        SearchRecursive(pattern, root, results);
        return results;
    }

    PerfCounters GetPerfCounters() const {
        return perf_counters_;
    }

    double GetCacheHitRatio() const {
        return cache_requests_ > 0 ? (double)cache_hits_ / cache_requests_ : 0.0;
    }

private:
    void SearchRecursive(const std::string& pattern, const std::string& dir, std::vector<std::string>& results) {
#ifdef _WIN32
        WIN32_FIND_DATAA find_data;
        std::string search_path = dir + "\\*";
        HANDLE handle = FindFirstFileA(search_path.c_str(), &find_data);
        
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
                    std::string full_path = dir + "\\" + find_data.cFileName;
                    
                    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                        SearchRecursive(pattern, full_path, results);
                    } else {
                        if (MatchesPattern(find_data.cFileName, pattern)) {
                            results.push_back(full_path);
                        }
                    }
                }
            } while (FindNextFileA(handle, &find_data));
            FindClose(handle);
        }
#endif
    }

    bool MatchesPattern(const std::string& filename, const std::string& pattern) {
        if (pattern == "*") return true;
        if (pattern.find("*.") == 0) {
            std::string ext = pattern.substr(1);
            return filename.size() >= ext.size() && 
                   filename.substr(filename.size() - ext.size()) == ext;
        }
        return filename.find(pattern) != std::string::npos;
    }

    void ReadPerfCounters() {
#ifdef _WIN32
        perf_counters_.cycles_start = __rdtsc();
#endif
    }
};

inline void cpuid_info(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
#ifdef _WIN32
    int regs[4];
    __cpuid(regs, leaf);
    *eax = regs[0];
    *ebx = regs[1];
    *ecx = regs[2];
    *edx = regs[3];
#else
    __cpuid(leaf, *eax, *ebx, *ecx, *edx);
#endif
}