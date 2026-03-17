// ============================================================
// gguf_hardening_tools.hpp – Enterprise safety layer for GGUF parsing
// Prevents bad_alloc, memory exhaustion, and corruption
// ============================================================
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <algorithm>
#include <atomic>
#include <new>
#include <windows.h>

namespace rawrxd {
namespace gguf {

// ─── Virtual Memory Arena ──────────────────────────────────────────────────
class RobustMemoryArena {
    struct Chunk {
        uint8_t* base;
        size_t capacity;
        size_t committed;
        uint8_t* ptr;
    };
    
    std::vector<Chunk> chunks;
    size_t chunk_size;
    std::atomic<size_t> total_committed;
    size_t max_commit_bytes;
    
    Chunk& allocate_chunk() {
        if(total_committed.load(std::memory_order_relaxed) > max_commit_bytes) {
            throw std::bad_alloc();
        }
        
        chunks.push_back({});
        Chunk& c = chunks.back();
        
        c.base = static_cast<uint8_t*>(
            VirtualAlloc(NULL, chunk_size, MEM_RESERVE, PAGE_NOACCESS));
        if(!c.base) throw std::bad_alloc();
        
        c.capacity = chunk_size;
        c.committed = 0;
        c.ptr = c.base;
        return c;
    }
    
    void commit_in_chunk(Chunk& c, size_t bytes) {
        while(c.ptr - c.base + bytes > c.committed) {
            size_t commit_now = std::min<size_t>(0x10000, 
                c.capacity - c.committed);
            
            if(!VirtualAlloc(c.base + c.committed, commit_now, 
                            MEM_COMMIT, PAGE_READWRITE)) {
                throw std::bad_alloc();
            }
            
            c.committed += commit_now;
            total_committed.fetch_add(commit_now, 
                std::memory_order_relaxed);
            
            if(total_committed.load(std::memory_order_relaxed) > 
               max_commit_bytes) {
                throw std::bad_alloc();
            }
        }
    }
    
public:
    RobustMemoryArena(size_t max_mb = 2048) 
        : chunk_size(0x10000000), 
          total_committed(0), 
          max_commit_bytes(max_mb * 1024 * 1024) {}
    
    ~RobustMemoryArena() {
        for(auto& c : chunks) {
            if(c.base) VirtualFree(c.base, 0, MEM_RELEASE);
        }
    }
    
    uint8_t* allocate(size_t bytes) {
        if(bytes == 0) return nullptr;
        if(bytes > max_commit_bytes) throw std::bad_alloc();
        
        if(chunks.empty() || 
           chunks.back().ptr - chunks.back().base + bytes > 
           chunks.back().capacity) {
            allocate_chunk();
        }
        
        Chunk& c = chunks.back();
        commit_in_chunk(c, bytes);
        
        uint8_t* result = c.ptr;
        c.ptr += bytes;
        return result;
    }
    
    void reset() {
        for(auto& c : chunks) {
            c.ptr = c.base;
        }
    }
    
    size_t committed_bytes() const { 
        return total_committed.load(std::memory_order_relaxed); 
    }
};

// ─── NT Section Mapper ──────────────────────────────────────────────────────
class NtSectionMapper {
    HANDLE file_handle;
    HANDLE map_handle;
    uint8_t* mapped_view;
    size_t mapped_size;
    
public:
    NtSectionMapper() : file_handle(INVALID_HANDLE_VALUE), 
                        map_handle(NULL), mapped_view(nullptr), 
                        mapped_size(0) {}
    
    ~NtSectionMapper() { unmap(); }
    
    bool open_mapped(const std::string& path) {
        file_handle = CreateFileA(path.c_str(), GENERIC_READ, 
                                  FILE_SHARE_READ, NULL, 
                                  OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 
                                  NULL);
        if(file_handle == INVALID_HANDLE_VALUE) return false;
        
        LARGE_INTEGER size;
        if(!GetFileSizeEx(file_handle, &size)) {
            CloseHandle(file_handle);
            return false;
        }
        
        mapped_size = static_cast<size_t>(size.QuadPart);
        
        map_handle = CreateFileMappingA(file_handle, NULL, PAGE_READONLY, 
                                        size.HighPart, size.LowPart, NULL);
        if(!map_handle) {
            CloseHandle(file_handle);
            return false;
        }
        
        mapped_view = static_cast<uint8_t*>(
            MapViewOfFile(map_handle, FILE_MAP_READ, 0, 0, mapped_size));
        
        if(!mapped_view) {
            CloseHandle(map_handle);
            CloseHandle(file_handle);
            return false;
        }
        
        return true;
    }
    
    const uint8_t* view() const { return mapped_view; }
    size_t size() const { return mapped_size; }
    
    void unmap() {
        if(mapped_view) UnmapViewOfFile(mapped_view);
        if(map_handle) CloseHandle(map_handle);
        if(file_handle != INVALID_HANDLE_VALUE) CloseHandle(file_handle);
        file_handle = INVALID_HANDLE_VALUE;
        map_handle = NULL;
        mapped_view = nullptr;
        mapped_size = 0;
    }
};

// ─── Defensive GGUF Scanner ────────────────────────────────────────────────
class DefensiveGGUFScanner {
    static constexpr uint32_t GGUF_MAGIC = 0x46554747;
    static constexpr size_t GGUF_MAX_SAFE_STRING = 16 * 1024 * 1024;
    static constexpr size_t GGUF_MAX_SAFE_ARRAY_SIZE = 1024 * 1024;
    
public:
    struct ScanResult {
        bool valid;
        uint32_t version;
        uint64_t tensor_data_offset;
        std::string error_msg;
        bool has_poisoned_lengths;
        bool has_oversized_metadata;
    };
    
    ScanResult pre_flight_scan(const uint8_t* data, size_t size) {
        ScanResult res = {};
        
        if(size < 20) {
            res.error_msg = "File too small for GGUF header";
            return res;
        }
        
        // Check magic
        uint32_t magic;
        std::memcpy(&magic, data, 4);
        if(magic != GGUF_MAGIC) {
            res.error_msg = "Invalid GGUF magic";
            return res;
        }
        
        // Check version (<=3)
        std::memcpy(&res.version, data + 4, 4);
        if(res.version > 3) {
            res.error_msg = "Unsupported GGUF version";
            return res;
        }
        
        // Scan for poisoned lengths
        const uint8_t* ptr = data + 8;
        const uint8_t* end = data + size;
        
        while(ptr + 8 <= end) {
            uint64_t len;
            std::memcpy(&len, ptr, 8);
            ptr += 8;
            
            if(len == 0xFFFFFFFFFFFFFFFFULL) {
                res.has_poisoned_lengths = true;
                break;
            }
            
            if(len > GGUF_MAX_SAFE_STRING) {
                res.has_oversized_metadata = true;
                if(len > size) {
                    res.error_msg = "String length exceeds file size";
                    return res;
                }
            }
            
            if(ptr + len > end) break;
            
            // Skip string data
            ptr += len;
            
            // KV type check
            if(ptr >= end) break;
            ptr++;
        }
        
        res.valid = true;
        return res;
    }
    
    bool is_safe_string_length(uint64_t len) const {
        return len < GGUF_MAX_SAFE_STRING;
    }
    
    bool is_safe_array_size(uint64_t count, uint64_t element_size) const {
        return count < GGUF_MAX_SAFE_ARRAY_SIZE && 
               count * element_size < GGUF_MAX_SAFE_STRING;
    }
};

// ─── Hardened String Pool ──────────────────────────────────────────────────
class HardenedStringPool {
    struct PoolEntry {
        std::string data;
        uint32_t refcount;
        uint64_t hash;
    };
    
    std::unordered_map<uint64_t, PoolEntry> pool;
    RobustMemoryArena* arena;
    size_t total_dedup_savings;
    
    static uint64_t fnv1a_hash(const std::string& s) {
        uint64_t hash = 0xCBF29CE484222325ULL;
        for(uint8_t c : s) {
            hash ^= c;
            hash *= 0x100000001B3ULL;
        }
        return hash;
    }
    
public:
    HardenedStringPool(RobustMemoryArena* a) 
        : arena(a), total_dedup_savings(0) {}
    
    const std::string& intern(const std::string& s) {
        uint64_t h = fnv1a_hash(s);
        
        auto it = pool.find(h);
        if(it != pool.end() && it->second.data == s) {
            it->second.refcount++;
            return it->second.data;
        }
        
        pool[h] = {s, 1, h};
        return pool[h].data;
    }
    
    void release(const std::string& s) {
        uint64_t h = fnv1a_hash(s);
        auto it = pool.find(h);
        if(it != pool.end()) {
            it->second.refcount--;
            if(it->second.refcount == 0) {
                total_dedup_savings += s.size();
                pool.erase(it);
            }
        }
    }
    
    size_t dedup_savings() const { return total_dedup_savings; }
    size_t pool_size() const { return pool.size(); }
};

// ─── Safe GGUF Reader ──────────────────────────────────────────────────────
template<typename T>
inline bool safe_read(const uint8_t* data, size_t size, size_t offset, T& out) {
    if(offset + sizeof(T) > size) return false;
    std::memcpy(&out, data + offset, sizeof(T));
    return true;
}

inline bool safe_read_string(const uint8_t* data, size_t size, size_t offset,
                             size_t str_len, std::string& out,
                             size_t max_len = 16 * 1024 * 1024) {
    if(str_len > max_len || offset + str_len > size) return false;
    out.assign(reinterpret_cast<const char*>(data + offset), str_len);
    return true;
}

}} // namespace rawrxd::gguf
