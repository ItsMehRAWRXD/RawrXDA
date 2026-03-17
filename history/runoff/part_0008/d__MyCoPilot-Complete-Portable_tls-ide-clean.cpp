#include "tls-ide-clean.hpp"
#include <iostream>

SystemsIDECore::SystemsIDECore() : running_(false) {
}

SystemsIDECore::~SystemsIDECore() {
    Shutdown();
}

bool SystemsIDECore::Initialize() {
    running_.store(true);
    std::cout << "IDE Core initialized\n";
    return true;
}

void SystemsIDECore::Shutdown() {
    running_.store(false);
    file_cache_.clear();
    std::cout << "IDE Core shutdown\n";
}

std::vector<std::string> SystemsIDECore::ListDirectory(const std::string& path) {
    std::vector<std::string> files;
    
    WIN32_FIND_DATAA find_data;
    std::string search_path = path + "\\*";
    
    HANDLE find_handle = FindFirstFileA(search_path.c_str(), &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(find_data.cFileName, ".") != 0 && 
                strcmp(find_data.cFileName, "..") != 0) {
                
                std::string name(find_data.cFileName);
                
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    name += "/";
                }
                files.push_back(name);
            }
        } while (FindNextFileA(find_handle, &find_data));
        FindClose(find_handle);
    }
    
    return files;
}

std::string SystemsIDECore::ReadFileOptimized(const std::string& path) {
    cache_hits_.fetch_add(1);
    
    // Check cache first
    auto cache_it = file_cache_.find(path);
    if (cache_it != file_cache_.end()) {
        return cache_it->second;
    }
    
    cache_misses_.fetch_add(1);
    
    HANDLE file_handle = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );
    
    if (file_handle == INVALID_HANDLE_VALUE) {
        return "";
    }
    
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle, &file_size)) {
        CloseHandle(file_handle);
        return "";
    }
    
    if (file_size.QuadPart > INT_MAX) {
        CloseHandle(file_handle);
        return "";
    }
    
    std::string content;
    content.resize(static_cast<size_t>(file_size.QuadPart));
    
    DWORD bytes_read;
    BOOL success = ReadFile(
        file_handle,
        &content[0],
        static_cast<DWORD>(file_size.QuadPart),
        &bytes_read,
        nullptr
    );
    
    CloseHandle(file_handle);
    
    if (!success || bytes_read != file_size.QuadPart) {
        return "";
    }
    
    // Cache small files
    if (content.size() < 1024 * 1024) {
        file_cache_[path] = content;
    }
    
    return content;
}

bool SystemsIDECore::WriteFileOptimized(const std::string& path, const std::string& content) {
    HANDLE file_handle = CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (file_handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD bytes_written;
    BOOL success = WriteFile(
        file_handle,
        content.c_str(),
        static_cast<DWORD>(content.size()),
        &bytes_written,
        nullptr
    );
    
    CloseHandle(file_handle);
    
    if (!success || bytes_written != content.size()) {
        return false;
    }
    
    file_cache_[path] = content;
    return true;
}

std::vector<std::string> SystemsIDECore::SearchFiles(const std::string& pattern, const std::string& root_path) {
    std::vector<std::string> results;
    
    std::function<void(const std::string&)> search_recursive = 
        [&](const std::string& current_path) {
            auto files = ListDirectory(current_path);
            
            for (const auto& file : files) {
                std::string full_path = current_path + "\\" + file;
                
                if (file.back() == '/') {
                    search_recursive(full_path.substr(0, full_path.length() - 1));
                } else if (file.find(pattern) != std::string::npos) {
                    results.push_back(full_path);
                }
            }
        };
    
    search_recursive(root_path);
    return results;
}

double SystemsIDECore::GetCacheHitRatio() const {
    uint64_t hits = cache_hits_.load();
    uint64_t misses = cache_misses_.load();
    
    if (hits + misses == 0) return 0.0;
    return static_cast<double>(hits) / static_cast<double>(hits + misses);
}