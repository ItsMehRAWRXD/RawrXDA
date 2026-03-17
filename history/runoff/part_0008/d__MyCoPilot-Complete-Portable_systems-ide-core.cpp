// systems-ide-core.cpp
// Implementation of high-performance IDE core

#include "systems-ide-core.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>

SystemsIDECore::SystemsIDECore() 
    : memory_pool_(std::make_unique<MemoryPool>(64 * 1024 * 1024)), // 64MB pool
      text_processor_(std::make_unique<TextProcessor>()),
      compiler_engine_(std::make_unique<CompilerEngine>()),
      debug_engine_(std::make_unique<DebugEngine>()),
      network_engine_(std::make_unique<NetworkEngine>()) {
}

SystemsIDECore::~SystemsIDECore() {
    Shutdown();
}

bool SystemsIDECore::Initialize() {
    // Enable hardware features
    uint32_t eax, ebx, ecx, edx;
    cpuid_info(1, &eax, &ebx, &ecx, &edx);
    
    // Check for required CPU features
    bool has_sse42 = (ecx & (1 << 20)) != 0;
    bool has_avx2 = (ecx & (1 << 28)) != 0;
    
    if (!has_sse42) {
        std::cerr << "Error: CPU lacks required SSE4.2 support\n";
        return false;
    }
    
    // Initialize performance counters
    StartPerfCounters();
    
    // Map D: drive for direct access
    if (!MapDrive("D:")) {
        std::cerr << "Warning: Could not map D: drive for direct access\n";
    }
    
    // Initialize subsystems
    if (!network_engine_->StartServer(8080)) {
        std::cerr << "Error: Failed to start network engine\n";
        return false;
    }
    
    debug_engine_->EnablePMU();
    
    std::cout << "Systems IDE Core initialized successfully\n";
    std::cout << "CPU Features: SSE4.2=" << has_sse42 << ", AVX2=" << has_avx2 << "\n";
    
    return true;
}

void SystemsIDECore::Shutdown() {
    running_.store(false);
    
    if (drive_mapping_.mapped_view) {
        UnmapViewOfFile(drive_mapping_.mapped_view);
    }
    if (drive_mapping_.mapping_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(drive_mapping_.mapping_handle);
    }
    if (drive_mapping_.drive_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(drive_mapping_.drive_handle);
    }
    
    network_engine_->StopServer();
    debug_engine_->DisablePMU();
}

bool SystemsIDECore::MapDrive(const std::string& drive_letter) {
    std::string drive_path = "\\\\.\\" + drive_letter;
    
    // Open drive handle with direct access
    drive_mapping_.drive_handle = CreateFileA(
        drive_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );
    
    if (drive_mapping_.drive_handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Get drive size
    LARGE_INTEGER drive_size;
    if (!GetFileSizeEx(drive_mapping_.drive_handle, &drive_size)) {
        CloseHandle(drive_mapping_.drive_handle);
        return false;
    }
    
    drive_mapping_.drive_size = drive_size.QuadPart;
    
    // Create file mapping (for large drives, map sections as needed)
    drive_mapping_.mapping_handle = CreateFileMappingA(
        drive_mapping_.drive_handle,
        nullptr,
        PAGE_READONLY,
        0, 0,  // Map entire drive
        nullptr
    );
    
    if (!drive_mapping_.mapping_handle) {
        CloseHandle(drive_mapping_.drive_handle);
        return false;
    }
    
    return true;
}

std::vector<std::string> SystemsIDECore::ListDirectory(const std::string& path) {
    std::vector<std::string> results;
    
    try {
        // Use direct WIN32 API for maximum performance
        WIN32_FIND_DATAA find_data;
        std::string search_path = path + "\\*";
        
        HANDLE find_handle = FindFirstFileA(search_path.c_str(), &find_data);
        if (find_handle == INVALID_HANDLE_VALUE) {
            return results;
        }
        
        do {
            if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
                std::string entry = find_data.cFileName;
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    entry = "[DIR] " + entry;
                }
                results.push_back(entry);
            }
        } while (FindNextFileA(find_handle, &find_data));
        
        FindClose(find_handle);
    } catch (const std::exception& e) {
        std::cerr << "Error listing directory: " << e.what() << "\n";
    }
    
    return results;
}

std::string SystemsIDECore::ReadFileOptimized(const std::string& path) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(file_cache_.cache_mutex);
        auto it = file_cache_.cache.find(path);
        if (it != file_cache_.cache.end()) {
            file_cache_.cache_hits.fetch_add(1);
            return std::string(it->second.begin(), it->second.end());
        }
        file_cache_.cache_misses.fetch_add(1);
    }
    
    // Use memory-mapped file I/O for large files
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
    
    // For small files, use direct read
    if (file_size.QuadPart < 1024 * 1024) {  // < 1MB
        std::vector<char> buffer(file_size.QuadPart);
        DWORD bytes_read;
        
        if (ReadFile(file_handle, buffer.data(), file_size.LowPart, &bytes_read, nullptr)) {
            CloseHandle(file_handle);
            
            // Cache the file
            {
                std::lock_guard<std::mutex> lock(file_cache_.cache_mutex);
                file_cache_.cache[path] = buffer;
            }
            
            return std::string(buffer.begin(), buffer.end());
        }
    } else {
        // Use memory mapping for large files
        HANDLE mapping_handle = CreateFileMappingA(file_handle, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (mapping_handle) {
            void* mapped_view = MapViewOfFile(mapping_handle, FILE_MAP_READ, 0, 0, 0);
            if (mapped_view) {
                std::string content(static_cast<char*>(mapped_view), file_size.QuadPart);
                UnmapViewOfFile(mapped_view);
                CloseHandle(mapping_handle);
                CloseHandle(file_handle);
                return content;
            }
            CloseHandle(mapping_handle);
        }
    }
    
    CloseHandle(file_handle);
    return "";
}

bool SystemsIDECore::WriteFileOptimized(const std::string& path, const std::string& content) {
    // Invalidate cache entry
    {
        std::lock_guard<std::mutex> lock(file_cache_.cache_mutex);
        file_cache_.cache.erase(path);
    }
    
    HANDLE file_handle = CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );
    
    if (file_handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    DWORD bytes_written;
    bool success = WriteFile(
        file_handle,
        content.c_str(),
        content.length(),
        &bytes_written,
        nullptr
    );
    
    CloseHandle(file_handle);
    return success && bytes_written == content.length();
}

std::vector<std::string> SystemsIDECore::SearchFiles(const std::string& pattern, const std::string& root_path) {
    std::vector<std::string> results;
    
    // Use parallel directory traversal for speed
    std::function<void(const std::string&)> search_recursive = [&](const std::string& current_path) {
        WIN32_FIND_DATAA find_data;
        std::string search_path = current_path + "\\*";
        
        HANDLE find_handle = FindFirstFileA(search_path.c_str(), &find_data);
        if (find_handle == INVALID_HANDLE_VALUE) {
            return;
        }
        
        do {
            if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
                std::string full_path = current_path + "\\" + find_data.cFileName;
                
                // Use SIMD-optimized pattern matching
                if (FastStringCompare(find_data.cFileName, pattern.c_str(), 
                                    std::min(strlen(find_data.cFileName), pattern.length()))) {
                    results.push_back(full_path);
                }
                
                // Recurse into subdirectories
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    search_recursive(full_path);
                }
            }
        } while (FindNextFileA(find_handle, &find_data));
        
        FindClose(find_handle);
    };
    
    search_recursive(root_path);
    return results;
}

double SystemsIDECore::GetCacheHitRatio() const {
    uint64_t hits = file_cache_.cache_hits.load();
    uint64_t misses = file_cache_.cache_misses.load();
    uint64_t total = hits + misses;
    
    return total > 0 ? static_cast<double>(hits) / total : 0.0;
}

// Memory Pool Implementation
SystemsIDECore::MemoryPool::MemoryPool(size_t size) : pool_size_(size) {
    // Allocate large aligned memory block
    pool_memory_ = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pool_memory_) {
        throw std::runtime_error("Failed to allocate memory pool");
    }
    
    // Initialize free list
    free_list_ = static_cast<Block*>(pool_memory_);
    free_list_->next = nullptr;
    free_list_->size = size - sizeof(Block);
    free_list_->is_free = true;
}

SystemsIDECore::MemoryPool::~MemoryPool() {
    if (pool_memory_) {
        VirtualFree(pool_memory_, 0, MEM_RELEASE);
    }
}

void* SystemsIDECore::MemoryPool::Allocate(size_t size) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    Block* current = free_list_;
    Block* previous = nullptr;
    
    while (current) {
        if (current->is_free && current->size >= size) {
            // Split block if necessary
            if (current->size > size + sizeof(Block)) {
                Block* new_block = reinterpret_cast<Block*>(
                    reinterpret_cast<char*>(current) + sizeof(Block) + size
                );
                new_block->size = current->size - size - sizeof(Block);
                new_block->is_free = true;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->is_free = false;
            return reinterpret_cast<char*>(current) + sizeof(Block);
        }
        
        previous = current;
        current = current->next;
    }
    
    return nullptr; // No suitable block found
}

void SystemsIDECore::MemoryPool::Deallocate(void* ptr) {
    if (!ptr) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    Block* block = reinterpret_cast<Block*>(
        reinterpret_cast<char*>(ptr) - sizeof(Block)
    );
    block->is_free = true;
    
    // Coalesce adjacent free blocks
    Block* current = free_list_;
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            current->size += sizeof(Block) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void* SystemsIDECore::MemoryPool::AllocateAligned(size_t size, size_t alignment) {
    size_t total_size = size + alignment - 1;
    void* ptr = Allocate(total_size);
    if (!ptr) return nullptr;
    
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    
    return reinterpret_cast<void*>(aligned_addr);
}

// Export C interface for JavaScript integration
extern "C" {
    __declspec(dllexport) SystemsIDECore* CreateIDECore() {
        return new SystemsIDECore();
    }
    
    __declspec(dllexport) void DestroyIDECore(SystemsIDECore* core) {
        delete core;
    }
    
    __declspec(dllexport) bool InitializeCore(SystemsIDECore* core) {
        return core->Initialize();
    }
    
    __declspec(dllexport) char* ListDirectoryC(SystemsIDECore* core, const char* path) {
        auto files = core->ListDirectory(path);
        std::string result;
        for (const auto& file : files) {
            result += file + "\n";
        }
        
        char* c_result = new char[result.length() + 1];
        strcpy_s(c_result, result.length() + 1, result.c_str());
        return c_result;
    }
    
    __declspec(dllexport) void FreeCString(char* str) {
        delete[] str;
    }
    
    __declspec(dllexport) char* ReadFileC(SystemsIDECore* core, const char* path) {
        std::string content = core->ReadFileOptimized(path);
        char* c_result = new char[content.length() + 1];
        strcpy_s(c_result, content.length() + 1, content.c_str());
        return c_result;
    }
    
    __declspec(dllexport) bool WriteFileC(SystemsIDECore* core, const char* path, const char* content) {
        return core->WriteFileOptimized(path, content);
    }
}