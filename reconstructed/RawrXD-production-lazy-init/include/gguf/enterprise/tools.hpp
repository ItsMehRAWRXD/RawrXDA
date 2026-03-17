// gguf_enterprise_tools.hpp
// RawrXD Enterprise GGUF Hardening Layer - Reverse-engineered tools
// Zero-abstraction NT-native memory safety, scalable metadata parsing

#ifndef GGUF_ENTERPRISE_TOOLS_HPP
#define GGUF_ENTERPRISE_TOOLS_HPP

#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <new>
#include <cstring>
#include <istream>
#include <vector>
#include <cstdio>

// -------------------------------------------------------------------------
// Tool 1: RobustMemoryArena
// Pre-committed virtual arena with granular suballocation tracking
// Prevents fragmentation during massive metadata parsing
// -------------------------------------------------------------------------
class RobustMemoryArena {
    struct BlockHeader {
        size_t size;
        uint32_t magic;  // 0xDEADBEEF validation
        uint8_t flags;
    };
    
    void* base_addr_ = nullptr;
    size_t committed_ = 0;
    size_t capacity_ = 0;
    std::atomic<size_t> offset_{0};
    HANDLE heap_handle_ = nullptr;

public:
    explicit RobustMemoryArena(size_t reserve_mb = 1024) {
        capacity_ = reserve_mb * 1024 * 1024;
        base_addr_ = VirtualAlloc(nullptr, capacity_, 
            MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
        
        if (!base_addr_) {
            heap_handle_ = HeapCreate(HEAP_NO_SERIALIZE, 64 * 1024 * 1024, 0);
            base_addr_ = nullptr;
        } else {
            heap_handle_ = nullptr;
            VirtualAlloc(base_addr_, 64 * 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);
            committed_ = 64 * 1024 * 1024;
        }
    }

    [[nodiscard]] void* Allocate(size_t size, size_t align = 16) {
        if (heap_handle_) {
            return HeapAlloc(heap_handle_, HEAP_ZERO_MEMORY, size + align);
        }

        size_t current = offset_.load(std::memory_order_relaxed);
        size_t aligned = (current + align - 1) & ~(align - 1);
        size_t next = aligned + size + sizeof(BlockHeader);

        if (next > capacity_) {
            return std::malloc(size);
        }

        if (next > committed_) {
            size_t commit_needed = ((next - committed_) + 4095) & ~4095;
            VirtualAlloc(static_cast<char*>(base_addr_) + committed_, 
                commit_needed, MEM_COMMIT, PAGE_READWRITE);
            committed_ += commit_needed;
        }

        if (offset_.compare_exchange_weak(current, next, 
            std::memory_order_release, std::memory_order_relaxed)) {
            auto* header = reinterpret_cast<BlockHeader*>(
                static_cast<char*>(base_addr_) + aligned);
            header->size = size;
            header->magic = 0xDEADBEEF;
            header->flags = 0x01;
            return static_cast<char*>(base_addr_) + aligned + sizeof(BlockHeader);
        }
        
        return std::malloc(size);
    }

    void Deallocate(void* p) {
        if (!p) return;
        auto* header = reinterpret_cast<BlockHeader*>(
            static_cast<char*>(p) - sizeof(BlockHeader));
        
        if (header->magic == 0xDEADBEEF && (header->flags & 0x01)) {
            return;
        }
        std::free(p);
    }

    void Reset() { 
        offset_.store(0, std::memory_order_release);
        if (base_addr_) {
            memset(base_addr_, 0xCC, committed_);
        }
    }

    ~RobustMemoryArena() {
        if (base_addr_) {
            VirtualFree(base_addr_, 0, MEM_RELEASE);
        } else if (heap_handle_) {
            HeapDestroy(heap_handle_);
        }
    }
};

// -------------------------------------------------------------------------
// Tool 2: NtSectionMapper
// Production-grade memory-mapped file with dynamic prefetch
// -------------------------------------------------------------------------
class NtSectionMapper {
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMapping_ = nullptr;
    void* view_base_ = nullptr;
    size_t view_size_ = 0;
    size_t file_size_ = 0;
    
    struct PrefetchContext {
        uint64_t last_access_offset = 0;
        size_t prefetch_window = 256 * 1024;
        bool sequential_hint = true;
    } prefetch_;

public:
    struct MapResult {
        void* data;
        size_t size;
        bool is_copy;
    };

    bool Open(const wchar_t* path) {
        hFile_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED, nullptr);
        if (hFile_ == INVALID_HANDLE_VALUE) return false;

        LARGE_INTEGER sz;
        GetFileSizeEx(hFile_, &sz);
        file_size_ = static_cast<size_t>(sz.QuadPart);

        hMapping_ = CreateFileMappingW(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        return hMapping_ != nullptr;
    }

    MapResult MapRange(uint64_t offset, size_t size, bool copy_on_write = false) {
        if (offset + size > file_size_) {
            return {nullptr, 0, false};
        }

        SYSTEM_INFO si;
        GetSystemInfo(&si);
        DWORD alloc_gran = si.dwAllocationGranularity;

        uint64_t aligned_offset = (offset / alloc_gran) * alloc_gran;
        size_t view_offset = static_cast<size_t>(offset - aligned_offset);
        size_t view_size = size + view_offset;

        void* view = MapViewOfFileEx(hMapping_, 
            copy_on_write ? FILE_MAP_COPY : FILE_MAP_READ,
            static_cast<DWORD>(aligned_offset >> 32),
            static_cast<DWORD>(aligned_offset),
            view_size, nullptr);

        if (!view) {
            auto* buffer = static_cast<uint8_t*>(std::malloc(size));
            if (!buffer) return {nullptr, 0, false};
            
            OVERLAPPED ov = {};
            ov.Offset = static_cast<DWORD>(offset);
            ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
            
            DWORD read = 0;
            if (!ReadFile(hFile_, buffer, static_cast<DWORD>(size), &read, &ov)) {
                std::free(buffer);
                return {nullptr, 0, false};
            }
            return {buffer, read, true};
        }

        if (prefetch_.sequential_hint && 
            offset == prefetch_.last_access_offset + prefetch_.prefetch_window) {
            MEMORY_RANGE_ENTRY entry;
            entry.VirtualAddress = view;
            entry.NumberOfBytes = size + prefetch_.prefetch_window;
            PrefetchVirtualMemory(GetCurrentProcess(), &entry, 1, 0);
        }
        prefetch_.last_access_offset = offset;
        prefetch_.prefetch_window = std::min(prefetch_.prefetch_window * 2,
            static_cast<size_t>(16 * 1024 * 1024));

        view_size_ = view_size;
        view_base_ = view;
        
        return {static_cast<uint8_t*>(view) + view_offset, size, false};
    }

    void Unmap(MapResult& result) {
        if (result.is_copy) {
            std::free(result.data);
        } else if (view_base_) {
            UnmapViewOfFile(view_base_);
        }
        result = {nullptr, 0, false};
    }

    bool CheckMemoryPressure(size_t requested_bytes) {
        MEMORYSTATUSEX ms = {sizeof(ms)};
        GlobalMemoryStatusEx(&ms);
        return ms.ullAvailVirtual > (requested_bytes * 2);
    }

    ~NtSectionMapper() {
        if (hMapping_) CloseHandle(hMapping_);
        if (hFile_ != INVALID_HANDLE_VALUE) CloseHandle(hFile_);
    }
};

// -------------------------------------------------------------------------
// Tool 3: DefensiveGGUFScanner
// Pre-flight corruption detection without allocation
// -------------------------------------------------------------------------
class DefensiveGGUFScanner {
public:
    struct SafetyReport {
        bool valid;
        uint64_t max_tensor_offset;
        size_t max_metadata_size;
        uint32_t corruption_flags;
        char advisory[256];
    };

    SafetyReport PreflightScan(std::istream& stream) {
        SafetyReport report = {};
        report.valid = false;
        auto original_pos = stream.tellg();

        uint32_t magic;
        stream.read(reinterpret_cast<char*>(&magic), 4);
        if (magic != 0x46554747 && magic != 0x47475546) {
            strcpy_s(report.advisory, "Invalid magic bytes");
            return report;
        }

        uint32_t version;
        stream.read(reinterpret_cast<char*>(&version), 4);
        if (version > 3) {
            strcpy_s(report.advisory, "Unsupported GGUF version");
            return report;
        }

        uint64_t tensor_count, metadata_count;
        stream.read(reinterpret_cast<char*>(&tensor_count), 8);
        stream.read(reinterpret_cast<char*>(&metadata_count), 8);

        uint64_t max_metadata = 0;
        for (uint64_t i = 0; i < metadata_count && stream.good(); ++i) {
            uint64_t key_len;
            stream.read(reinterpret_cast<char*>(&key_len), 8);
            if (key_len > 65535) {
                report.corruption_flags |= 0x01;
                snprintf(report.advisory, sizeof(report.advisory),
                    "Suspicious key length at metadata %llu: %llu", i, key_len);
                return report;
            }
            stream.seekg(static_cast<std::streamoff>(key_len), std::ios::cur);
            uint32_t type;
            stream.read(reinterpret_cast<char*>(&type), 4);
            
            if (type == 8) {
                uint64_t str_len;
                stream.read(reinterpret_cast<char*>(&str_len), 8);
                if (str_len > 100 * 1024 * 1024) {
                    report.corruption_flags |= 0x02;
                    snprintf(report.advisory, sizeof(report.advisory),
                        "Oversized string detected at metadata %llu: %llu MB", i, str_len / (1024*1024));
                }
                stream.seekg(static_cast<std::streamoff>(str_len), std::ios::cur);
                max_metadata += str_len;
            } else if (type == 9) {
                uint32_t arr_type;
                uint64_t arr_len;
                stream.read(reinterpret_cast<char*>(&arr_type), 4);
                stream.read(reinterpret_cast<char*>(&arr_len), 8);
                if (arr_type == 8) {
                    if (arr_len > 1000000) {
                        report.corruption_flags |= 0x04;
                        stream.seekg(static_cast<std::streamoff>(arr_len * 8), std::ios::cur);
                    } else {
                        for (uint64_t j = 0; j < arr_len; ++j) {
                            uint64_t s;
                            stream.read(reinterpret_cast<char*>(&s), 8);
                            stream.seekg(static_cast<std::streamoff>(s), std::ios::cur);
                            max_metadata += s;
                        }
                    }
                } else {
                    size_t type_size = GetGGUFTypeSize(arr_type);
                    stream.seekg(static_cast<std::streamoff>(arr_len * type_size), std::ios::cur);
                }
            } else {
                stream.seekg(GetGGUFTypeSize(type), std::ios::cur);
            }
        }

        uint64_t max_offset = 0;
        for (uint64_t i = 0; i < tensor_count && stream.good(); ++i) {
            uint64_t name_len;
            stream.read(reinterpret_cast<char*>(&name_len), 8);
            stream.seekg(static_cast<std::streamoff>(name_len), std::ios::cur);
            
            uint32_t n_dims;
            stream.read(reinterpret_cast<char*>(&n_dims), 4);
            stream.seekg(n_dims * sizeof(uint64_t), std::ios::cur);
            
            uint32_t type;
            stream.read(reinterpret_cast<char*>(&type), 4);
            
            uint64_t offset;
            stream.read(reinterpret_cast<char*>(&offset), 8);
            
            if (offset > max_offset) max_offset = offset;
        }

        stream.seekg(original_pos);
        report.valid = (report.corruption_flags == 0);
        report.max_tensor_offset = max_offset;
        report.max_metadata_size = max_metadata;
        if (report.valid) {
            strcpy_s(report.advisory, "GGUF file structure validated");
        }
        return report;
    }

private:
    static size_t GetGGUFTypeSize(uint32_t type) {
        switch(type) {
            case 0: return 1;
            case 1: return 1;
            case 2: return 2;
            case 3: return 2;
            case 4: return 4;
            case 5: return 4;
            case 6: return 4;
            case 7: return 1;
            case 10: return 8;
            case 11: return 8;
            case 12: return 8;
            default: return 0;
        }
    }
};

// -------------------------------------------------------------------------
// Tool 4: HardenedStringPool
// Immutable string deduplication for tokenizer metadata
// -------------------------------------------------------------------------
class HardenedStringPool {
    struct Entry {
        char* data;
        size_t len;
        uint32_t hash;
        Entry* next;
    };
    
    RobustMemoryArena* arena_;
    Entry** buckets_;
    size_t bucket_count_;
    std::atomic<size_t> dedup_savings_{0};

public:
    explicit HardenedStringPool(RobustMemoryArena* arena, size_t buckets = 1024)
        : arena_(arena), bucket_count_(buckets) {
        buckets_ = static_cast<Entry**>(std::calloc(buckets, sizeof(Entry*)));
    }

    const char* Intern(const char* src, size_t len) {
        if (!src || len == 0) return "";
        uint32_t hash = HashString(src, len);
        size_t idx = hash & (bucket_count_ - 1);
        
        for (Entry* e = buckets_[idx]; e; e = e->next) {
            if (e->hash == hash && e->len == len && memcmp(e->data, src, len) == 0) {
                dedup_savings_ += len;
                return e->data;
            }
        }
        
        Entry* new_entry = static_cast<Entry*>(arena_->Allocate(sizeof(Entry)));
        char* str_copy = static_cast<char*>(arena_->Allocate(len + 1));
        memcpy(str_copy, src, len);
        str_copy[len] = '\0';
        
        new_entry->data = str_copy;
        new_entry->len = len;
        new_entry->hash = hash;
        new_entry->next = buckets_[idx];
        buckets_[idx] = new_entry;
        
        return str_copy;
    }

    size_t GetDedupSavings() const { return dedup_savings_.load(); }

private:
    static uint32_t HashString(const char* data, size_t len) {
        uint32_t hash = 2166136261U;
        for (size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint8_t>(data[i]);
            hash *= 16777619U;
        }
        return hash;
    }
};

#endif // GGUF_ENTERPRISE_TOOLS_HPP
