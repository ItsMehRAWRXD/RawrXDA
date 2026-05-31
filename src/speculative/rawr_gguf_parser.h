// rawr_gguf_parser.h — Bulletproof GGUF metadata + tensor index parser
// Handles: all architectures, string safety, alignment, mmap, error recovery
// Zero external deps, C++17, ~350 lines

#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#define MMAP_WINDOWS
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace rawr {

// ─── GGUF Types ──────────────────────────────────────────────────────────────
enum class GGUFType : uint32_t {
    UINT8 = 0, INT8 = 1, UINT16 = 2, INT16 = 3, UINT32 = 4, INT32 = 5,
    FLOAT32 = 6, BOOL = 7, STRING = 8, ARRAY = 9, UINT64 = 10, INT64 = 11,
    FLOAT64 = 12
};

static inline size_t gguf_type_size(GGUFType t) {
    switch (t) {
        case GGUFType::UINT8:  case GGUFType::INT8:   return 1;
        case GGUFType::UINT16: case GGUFType::INT16:  return 2;
        case GGUFType::UINT32: case GGUFType::INT32:  case GGUFType::FLOAT32: return 4;
        case GGUFType::UINT64: case GGUFType::INT64:  case GGUFType::FLOAT64: return 8;
        case GGUFType::BOOL:   return 1;
        default: return 0;
    }
}

// ─── Mmap File ───────────────────────────────────────────────────────────────
struct MmapFile {
    uint8_t* data = nullptr;
    size_t size = 0;
#ifdef _WIN32
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = nullptr;
#else
    int fd = -1;
#endif

    bool open(const char* path) {
#ifdef _WIN32
        hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER sz; GetFileSizeEx(hFile, &sz);
        size = (size_t)sz.QuadPart;
        hMap = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMap) { CloseHandle(hFile); hFile = INVALID_HANDLE_VALUE; return false; }
        data = (uint8_t*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, size);
        if (!data) { CloseHandle(hMap); CloseHandle(hFile); hMap = nullptr; hFile = INVALID_HANDLE_VALUE; return false; }
#else
        fd = ::open(path, O_RDONLY);
        if (fd < 0) return false;
        struct stat st; fstat(fd, &st); size = st.st_size;
        data = (uint8_t*)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (data == MAP_FAILED) { ::close(fd); fd = -1; return false; }
#endif
        return true;
    }
    void close() {
        if (!data) return;
#ifdef _WIN32
        UnmapViewOfFile(data); data = nullptr;
        if (hMap) { CloseHandle(hMap); hMap = nullptr; }
        if (hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = INVALID_HANDLE_VALUE; }
#else
        munmap(data, size); data = nullptr;
        if (fd >= 0) { ::close(fd); fd = -1; }
#endif
    }
    ~MmapFile() { close(); }
};

// ─── Safe Binary Reader ──────────────────────────────────────────────────────
struct GGUFReader {
    const uint8_t* base = nullptr;
    const uint8_t* end = nullptr;
    const uint8_t* cur = nullptr;
    bool ok = true;

    void init(const uint8_t* b, size_t sz) { base = b; end = b + sz; cur = b; ok = true; }

    bool check(size_t n) {
        if (!ok || (size_t)(end - cur) < n) { ok = false; return false; }
        return true;
    }

    template<typename T>
    T read() {
        T v{};
        if (!check(sizeof(T))) return v;
        memcpy(&v, cur, sizeof(T));
        cur += sizeof(T);
        return v;
    }

    uint64_t read_u64() { return read<uint64_t>(); }
    uint32_t read_u32() { return read<uint32_t>(); }
    uint16_t read_u16() { return read<uint16_t>(); }
    uint8_t  read_u8()  { return read<uint8_t>(); }
    float    read_f32() { return read<float>(); }
    double   read_f64() { return read<double>(); }

    std::string read_string() {
        uint64_t len = read_u64();
        if (!ok || len > 4096) { ok = false; return ""; } // sanity cap
        if (!check(len)) return "";
        std::string s((const char*)cur, (size_t)len);
        cur += len;
        return s;
    }

    void skip_value(GGUFType type) {
        switch (type) {
            case GGUFType::UINT8: case GGUFType::INT8: case GGUFType::BOOL:
                check(1); cur += 1; break;
            case GGUFType::UINT16: case GGUFType::INT16:
                check(2); cur += 2; break;
            case GGUFType::UINT32: case GGUFType::INT32: case GGUFType::FLOAT32:
                check(4); cur += 4; break;
            case GGUFType::UINT64: case GGUFType::INT64: case GGUFType::FLOAT64:
                check(8); cur += 8; break;
            case GGUFType::STRING:
                read_string(); break;
            case GGUFType::ARRAY: {
                GGUFType etype = (GGUFType)read_u32();
                uint64_t count = read_u64();
                size_t esz = gguf_type_size(etype);
                if (esz > 0 && count < 0x10000000) {
                    check(esz * count);
                    cur += esz * count;
                } else if (etype == GGUFType::STRING) {
                    for (uint64_t i = 0; i < count && ok; ++i) read_string();
                } else if (etype == GGUFType::ARRAY) {
                    for (uint64_t i = 0; i < count && ok; ++i) skip_value(GGUFType::ARRAY);
                }
                break;
            }
        }
    }
};

// ─── Tensor Info ─────────────────────────────────────────────────────────────
struct TensorInfo {
    std::string name;
    uint32_t n_dims = 0;
    uint64_t dims[4] = {0};
    GGUFType type = GGUFType::FLOAT32;
    uint64_t offset = 0;       // offset from data start (after tensor table)
    uint64_t file_offset = 0;  // absolute offset in file
    size_t size_bytes = 0;
};

// ─── Model Config ────────────────────────────────────────────────────────────
struct ModelConfig {
    std::string arch = "unknown";
    uint32_t vocab_size = 32000;
    uint32_t hidden_size = 4096;
    uint32_t num_layers = 32;
    uint32_t num_heads = 32;
    uint32_t num_kv_heads = 32;
    uint32_t intermediate_size = 11008;
    uint32_t max_seq_len = 2048;
    float rms_norm_eps = 1e-6f;
    float rope_theta = 10000.0f;
    bool tie_word_embeddings = false;
};

// ─── GGUF Parser ─────────────────────────────────────────────────────────────
struct GGUFParsed {
    ModelConfig config;
    std::vector<TensorInfo> tensors;
    uint64_t data_offset = 0;  // where tensor data starts
    const uint8_t* data_ptr = nullptr;
    bool valid = false;
    std::string error;
};

// Architecture key mapping: try arch-prefixed keys first, then llama fallback
static uint32_t get_kv_u32(const std::unordered_map<std::string, std::string>& kv,
                           const std::string& arch, const char* key, uint32_t fallback) {
    auto it = kv.find(arch + "." + key);
    if (it != kv.end()) return (uint32_t)std::stoull(it->second);
    it = kv.find(std::string("llama.") + key);
    if (it != kv.end()) return (uint32_t)std::stoull(it->second);
    return fallback;
}
static float get_kv_f32(const std::unordered_map<std::string, std::string>& kv,
                        const std::string& arch, const char* key, float fallback) {
    auto it = kv.find(arch + "." + key);
    if (it != kv.end()) return std::stof(it->second);
    it = kv.find(std::string("llama.") + key);
    if (it != kv.end()) return std::stof(it->second);
    return fallback;
}

inline GGUFParsed parse_gguf(const char* path) {
    GGUFParsed result;
    MmapFile mmap;
    if (!mmap.open(path)) {
        result.error = "Failed to mmap file"; return result;
    }

    GGUFReader r;
    r.init(mmap.data, mmap.size);

    // Header
    uint32_t magic = r.read_u32();
    if (magic != 0x46554747) { // "GGUF" little-endian
        result.error = "Invalid magic"; return result;
    }
    uint32_t version = r.read_u32();
    if (version != 2 && version != 3) {
        result.error = "Unsupported version"; return result;
    }
    uint64_t n_tensors = r.read_u64();
    uint64_t n_kv = r.read_u64();

    if (n_tensors > 10000 || n_kv > 10000) {
        result.error = "Suspicious tensor/kv count"; return result;
    }

    // ── Parse KV pairs ───────────────────────────────────────────────────────
    std::unordered_map<std::string, std::string> kv_str;
    std::string arch_name = "llama";

    for (uint64_t i = 0; i < n_kv && r.ok; ++i) {
        std::string key = r.read_string();
        if (!r.ok) break;
        GGUFType vtype = (GGUFType)r.read_u32();
        if (!r.ok) break;

        // Store strings directly, convert scalars to string
        if (vtype == GGUFType::STRING) {
            std::string val = r.read_string();
            kv_str[key] = val;
            if (key == "general.architecture") arch_name = val;
        } else if (vtype == GGUFType::UINT32 || vtype == GGUFType::INT32) {
            kv_str[key] = std::to_string(r.read_u32());
        } else if (vtype == GGUFType::UINT64 || vtype == GGUFType::INT64) {
            kv_str[key] = std::to_string(r.read_u64());
        } else if (vtype == GGUFType::FLOAT32) {
            kv_str[key] = std::to_string(r.read_f32());
        } else if (vtype == GGUFType::FLOAT64) {
            kv_str[key] = std::to_string(r.read_f64());
        } else if (vtype == GGUFType::BOOL) {
            kv_str[key] = r.read_u8() ? "1" : "0";
        } else if (vtype == GGUFType::UINT8 || vtype == GGUFType::INT8) {
            kv_str[key] = std::to_string((int)r.read_u8());
        } else if (vtype == GGUFType::UINT16 || vtype == GGUFType::INT16) {
            kv_str[key] = std::to_string(r.read_u16());
        } else {
            r.skip_value(vtype); // skip arrays and unknown
        }
    }

    if (!r.ok) { result.error = "KV parse failed"; return result; }

    // ── Build config with architecture-aware key lookup ──────────────────────
    result.config.arch = arch_name;
    result.config.vocab_size = get_kv_u32(kv_str, arch_name, "vocab_size", 32000);
    result.config.hidden_size = get_kv_u32(kv_str, arch_name, "embedding_length", 4096);
    result.config.num_layers = get_kv_u32(kv_str, arch_name, "block_count", 32);
    result.config.num_heads = get_kv_u32(kv_str, arch_name, "attention.head_count", 32);
    result.config.num_kv_heads = get_kv_u32(kv_str, arch_name, "attention.head_count_kv", result.config.num_heads);
    result.config.intermediate_size = get_kv_u32(kv_str, arch_name, "feed_forward_length",
        result.config.hidden_size * 4); // sensible default
    result.config.max_seq_len = get_kv_u32(kv_str, arch_name, "context_length", 2048);
    result.config.rms_norm_eps = get_kv_f32(kv_str, arch_name, "attention.layer_norm_rms_epsilon", 1e-6f);
    result.config.rope_theta = get_kv_f32(kv_str, arch_name, "rope.freq_base", 10000.0f);
    auto it = kv_str.find("general.tie_word_embeddings");
    if (it != kv_str.end()) result.config.tie_word_embeddings = (it->second == "1");

    // ── Parse tensor info ────────────────────────────────────────────────────
    uint64_t tensor_table_end = 0;
    for (uint64_t i = 0; i < n_tensors && r.ok; ++i) {
        TensorInfo ti;
        ti.name = r.read_string();
        if (!r.ok || ti.name.empty()) { r.ok = false; break; }

        ti.n_dims = r.read_u32();
        if (ti.n_dims > 4 || ti.n_dims == 0) { r.ok = false; break; }

        for (uint32_t d = 0; d < ti.n_dims; ++d) {
            ti.dims[d] = r.read_u64();
        }
        ti.type = (GGUFType)r.read_u32();
        ti.offset = r.read_u64();

        // Compute size
        size_t type_size = gguf_type_size(ti.type);
        size_t block_size = 1;
        if ((int)ti.type >= 2 && (int)ti.type <= 15) { // quantized types
            // Q4_0 = 2, Q4_1 = 3, Q5_0 = 6, Q5_1 = 7, Q8_0 = 8, Q8_1 = 9
            // These use 32-element blocks
            if ((int)ti.type == 2 || (int)ti.type == 3) block_size = 32; // Q4_0/Q4_1
            else if ((int)ti.type == 6 || (int)ti.type == 7) block_size = 32; // Q5_0/Q5_1
            else if ((int)ti.type == 8 || (int)ti.type == 9) block_size = 32; // Q8_0/Q8_1
            else if ((int)ti.type >= 10 && (int)ti.type <= 15) block_size = 256; // K-quants
        }
        size_t n_elements = 1;
        for (uint32_t d = 0; d < ti.n_dims; ++d) n_elements *= (size_t)ti.dims[d];
        if (block_size > 1) {
            size_t n_blocks = (n_elements + block_size - 1) / block_size;
            size_t bytes_per_block = (block_size / 2); // Q4 = 4 bits per weight
            if ((int)ti.type == 2 || (int)ti.type == 3) bytes_per_block = block_size / 2 + 2; // +scale (fp16)
            if ((int)ti.type == 3) bytes_per_block += 2; // Q4_1 has min too
            if ((int)ti.type == 6 || (int)ti.type == 7) bytes_per_block = block_size / 2 + 2; // Q5
            if ((int)ti.type == 7) bytes_per_block += 2;
            if ((int)ti.type == 8) bytes_per_block = block_size + 2; // Q8_0
            if ((int)ti.type == 9) bytes_per_block = block_size + 4; // Q8_1
            ti.size_bytes = n_blocks * bytes_per_block;
        } else {
            ti.size_bytes = n_elements * type_size;
        }

        result.tensors.push_back(ti);
        uint64_t abs_end = ti.offset + ti.size_bytes;
        if (abs_end > tensor_table_end) tensor_table_end = abs_end;
    }

    if (!r.ok) { result.error = "Tensor parse failed at " + std::to_string(r.cur - r.base); return result; }

    // ── Compute data offset with alignment ───────────────────────────────────
    result.data_offset = (uint64_t)(r.cur - r.base);
    uint64_t align = 32; // GGUF alignment
    result.data_offset = (result.data_offset + align - 1) & ~(align - 1);
    result.data_ptr = r.base + result.data_offset;

    // Validate all tensor offsets fit in file
    for (auto& ti : result.tensors) {
        ti.file_offset = result.data_offset + ti.offset;
        if (ti.file_offset + ti.size_bytes > mmap.size) {
            result.error = "Tensor '" + ti.name + "' extends past file end";
            return result;
        }
    }

    result.valid = true;
    mmap.close();
    return result;
}

// ─── Quantized Block Types ───────────────────────────────────────────────────
struct block_q4_0 {
    uint16_t d;      // scale (fp16)
    uint8_t qs[16];  // 32 4-bit weights
};
static_assert(sizeof(block_q4_0) == 18, "q4_0 block size");

struct block_q4_1 {
    uint16_t d;      // scale
    uint16_t m;      // min
    uint8_t qs[16];  // 32 weights
};
static_assert(sizeof(block_q4_1) == 20, "q4_1 block size");

// ─── Dequantization ──────────────────────────────────────────────────────────
inline void dequantize_q4_0_row(const uint8_t* src, float* dst, size_t n) {
    const block_q4_0* blocks = (const block_q4_0*)src;
    size_t nb = (n + 31) / 32;
    for (size_t b = 0; b < nb; ++b) {
        float d = ((const uint16_t*)&blocks[b].d)[0];
        // fp16 to fp32 (simple, no denormals)
        uint32_t h = blocks[b].d;
        uint32_t sign = (h >> 15) & 1;
        uint32_t exp = (h >> 10) & 0x1F;
        uint32_t mant = h & 0x3FF;
        if (exp == 0) { d = 0; }
        else {
            uint32_t f32 = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
            memcpy(&d, &f32, 4);
        }
        for (int i = 0; i < 16; ++i) {
            uint8_t q = blocks[b].qs[i];
            int x0 = (q & 0x0F) - 8;
            int x1 = (q >> 4) - 8;
            if (b * 32 + i * 2 < n) dst[b * 32 + i * 2] = x0 * d;
            if (b * 32 + i * 2 + 1 < n) dst[b * 32 + i * 2 + 1] = x1 * d;
        }
    }
}

inline void dequantize_q4_1_row(const uint8_t* src, float* dst, size_t n) {
    const block_q4_1* blocks = (const block_q4_1*)src;
    size_t nb = (n + 31) / 32;
    for (size_t b = 0; b < nb; ++b) {
        float d, m;
        uint32_t h = blocks[b].d; 
        // fp16→fp32 for d
        uint32_t sign = (h >> 15) & 1, exp = (h >> 10) & 0x1F, mant = h & 0x3FF;
        if (exp == 0) d = 0;
        else { uint32_t f = (sign<<31)|((exp+112)<<23)|(mant<<13); memcpy(&d,&f,4); }
        h = blocks[b].m;
        sign = (h >> 15) & 1; exp = (h >> 10) & 0x1F; mant = h & 0x3FF;
        if (exp == 0) m = 0;
        else { uint32_t f = (sign<<31)|((exp+112)<<23)|(mant<<13); memcpy(&m,&f,4); }
        for (int i = 0; i < 16; ++i) {
            uint8_t q = blocks[b].qs[i];
            int x0 = (q & 0x0F);
            int x1 = (q >> 4);
            if (b * 32 + i * 2 < n) dst[b * 32 + i * 2] = x0 * d + m;
            if (b * 32 + i * 2 + 1 < n) dst[b * 32 + i * 2 + 1] = x1 * d + m;
        }
    }
}

// ─── Get tensor data pointer ─────────────────────────────────────────────────
inline const uint8_t* get_tensor_data(const GGUFParsed& gguf, const char* name) {
    for (const auto& ti : gguf.tensors) {
        if (ti.name == name) return gguf.data_ptr + ti.offset;
    }
    return nullptr;
}

inline bool has_tensor(const GGUFParsed& gguf, const char* name) {
    for (const auto& ti : gguf.tensors) if (ti.name == name) return true;
    return false;
}

} // namespace rawr
