// streaming_gguf_loader_impl.hpp
// Fully reverse-engineered robust parsing tools for GGUF/RLMM formats
// Zero STL exceptions in hot paths, manual bounds checking, memory poisoning

#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <type_traits>

namespace rawrxd::robust {

// ═════════════════════════════════════════════════════════════════════════════
// 1. SAFE MEMORY BUFFERS (Poison-on-free, canary guards)
// ═════════════════════════════════════════════════════════════════════════════

struct SecureBuffer {
    uint8_t* data = nullptr;
    size_t size = 0;
    size_t cap = 0;
    uint32_t canary_front = 0;
    uint32_t canary_back = 0;
    static constexpr uint32_t CANARY_MAGIC = 0xDEADBEEF;
    
    explicit SecureBuffer(size_t initial = 4096) { 
        reserve(initial); 
    }
    
    void reserve(size_t new_cap) {
        if (new_cap <= cap) return;
        
        // 16-byte align + canary space
        size_t alloc_size = new_cap + 32;
        uint8_t* new_data = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, alloc_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)
        );
        
        if (!new_data) {
            fprintf(stderr, "[FATAL] SecureBuffer: VirtualAlloc failed for %zu bytes\n", alloc_size);
            TerminateProcess(GetCurrentProcess(), 0xC0000017); // STATUS_NO_MEMORY
        }
        
        // Poison unused regions
        memset(new_data, 0xCC, alloc_size); // 0xCC = INT3 (debug pattern)
        
        // Setup canaries
        *reinterpret_cast<uint32_t*>(new_data) = CANARY_MAGIC;
        *reinterpret_cast<uint32_t*>(new_data + alloc_size - 4) = CANARY_MAGIC;
        
        if (data) {
            memcpy(new_data + 4, data + 4, size);
            VirtualFree(data - 4, 0, MEM_RELEASE);
        }
        
        data = new_data + 4; // Offset past front canary
        cap = new_cap;
    }
    
    void append(const void* src, size_t len) {
        if (size + len > cap) reserve((cap + len) * 2);
        memcpy(data + size, src, len);
        size += len;
    }
    
    void wipe() {
        if (!data) return;
        SecureZeroMemory(data, size); // Anti-forensic
        size = 0;
    }
    
    ~SecureBuffer() {
        if (data) {
            check_canaries();
            wipe();
            VirtualFree(data - 4, 0, MEM_RELEASE);
        }
    }
    
    void check_canaries() const {
        if (*reinterpret_cast<const uint32_t*>(data - 4) != CANARY_MAGIC ||
            *reinterpret_cast<const uint32_t*>(data + cap + 4) != CANARY_MAGIC) {
            fprintf(stderr, "[SECURITY] Buffer overflow detected in SecureBuffer\n");
            DebugBreak();
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// 2. ROBUST STREAM READER (Resists truncation attacks, EOF confusion)
// ═════════════════════════════════════════════════════════════════════════════

class RobustStream {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
    uint64_t file_size = 0;
    uint64_t pos = 0;
    const uint8_t* mapped = nullptr;
    bool is_mmapped = false;
    
public:
    // RAII wrapper for memory-mapped files with fallback to buffered I/O
    explicit RobustStream(const wchar_t* path) {
        hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "[RobustStream] CreateFileW failed: %lu\n", GetLastError());
            return;
        }
        
        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile, &size)) {
            fprintf(stderr, "[RobustStream] GetFileSizeEx failed: %lu\n", GetLastError());
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
            return;
        }
        file_size = size.QuadPart;
        
        // Try memory mapping for files < 4GB (safer), buffered for huge files
        if (file_size < (1ULL << 32)) {
            hMapping = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
            if (hMapping) {
                mapped = static_cast<const uint8_t*>(
                    MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0)
                );
                if (mapped) is_mmapped = true;
            }
        }
    }
    
    // Bounds-checked read with poisoned buffer on failure
    bool read(void* dst, uint64_t len, bool poison_on_fail = true) {
        if (pos + len > file_size) {
            if (poison_on_fail) memset(dst, 0xDD, static_cast<size_t>(len)); // 0xDD = dead
            return false;
        }
        
        if (is_mmapped) {
            memcpy(dst, mapped + pos, static_cast<size_t>(len));
        } else {
            DWORD read_bytes = 0;
            if (!ReadFile(hFile, dst, static_cast<DWORD>(len), &read_bytes, nullptr) || 
                read_bytes != len) {
                if (poison_on_fail) memset(dst, 0xDD, static_cast<size_t>(len));
                return false;
            }
        }
        pos += len;
        return true;
    }
    
    // Fast skip without read (for large data)
    bool skip(uint64_t len) {
        if (pos + len > file_size) return false;
        pos += len;
        if (!is_mmapped) {
            LARGE_INTEGER move;
            move.QuadPart = static_cast<LONGLONG>(pos);
            SetFilePointerEx(hFile, move, nullptr, FILE_BEGIN);
        }
        return true;
    }
    
    // Peek at next N bytes without consuming
    bool peek(void* dst, uint64_t len) const {
        if (pos + len > file_size) return false;
        if (is_mmapped) {
            memcpy(dst, mapped + pos, static_cast<size_t>(len));
        } else {
            // For buffered, we'd need caching - simplified here
            return false;
        }
        return true;
    }
    
    uint64_t tell() const { return pos; }
    uint64_t size() const { return file_size; }
    bool eof() const { return pos >= file_size; }
    
    ~RobustStream() {
        if (mapped) UnmapViewOfFile(mapped);
        if (hMapping) CloseHandle(hMapping);
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// 3. GGUF METADATA PARSER (Resists bad_alloc via preflight checks)
// ═════════════════════════════════════════════════════════════════════════════

struct GGUFValue {
    enum Type : uint32_t {
        UINT8 = 0, INT8 = 1, UINT16 = 2, INT16 = 3,
        UINT32 = 4, INT32 = 5, FLOAT32 = 6, BOOL = 7,
        STRING = 8, ARRAY = 9, UINT64 = 10, INT64 = 11,
        FLOAT64 = 12
    };
    
    Type type;
    union {
        uint64_t u64;
        int64_t  i64;
        double   f64;
        // Strings/arrays stored in external SecureBuffer to avoid stack overflow
    } scalar;
    SecureBuffer* heap_data = nullptr; // For strings/arrays
    
    ~GGUFValue() { delete heap_data; }
};

class RobustGGUFParser {
    RobustStream& stream;
    SecureBuffer key_buffer;
    static constexpr uint64_t MAX_KEY_LEN = 1024;      // 1KB max key
    static constexpr uint64_t MAX_STRING_LEN = 100ULL * 1024 * 1024; // 100MB safety
    static constexpr uint64_t MAX_ARRAY_ELEMS = 1000000; // 1M elements
    
public:
    explicit RobustGGUFParser(RobustStream& s) : stream(s) {}
    
    // Parse single key-value pair with full bounds checking
    bool parse_metadata_entry(char* key_out, size_t key_max, GGUFValue& value_out) {
        // Read key length (uint32_t in GGUF v3)
        uint32_t key_len = 0;
        if (!stream.read(&key_len, 4)) return false;
        
        if (key_len > MAX_KEY_LEN) {
            fprintf(stderr, "[GGUF] Suspicious key length: %u, skipping\n", key_len);
            stream.skip(key_len);
            return parse_metadata_entry(key_out, key_max, value_out); // Recurse to next
        }
        
        // Read key name (with null terminator safety)
        if (key_len + 1 > key_max) {
            stream.skip(key_len);
            return false;
        }
        if (!stream.read(key_out, key_len)) return false;
        key_out[key_len] = '\0';
        
        // Read value type
        GGUFValue::Type vtype;
        if (!stream.read(&vtype, 4)) return false;
        value_out.type = vtype;
        
        // Type-specific parsing with allocation guards
        switch (vtype) {
            case GGUFValue::STRING: return parse_string(value_out);
            case GGUFValue::ARRAY:  return parse_array(value_out);
            case GGUFValue::UINT64:
            case GGUFValue::INT64:
            case GGUFValue::FLOAT64:
                return stream.read(&value_out.scalar.u64, 8);
            default: // 32-bit and smaller
                return stream.read(&value_out.scalar.u64, 4);
        }
    }
    
private:
    bool parse_string(GGUFValue& val) {
        uint64_t str_len = 0;
        if (!stream.read(&str_len, 8)) return false;
        
        if (str_len > MAX_STRING_LEN) {
            fprintf(stderr, "[GGUF] Oversized string (%llu bytes), lazy-skipping\n", str_len);
            stream.skip(str_len);
            val.heap_data = new SecureBuffer(32);
            const char* msg = "[skipped: oversized string]";
            val.heap_data->append(msg, strlen(msg));
            return true;
        }
        
        val.heap_data = new SecureBuffer(str_len + 1);
        if (!stream.read(val.heap_data->data, str_len)) {
            delete val.heap_data;
            val.heap_data = nullptr;
            return false;
        }
        val.heap_data->size = str_len;
        val.heap_data->data[str_len] = '\0';
        return true;
    }
    
    bool parse_array(GGUFValue& val) {
        GGUFValue::Type elem_type;
        uint64_t count = 0;
        
        if (!stream.read(&elem_type, 4) || !stream.read(&count, 8)) return false;
        
        if (count > MAX_ARRAY_ELEMS) {
            fprintf(stderr, "[GGUF] Array too large (%llu elems), skipping\n", count);
            // Skip without reading - calculate bytes to skip
            uint64_t elem_size = get_type_size(elem_type);
            stream.skip(count * elem_size);
            return true;
        }
        
        // For string arrays (like merges), skip entirely if too many
        if (elem_type == GGUFValue::STRING && count > 100000) {
            fprintf(stderr, "[GGUF] Large string array (%llu elems), skipping\n", count);
            for (uint64_t i = 0; i < count; i++) {
                uint64_t len = 0;
                stream.read(&len, 8);
                stream.skip(len);
            }
            return true;
        }
        
        // Store raw array bytes in secure buffer
        uint64_t total_bytes = count * get_type_size(elem_type);
        val.heap_data = new SecureBuffer(total_bytes);
        val.heap_data->size = total_bytes;
        
        return stream.read(val.heap_data->data, total_bytes);
    }
    
    static uint64_t get_type_size(GGUFValue::Type t) {
        switch (t) {
            case GGUFValue::UINT8:
            case GGUFValue::INT8: return 1;
            case GGUFValue::UINT16:
            case GGUFValue::INT16: return 2;
            case GGUFValue::UINT32:
            case GGUFValue::INT32:
            case GGUFValue::FLOAT32: return 4;
            case GGUFValue::UINT64:
            case GGUFValue::INT64:
            case GGUFValue::FLOAT64: return 8;
            case GGUFValue::BOOL: return 1;
            case GGUFValue::STRING: return 0; // Variable
            default: return 1;
        }
    }
};

// ═════════════════════════════════════════════════════════════════════════════
// 4. LAZY TENSOR MAPPER (Map-on-demand, export for MASM bridge)
// ═════════════════════════════════════════════════════════════════════════════

extern "C" __declspec(dllexport) void* RawrXD_MapTensorLazy(
    const wchar_t* gguf_path,
    uint64_t tensor_offset,
    uint64_t tensor_size,
    uint32_t* out_error
) {
    if (!out_error) return nullptr;
    *out_error = 0;
    
    // Open file with sequential scan hint (no cache pollution)
    HANDLE hFile = CreateFileW(gguf_path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        *out_error = GetLastError();
        return nullptr;
    }
    
    // Create mapping with write-copy protection (resists modification attacks)
    HANDLE hMap = CreateFileMapping(hFile, nullptr, PAGE_WRITECOPY, 
                                    static_cast<DWORD>(tensor_offset >> 32), 
                                    static_cast<DWORD>(tensor_offset & 0xFFFFFFFF), 
                                    nullptr);
    if (!hMap) {
        *out_error = GetLastError();
        CloseHandle(hFile);
        return nullptr;
    }
    
    // Map only the specific tensor region (granular, not whole file)
    void* ptr = MapViewOfFile(hMap, FILE_MAP_COPY, 
                             static_cast<DWORD>(tensor_offset >> 32), 
                             static_cast<DWORD>(tensor_offset & 0xFFFFFFFF),
                             static_cast<SIZE_T>(tensor_size));
    
    // Close handles immediately - view persists
    CloseHandle(hMap);
    CloseHandle(hFile);
    
    if (!ptr) *out_error = GetLastError();
    return ptr;
}

// ═════════════════════════════════════════════════════════════════════════════
// 5. DIAGNOSTIC DUMPER (Reverse-engineered format inspector)
// ═════════════════════════════════════════════════════════════════════════════

inline void DumpGGUFSafe(const wchar_t* path) {
    RobustStream rs(path);
    if (!rs.size()) {
        printf("Failed to open: %ls\n", path);
        return;
    }
    
    // Verify magic (GGUF = 0x46554747 little endian)
    uint32_t magic = 0;
    if (!rs.read(&magic, 4) || magic != 0x46554747) {
        printf("Invalid GGUF magic: 0x%08X\n", magic);
        return;
    }
    
    uint32_t version = 0;
    rs.read(&version, 4);
    printf("GGUF Version: %u\n", version);
    
    uint64_t n_tensors = 0, n_kv = 0;
    rs.read(&n_tensors, 8);
    rs.read(&n_kv, 8);
    printf("Tensors: %llu, Metadata pairs: %llu\n", n_tensors, n_kv);
    
    RobustGGUFParser parser(rs);
    char key[1024];
    GGUFValue val;
    
    for (uint64_t i = 0; i < n_kv && i < 1000; i++) { // Safety cap
        if (!parser.parse_metadata_entry(key, sizeof(key), val)) {
            printf("Parse error at entry %llu\n", i);
            break;
        }
        
        printf("%s = ", key);
        if (val.type == GGUFValue::STRING && val.heap_data) {
            printf("(string, %zu bytes) \"%.50s%s\"\n", 
                   val.heap_data->size,
                   val.heap_data->data,
                   val.heap_data->size > 50 ? "..." : "");
        } else if (val.type == GGUFValue::ARRAY) {
            printf("(array)\n");
        } else {
            printf("%llu\n", val.scalar.u64);
        }
    }
}

} // namespace rawrxd::robust
