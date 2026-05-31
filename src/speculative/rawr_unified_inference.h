// rawr_unified_inference.h
// Unified inference engine combining parser, runtime, and kernels

#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace rawr {

// ============================================================================
// QUANTIZATION BLOCKS
// ============================================================================

struct block_q8_0 {
    uint16_t d;
    int8_t qs[32];
};
static_assert(sizeof(block_q8_0) == 34, "q8_0 block size");

struct block_q4_0 {
    uint16_t d;
    uint8_t qs[16];
};
static_assert(sizeof(block_q4_0) == 18, "q4_0 block size");

// ============================================================================
// FP16 CONVERSION
// ============================================================================

inline float fp16_to_fp32(uint16_t h) {
    uint32_t sign = (h >> 15) & 0x1;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    if (exp == 0) {
        if (mant == 0) return 0.0f;
        float val = mant / 1024.0f;
        return sign ? -val * 6.1035e-5f : val * 6.1035e-5f;
    }
    if (exp == 0x1F) {
        return mant ? NAN : (sign ? -INFINITY : INFINITY);
    }
    
    uint32_t f32 = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
    float result;
    memcpy(&result, &f32, 4);
    return result;
}

// ============================================================================
// DEQUANTIZATION
// ============================================================================

inline void dequantize_q8_0_row(const uint8_t* src, float* dst, size_t n) {
    const block_q8_0* blocks = (const block_q8_0*)src;
    size_t nb = (n + 31) / 32;
    
    for (size_t b = 0; b < nb; b++) {
        float d = fp16_to_fp32(blocks[b].d);
        for (int i = 0; i < 32; i++) {
            size_t idx = b * 32 + i;
            if (idx < n) dst[idx] = blocks[b].qs[i] * d;
        }
    }
}

inline void dequantize_q4_0_row(const uint8_t* src, float* dst, size_t n) {
    const block_q4_0* blocks = (const block_q4_0*)src;
    size_t nb = (n + 31) / 32;
    
    for (size_t b = 0; b < nb; b++) {
        float d = fp16_to_fp32(blocks[b].d);
        for (int i = 0; i < 16; i++) {
            uint8_t q = blocks[b].qs[i];
            int x0 = (q & 0x0F) - 8;
            int x1 = (q >> 4) - 8;
            
            size_t idx0 = b * 32 + i * 2;
            size_t idx1 = idx0 + 1;
            
            if (idx0 < n) dst[idx0] = x0 * d;
            if (idx1 < n) dst[idx1] = x1 * d;
        }
    }
}

// ============================================================================
// MATH OPERATIONS
// ============================================================================

inline float vec_dot(const float* a, const float* b, size_t n) {
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

inline void matmul_vec(const float* x, const float* W, float* y, 
                       size_t n_rows, size_t n_cols) {
    for (size_t i = 0; i < n_rows; i++) {
        y[i] = vec_dot(x, W + i * n_cols, n_cols);
    }
}

inline float silu(float x) {
    return x / (1.0f + expf(-x));
}

inline void softmax(float* x, size_t n) {
    float max_val = x[0];
    for (size_t i = 1; i < n; i++) if (x[i] > max_val) max_val = x[i];
    
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        x[i] = expf(x[i] - max_val);
        sum += x[i];
    }
    for (size_t i = 0; i < n; i++) x[i] /= sum;
}

inline void rmsnorm(const float* x, const float* weight, float* out, 
                    size_t n, float eps = 1e-5f) {
    float sum_sq = 0.0f;
    for (size_t i = 0; i < n; i++) sum_sq += x[i] * x[i];
    float scale = 1.0f / sqrtf(sum_sq / n + eps);
    for (size_t i = 0; i < n; i++) out[i] = x[i] * scale * weight[i];
}

inline void apply_rope(float* q, float* k, size_t head_dim, size_t pos, float theta = 10000.0f) {
    for (size_t i = 0; i < head_dim; i += 2) {
        float freq = 1.0f / powf(theta, (float)i / head_dim);
        float val = pos * freq;
        float cos_val = cosf(val);
        float sin_val = sinf(val);
        
        float q0 = q[i], q1 = q[i + 1];
        q[i] = q0 * cos_val - q1 * sin_val;
        q[i + 1] = q0 * sin_val + q1 * cos_val;
        
        float k0 = k[i], k1 = k[i + 1];
        k[i] = k0 * cos_val - k1 * sin_val;
        k[i + 1] = k0 * sin_val + k1 * cos_val;
    }
}

inline int sample_greedy(const float* logits, size_t vocab_size) {
    int best_idx = 0;
    float best_val = logits[0];
    for (size_t i = 1; i < vocab_size; i++) {
        if (logits[i] > best_val) {
            best_val = logits[i];
            best_idx = i;
        }
    }
    return best_idx;
}

// ============================================================================
// MODEL CONFIG
// ============================================================================

struct ModelConfig {
    int vocab_size = 32000;
    int hidden_dim = 4096;
    int n_layers = 32;
    int n_heads = 32;
    int n_kv_heads = 32;
    int head_dim = 128;
    int ffn_dim = 11008;
    float rms_norm_eps = 1e-5f;
    float rope_theta = 10000.0f;
    int max_seq_len = 4096;
    int sliding_window = 0;
    int n_experts = 0;
    int n_experts_per_token = 0;
    
    bool is_gqa() const { return n_kv_heads < n_heads; }
    bool is_mqa() const { return n_kv_heads == 1; }
    bool is_mha() const { return n_kv_heads == n_heads; }
    bool is_moe() const { return n_experts > 0; }
};

// Phi-3-mini config
inline ModelConfig config_phi3mini() {
    ModelConfig cfg;
    cfg.vocab_size = 32064;
    cfg.hidden_dim = 3072;
    cfg.n_layers = 32;
    cfg.n_heads = 32;
    cfg.n_kv_heads = 32;
    cfg.head_dim = 96;
    cfg.ffn_dim = 8192;
    cfg.rms_norm_eps = 1e-5f;
    cfg.rope_theta = 10000.0f;
    cfg.max_seq_len = 4096;
    return cfg;
}

// ministral3 config
inline ModelConfig config_ministral3() {
    ModelConfig cfg;
    cfg.vocab_size = 131072;
    cfg.hidden_dim = 4096;
    cfg.n_layers = 34;
    cfg.n_heads = 32;
    cfg.n_kv_heads = 8;
    cfg.head_dim = 128;
    cfg.ffn_dim = 14336;
    cfg.rms_norm_eps = 1e-5f;
    cfg.rope_theta = 1000000.0f;
    cfg.max_seq_len = 131072;
    return cfg;
}

// gptoss20b config
inline ModelConfig config_gptoss20b() {
    ModelConfig cfg;
    cfg.vocab_size = 32000;
    cfg.hidden_dim = 2880;
    cfg.n_layers = 24;
    cfg.n_heads = 64;
    cfg.n_kv_heads = 8;
    cfg.head_dim = 64;
    cfg.ffn_dim = 2880;
    cfg.rms_norm_eps = 1e-5f;
    cfg.rope_theta = 150000.0f;
    cfg.max_seq_len = 8192;
    cfg.sliding_window = 128;
    cfg.n_experts = 32;
    cfg.n_experts_per_token = 4;
    return cfg;
}

// ============================================================================
// SIMPLE GGUF PARSER (minimal)
// ============================================================================

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

struct TensorInfo {
    std::string name;
    uint32_t n_dims = 0;
    uint64_t dims[4] = {0};
    uint32_t type = 0;
    uint64_t offset = 0;
    uint64_t file_offset = 0;
    size_t size_bytes = 0;
};

struct ParsedGGUF {
    bool valid = false;
    std::string error;
    ModelConfig config;
    std::vector<TensorInfo> tensors;
    uint64_t data_offset = 0;
    const uint8_t* data_ptr = nullptr;
    MmapFile mmap;
};

inline ParsedGGUF parse_gguf_simple(const char* path) {
    ParsedGGUF result;
    if (!result.mmap.open(path)) {
        result.error = "Failed to mmap file";
        return result;
    }
    
    const uint8_t* data = result.mmap.data;
    size_t size = result.mmap.size;
    size_t cursor = 0;
    
    auto check = [&](size_t n) {
        if (cursor + n > size) {
            result.error = "Unexpected EOF at cursor " + std::to_string(cursor);
            return false;
        }
        return true;
    };
    
    auto read_u32 = [&]() {
        if (!check(4)) return 0u;
        uint32_t v; memcpy(&v, data + cursor, 4); cursor += 4; return v;
    };
    auto read_u64 = [&]() {
        if (!check(8)) return 0ull;
        uint64_t v; memcpy(&v, data + cursor, 8); cursor += 8; return v;
    };
    auto read_f32 = [&]() {
        if (!check(4)) return 0.0f;
        float v; memcpy(&v, data + cursor, 4); cursor += 4; return v;
    };
    auto read_string = [&]() {
        uint64_t len = read_u64();
        if (len > 10000 || !check(len)) {
            return std::string(); // Invalid length
        }
        std::string s((const char*)data + cursor, len);
        cursor += len;
        return s;
    };
    
    // Header
    uint32_t magic = read_u32();
    if (magic != 0x46554747) { result.error = "Invalid magic"; return result; }
    uint32_t version = read_u32();
    if (version != 3) { result.error = "Only GGUF v3 supported"; return result; }
    uint64_t n_tensors = read_u64();
    uint64_t n_kv = read_u64();
    
    // Parse KV
    std::string arch_name = "llama";
    for (uint64_t i = 0; i < n_kv; i++) {
        std::string key = read_string();
        uint32_t type = read_u32();
        
        if (key == "general.architecture") {
            if (type == 8) arch_name = read_string();
        } else if (type == 4) { // uint32
            uint32_t v = read_u32();
            if (key.find("vocab_size") != std::string::npos) result.config.vocab_size = v;
            else if (key.find("embedding_length") != std::string::npos) result.config.hidden_dim = v;
            else if (key.find("block_count") != std::string::npos) result.config.n_layers = v;
            else if (key.find("attention.head_count") != std::string::npos && key.find("kv") == std::string::npos) result.config.n_heads = v;
            else if (key.find("attention.head_count_kv") != std::string::npos) result.config.n_kv_heads = v;
            else if (key.find("feed_forward_length") != std::string::npos) result.config.ffn_dim = v;
            else if (key.find("context_length") != std::string::npos) result.config.max_seq_len = v;
        } else if (type == 6) { // float32
            float v = read_f32();
            if (key.find("layer_norm_rms_epsilon") != std::string::npos) result.config.rms_norm_eps = v;
            else if (key.find("rope.freq_base") != std::string::npos) result.config.rope_theta = v;
        } else if (type == 8) { // string
            read_string();
        } else if (type == 9) { // array
            read_u32(); read_u64(); // skip
        } else {
            // Skip other types
            switch (type) {
                case 0: case 1: cursor += 1; break;
                case 2: case 3: cursor += 2; break;
                case 5: cursor += 4; break;
                case 10: case 11: cursor += 8; break;
                case 12: cursor += 8; break;
                default: break;
            }
        }
    }
    
    // Parse tensors
    for (uint64_t i = 0; i < n_tensors; i++) {
        TensorInfo ti;
        ti.name = read_string();
        ti.n_dims = read_u32();
        for (uint32_t d = 0; d < ti.n_dims; d++) ti.dims[d] = read_u64();
        ti.type = read_u32();
        ti.offset = read_u64();
        result.tensors.push_back(ti);
    }
    
    // Align to data section
    result.data_offset = (cursor + 31) & ~31ULL;
    result.data_ptr = data + result.data_offset;
    
    // Calculate file offsets
    for (auto& ti : result.tensors) {
        ti.file_offset = result.data_offset + ti.offset;
    }
    
    result.config.head_dim = result.config.hidden_dim / result.config.n_heads;
    result.valid = true;
    return result;
}

} // namespace rawr
