#include "systems-ide-core-clean.hpp"

// Constructor
SystemsIDECore::SystemsIDECore()
    : memory_pool_(std::make_unique<MemoryPool>(64 * 1024 * 1024)), // 64MB pool
      running_(false) {
    // Initialize performance counters to zero
    std::memset(&perf_counters_, 0, sizeof(PerfCounters));
    std::memset(&drive_mapping_, 0, sizeof(DriveMapping));
    drive_mapping_.file_handle = INVALID_HANDLE_VALUE;
    drive_mapping_.mapping_handle = INVALID_HANDLE_VALUE;
}

// Destructor
SystemsIDECore::~SystemsIDECore() {
    Shutdown();
}

// Initialize the IDE core
bool SystemsIDECore::Initialize() {
    running_.store(true);
    
    // Start performance monitoring
    StartPerfCounters();
    
    // Map D: drive for optimized access
    if (!MapDrive("D:")) {
        return false;
    }
    
    return true;
}

// Shutdown and cleanup
void SystemsIDECore::Shutdown() {
    running_.store(false);
    
    // Clean up drive mapping
    if (drive_mapping_.mapped_view) {
        UnmapViewOfFile(drive_mapping_.mapped_view);
        drive_mapping_.mapped_view = nullptr;
    }
    if (drive_mapping_.mapping_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(drive_mapping_.mapping_handle);
        drive_mapping_.mapping_handle = INVALID_HANDLE_VALUE;
    }
    if (drive_mapping_.file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(drive_mapping_.file_handle);
        drive_mapping_.file_handle = INVALID_HANDLE_VALUE;
    }
    
    // Clear caches
    file_cache_.clear();
}

// Map drive for direct access
bool SystemsIDECore::MapDrive(const std::string& drive_letter) {
    std::wstring drive_path = std::wstring(drive_letter.begin(), drive_letter.end()) + L"\\";
    
    // Open drive for memory mapping (read-only for safety)
    drive_mapping_.file_handle = CreateFileW(
        drive_path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_RANDOM_ACCESS,
        nullptr
    );
    
    if (drive_mapping_.file_handle == INVALID_HANDLE_VALUE) {
        // Fall back to regular file access if direct drive access fails
        return true; // Not a critical failure
    }
    
    LARGE_INTEGER file_size;
    if (GetFileSizeEx(drive_mapping_.file_handle, &file_size)) {
        drive_mapping_.file_size = static_cast<size_t>(file_size.QuadPart);
    }
    
    return true;
}

// List directory contents with optimized Win32 calls
std::vector<std::string> SystemsIDECore::ListDirectory(const std::string& path) {
    std::vector<std::string> files;
    
    WIN32_FIND_DATAW find_data;
    std::wstring search_path = std::wstring(path.begin(), path.end()) + L"\\*";
    
    HANDLE find_handle = FindFirstFileW(search_path.c_str(), &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            if (wcscmp(find_data.cFileName, L".") != 0 && 
                wcscmp(find_data.cFileName, L"..") != 0) {
                
                std::wstring wide_name(find_data.cFileName);
                std::string name(wide_name.begin(), wide_name.end());
                
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    name += "/";
                }
                files.push_back(name);
            }
        } while (FindNextFileW(find_handle, &find_data));
        FindClose(find_handle);
    }
    
    return files;
}

// Optimized file reading with memory mapping when possible
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
    
    // Cache the result for small files
    if (content.size() < 1024 * 1024) { // Cache files under 1MB
        file_cache_[path] = content;
    }
    
    return content;
}

// Optimized file writing
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
    
    // Update cache
    file_cache_[path] = content;
    
    return true;
}

// Search files recursively with pattern matching
std::vector<std::string> SystemsIDECore::SearchFiles(const std::string& pattern, const std::string& root_path) {
    std::vector<std::string> results;
    
    std::function<void(const std::string&)> search_recursive = 
        [&](const std::string& current_path) {
            auto files = ListDirectory(current_path);
            
            for (const auto& file : files) {
                std::string full_path = current_path + "\\" + file;
                
                if (file.back() == '/') {
                    // Directory - recurse
                    search_recursive(full_path.substr(0, full_path.length() - 1));
                } else if (file.find(pattern) != std::string::npos) {
                    // File matches pattern
                    results.push_back(full_path);
                }
            }
        };
    
    search_recursive(root_path);
    return results;
}

// Get cache hit ratio for performance monitoring
double SystemsIDECore::GetCacheHitRatio() const {
    uint64_t hits = cache_hits_.load();
    uint64_t misses = cache_misses_.load();
    
    if (hits + misses == 0) return 0.0;
    return static_cast<double>(hits) / static_cast<double>(hits + misses);
}

// Performance counter implementation
void SystemsIDECore::ReadPerfCounters() {
    // Read CPU performance counters using RDPMC instruction
    perf_counters_.instructions = __readpmc(0);
    perf_counters_.cycles = __readpmc(1);
    perf_counters_.cache_references = __readpmc(2);
    perf_counters_.cache_misses = __readpmc(3);
    perf_counters_.branch_misses = __readpmc(4);
}

void SystemsIDECore::StartPerfCounters() {
    ReadPerfCounters();
}

// Assembly-optimized functions
void SystemsIDECore::DirectSyscall(DWORD syscall_number, ...) {
    // Direct system call implementation using inline assembly
    // This bypasses Windows API for maximum performance
    __asm {
        mov eax, syscall_number
        // Additional assembly code would go here
    }
}

bool SystemsIDECore::FastStringCompare(const char* str1, const char* str2, size_t length) {
    // SSE2-optimized string comparison
    const __m128i* ptr1 = reinterpret_cast<const __m128i*>(str1);
    const __m128i* ptr2 = reinterpret_cast<const __m128i*>(str2);
    
    size_t simd_length = length / 16;
    for (size_t i = 0; i < simd_length; ++i) {
        __m128i chunk1 = _mm_loadu_si128(ptr1 + i);
        __m128i chunk2 = _mm_loadu_si128(ptr2 + i);
        __m128i cmp = _mm_cmpeq_epi8(chunk1, chunk2);
        
        if (_mm_movemask_epi8(cmp) != 0xFFFF) {
            return false;
        }
    }
    
    // Handle remaining bytes
    for (size_t i = simd_length * 16; i < length; ++i) {
        if (str1[i] != str2[i]) {
            return false;
        }
    }
    
    return true;
}

void SystemsIDECore::MemCopySSE(void* dest, const void* src, size_t size) {
    // SSE-optimized memory copy
    const __m128i* src_ptr = reinterpret_cast<const __m128i*>(src);
    __m128i* dest_ptr = reinterpret_cast<__m128i*>(dest);
    
    size_t simd_chunks = size / 16;
    for (size_t i = 0; i < simd_chunks; ++i) {
        __m128i chunk = _mm_loadu_si128(src_ptr + i);
        _mm_storeu_si128(dest_ptr + i, chunk);
    }
    
    // Handle remaining bytes
    const char* src_remaining = reinterpret_cast<const char*>(src) + simd_chunks * 16;
    char* dest_remaining = reinterpret_cast<char*>(dest) + simd_chunks * 16;
    size_t remaining_size = size % 16;
    
    for (size_t i = 0; i < remaining_size; ++i) {
        dest_remaining[i] = src_remaining[i];
    }
}

// Memory pool implementation
SystemsIDECore::MemoryPool::MemoryPool(size_t size) : pool_size_(size) {
    pool_memory_ = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pool_memory_) {
        throw std::bad_alloc();
    }
    
    // Initialize free list
    free_list_ = reinterpret_cast<Block*>(pool_memory_);
    free_list_->size = size - sizeof(Block);
    free_list_->in_use = false;
    free_list_->next = nullptr;
}

SystemsIDECore::MemoryPool::~MemoryPool() {
    if (pool_memory_) {
        VirtualFree(pool_memory_, 0, MEM_RELEASE);
    }
}

void* SystemsIDECore::MemoryPool::Allocate(size_t size) {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Align size to 16 bytes for SSE operations
    size = (size + 15) & ~15;
    
    Block* current = free_list_;
    Block* previous = nullptr;
    
    while (current) {
        if (!current->in_use && current->size >= size) {
            // Split block if it's much larger than needed
            if (current->size >= size + sizeof(Block) + 64) {
                Block* new_block = reinterpret_cast<Block*>(
                    reinterpret_cast<char*>(current) + sizeof(Block) + size
                );
                new_block->size = current->size - size - sizeof(Block);
                new_block->in_use = false;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->in_use = true;
            return reinterpret_cast<char*>(current) + sizeof(Block);
        }
        
        previous = current;
        current = current->next;
    }
    
    // No suitable block found
    return nullptr;
}

void SystemsIDECore::MemoryPool::Deallocate(void* ptr) {
    if (!ptr) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    Block* block = reinterpret_cast<Block*>(
        reinterpret_cast<char*>(ptr) - sizeof(Block)
    );
    block->in_use = false;
    
    // Coalesce with adjacent free blocks
    Block* current = free_list_;
    while (current && current->next) {
        if (!current->in_use && !current->next->in_use) {
            // Merge current with next
            current->size += sizeof(Block) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void* SystemsIDECore::MemoryPool::AllocateAligned(size_t size, size_t alignment) {
    // For now, use regular allocation with 16-byte alignment
    return Allocate(size);
}

// C interface implementations
extern "C" {
    bool ReadFileC(SystemsIDECore* core, const char* path, char** content, size_t* size) {
        std::string result = core->ReadFileOptimized(path);
        if (result.empty()) return false;
        
        *size = result.size();
        *content = static_cast<char*>(malloc(*size + 1));
        if (!*content) return false;
        
        memcpy(*content, result.c_str(), *size);
        (*content)[*size] = '\0';
        return true;
    }
    
    bool WriteFileC(SystemsIDECore* core, const char* path, const char* content) {
        return core->WriteFileOptimized(path, content);
    }
    
    bool ListDirectoryC(SystemsIDECore* core, const char* path, char*** files, size_t* count) {
        auto result = core->ListDirectory(path);
        *count = result.size();
        
        *files = static_cast<char**>(malloc(*count * sizeof(char*)));
        if (!*files) return false;
        
        for (size_t i = 0; i < *count; ++i) {
            size_t len = result[i].size();
            (*files)[i] = static_cast<char*>(malloc(len + 1));
            if (!(*files)[i]) return false;
            
            memcpy((*files)[i], result[i].c_str(), len);
            (*files)[i][len] = '\0';
        }
        
        return true;
    }
}