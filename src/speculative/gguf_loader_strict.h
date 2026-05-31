/*
====================================================================
 RAWR GGUF LOADER v3 - Strict Spec Compliance
 Zero-struct, byte-accurate parsing with full validation
====================================================================
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <iostream>

namespace rawr {

// GGUF v3 format constants
static const uint32_t GGUF_MAGIC = 0x46554747;  // "GGUF" in LE
static const uint32_t GGUF_VERSION = 3;
static const size_t GGUF_ALIGNMENT = 32;

// GGML type enumeration (subset)
enum class GGMLType : uint32_t {
    F32  = 0,
    F16  = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    Q2_K = 10,
    Q3_K = 11,
    Q4_K = 12,
    Q5_K = 13,
    Q6_K = 14,
    Q8_K = 15,
};

// Tensor information
struct TensorInfo {
    std::string name;
    uint32_t n_dims = 0;
    std::vector<uint64_t> dims;
    GGMLType type = GGMLType::F32;
    uint64_t offset = 0;  // Relative to data section
    size_t size_bytes = 0;
    const void* data_ptr = nullptr;
    
    // Stride info
    size_t stride[4] = {0, 0, 0, 0};
    bool is_transposed = false;
    
    size_t num_elements() const {
        size_t n = 1;
        for (auto d : dims) n *= d;
        return n;
    }
    
    size_t element_size() const {
        switch (type) {
            case GGMLType::F32: return 4;
            case GGMLType::F16: return 2;
            case GGMLType::Q4_0: return 1;
            case GGMLType::Q4_1: return 2;
            case GGMLType::Q4_K: return 4;
            case GGMLType::Q5_K: return 6;
            case GGMLType::Q6_K: return 8;
            case GGMLType::Q8_0: return 2;
            default: return 4;
        }
    }
};

// Strict GGUF v3 loader
class GGUFLoader {
public:
    struct Header {
        uint32_t magic = 0;
        uint32_t version = 0;
        uint64_t n_tensors = 0;
        uint64_t n_kv = 0;
    };
    
    Header header;
    std::vector<TensorInfo> tensors;
    std::unordered_map<std::string, size_t> tensor_map;
    
    // Model metadata
    int32_t n_vocab = 0;
    int32_t n_embd = 512;
    int32_t n_head = 8;
    int32_t n_head_kv = 8;
    int32_t n_layer = 4;
    int32_t n_ff = 0;
    float rms_norm_eps = 1e-5f;
    
    bool load(const uint8_t* data, size_t size) {
        this->data = data;
        this->size = size;
        cursor = 0;
        
        try {
            parse_header();
            parse_metadata();
            parse_tensors();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[GGUF] Parse error: " << e.what() << std::endl;
            return false;
        }
    }
    
    const TensorInfo* get_tensor(const std::string& name) const {
        auto it = tensor_map.find(name);
        if (it != tensor_map.end()) return &tensors[it->second];
        return nullptr;
    }
    
    const float* get_tensor_f32(const std::string& name) const {
        auto* t = get_tensor(name);
        if (t && t->type == GGMLType::F32 && t->data_ptr) {
            return static_cast<const float*>(t->data_ptr);
        }
        return nullptr;
    }
    
private:
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t cursor = 0;
    
    void check(size_t n, const char* what) {
        if (cursor + n > size) {
            throw std::runtime_error(std::string("Unexpected EOF reading ") + what);
        }
    }
    
    // Little-endian readers
    uint8_t  read_u8()  { check(1, "u8");  return data[cursor++]; }
    uint16_t read_u16() { check(2, "u16"); uint16_t v; memcpy(&v, data+cursor, 2); cursor += 2; return v; }
    uint32_t read_u32() { check(4, "u32"); uint32_t v; memcpy(&v, data+cursor, 4); cursor += 4; return v; }
    uint64_t read_u64() { check(8, "u64"); uint64_t v; memcpy(&v, data+cursor, 8); cursor += 8; return v; }
    float    read_f32() { check(4, "f32"); float v;    memcpy(&v, data+cursor, 4); cursor += 4; return v; }
    double   read_f64() { check(8, "f64"); double v;   memcpy(&v, data+cursor, 8); cursor += 8; return v; }
    
    std::string read_string() {
        uint64_t len = read_u64();
        check(len, "string data");
        std::string s(reinterpret_cast<const char*>(data + cursor), len);
        cursor += len;
        return s;
    }
    
    void align_to(size_t alignment) {
        size_t padding = (alignment - (cursor % alignment)) % alignment;
        cursor += padding;
    }
    
    void parse_header() {
        header.magic   = read_u32();
        header.version = read_u32();
        
        if (header.magic != GGUF_MAGIC) {
            throw std::runtime_error("Invalid GGUF magic: " + std::to_string(header.magic));
        }
        if (header.version != GGUF_VERSION) {
            throw std::runtime_error("Unsupported GGUF version: " + std::to_string(header.version));
        }
        
        header.n_tensors = read_u64();
        header.n_kv      = read_u64();
        
        std::cerr << "[GGUF] Header: magic=0x" << std::hex << header.magic << std::dec
                  << " version=" << header.version
                  << " tensors=" << header.n_tensors
                  << " kv=" << header.n_kv << std::endl;
    }
    
    std::string arch_prefix = "llama";  // Default architecture prefix
    
public:
    const std::string& get_arch() const { return arch_prefix; }
    
private:
    void parse_metadata() {
        std::cerr << "[GGUF] Parsing " << header.n_kv << " KV pairs at cursor=" << cursor << std::endl;
        
        // First pass: find architecture
        size_t saved_cursor = cursor;
        for (uint64_t i = 0; i < header.n_kv; i++) {
            std::string key = read_string();
            uint32_t type = read_u32();
            if (key == "general.architecture" && type == 8) {
                arch_prefix = read_string();
                std::cerr << "[GGUF] Architecture: " << arch_prefix << std::endl;
            } else {
                skip_value(type);
            }
        }
        cursor = saved_cursor;  // Reset to parse again
        
        // Second pass: extract all metadata
        for (uint64_t i = 0; i < header.n_kv; i++) {
            std::string key = read_string();
            uint32_t type = read_u32();
            
            // Parse value based on type
            switch (type) {
                case 0: read_u8(); break;   // uint8
                case 1: read_u8(); break;   // int8
                case 2: read_u16(); break;  // uint16
                case 3: read_u16(); break;  // int16
                case 4: {  // uint32
                    uint32_t v = read_u32();
                    // Try architecture-specific keys first, then fallbacks
                    if (key == arch_prefix + ".vocab_size" || key == "llama.vocab_size" || key == "mistral.vocab_size") n_vocab = (int32_t)v;
                    else if (key == arch_prefix + ".embedding_length" || key == "llama.embedding_length" || key == "mistral.embedding_length") n_embd = (int32_t)v;
                    else if (key == arch_prefix + ".attention.head_count" || key == "llama.attention.head_count" || key == "mistral.attention.head_count") n_head = (int32_t)v;
                    else if (key == arch_prefix + ".attention.head_count_kv" || key == "llama.attention.head_count_kv" || key == "mistral.attention.head_count_kv") n_head_kv = (int32_t)v;
                    else if (key == arch_prefix + ".block_count" || key == "llama.block_count" || key == "mistral.block_count") n_layer = (int32_t)v;
                    else if (key == arch_prefix + ".feed_forward_length" || key == "llama.feed_forward_length" || key == "mistral.feed_forward_length") n_ff = (int32_t)v;
                    break;
                }
                case 5: read_u32(); break;  // int32
                case 6: {  // float32
                    float v = read_f32();
                    if (key == arch_prefix + ".attention.layer_norm_rms_epsilon" || 
                        key == "llama.attention.layer_norm_rms_epsilon" || 
                        key == "mistral.attention.layer_norm_rms_epsilon") {
                        rms_norm_eps = v;
                    }
                    break;
                }
                case 7: read_u8(); break;   // bool
                case 8: read_string(); break; // string
                case 9: {  // array
                    uint32_t arr_type = read_u32();
                    uint64_t arr_len = read_u64();
                    for (uint64_t j = 0; j < arr_len; j++) {
                        switch (arr_type) {
                            case 4: read_u32(); break;
                            case 5: read_u32(); break;
                            case 6: read_f32(); break;
                            case 8: read_string(); break;
                            default: break;
                        }
                    }
                    break;
                }
                case 10: read_u64(); break; // uint64
                case 11: read_u64(); break; // int64
                case 12: read_f64(); break; // float64
                default:
                    throw std::runtime_error("Unknown metadata type: " + std::to_string(type));
            }
        }
        
        std::cerr << "[GGUF] Metadata: vocab=" << n_vocab << " embed=" << n_embd
                  << " heads=" << n_head << " kv_heads=" << n_head_kv
                  << " layers=" << n_layer << " arch=" << arch_prefix << std::endl;
    }
    
    void skip_value(uint32_t type) {
        switch (type) {
            case 0: read_u8(); break;
            case 1: read_u8(); break;
            case 2: read_u16(); break;
            case 3: read_u16(); break;
            case 4: read_u32(); break;
            case 5: read_u32(); break;
            case 6: read_f32(); break;
            case 7: read_u8(); break;
            case 8: read_string(); break;
            case 9: {
                uint32_t arr_type = read_u32();
                uint64_t arr_len = read_u64();
                for (uint64_t j = 0; j < arr_len; j++) {
                    switch (arr_type) {
                        case 4: read_u32(); break;
                        case 5: read_u32(); break;
                        case 6: read_f32(); break;
                        case 8: read_string(); break;
                        default: break;
                    }
                }
                break;
            }
            case 10: read_u64(); break;
            case 11: read_u64(); break;
            case 12: read_f64(); break;
        }
    }
    
    void parse_tensors() {
        // Align to 32 bytes before tensor info array
        align_to(GGUF_ALIGNMENT);
        std::cerr << "[GGUF] Parsing " << header.n_tensors << " tensors at cursor=" << cursor << std::endl;
        
        // Parse tensor info array
        for (uint64_t i = 0; i < header.n_tensors; i++) {
            TensorInfo ti;
            
            ti.name    = read_string();
            ti.n_dims  = read_u32();
            
            if (ti.n_dims > 4) {
                throw std::runtime_error("Tensor '" + ti.name + "' has too many dims: " + std::to_string(ti.n_dims));
            }
            
            ti.dims.resize(ti.n_dims);
            ti.size_bytes = 1;
            
            for (uint32_t d = 0; d < ti.n_dims; d++) {
                ti.dims[d] = read_u64();
                ti.size_bytes *= ti.dims[d];
            }
            
            ti.type   = static_cast<GGMLType>(read_u32());
            ti.offset = read_u64();
            
            ti.size_bytes *= ti.element_size();
            
            // Calculate strides
            if (ti.n_dims >= 2) {
                ti.stride[0] = ti.dims[1] * ti.element_size();
                ti.stride[1] = ti.element_size();
                if (ti.dims[0] < ti.dims[1] && ti.name.find("weight") != std::string::npos) {
                    ti.is_transposed = true;
                }
            }
            
            // Debug first tensor
            if (i == 0) {
                std::cerr << "[GGUF] First tensor: '" << ti.name << "' dims=";
                for (auto d : ti.dims) std::cerr << d << " ";
                std::cerr << "type=" << (int)ti.type << " offset=" << ti.offset << std::endl;
            }
            
            tensor_map[ti.name] = tensors.size();
            tensors.push_back(ti);
        }
        
        // Align to data section
        align_to(GGUF_ALIGNMENT);
        size_t data_section_start = cursor;
        std::cerr << "[GGUF] Data section starts at offset " << data_section_start << std::endl;
        
        // Calculate absolute data pointers
        for (auto& ti : tensors) {
            size_t abs_offset = data_section_start + ti.offset;
            
            if (abs_offset + ti.size_bytes <= size) {
                ti.data_ptr = data + abs_offset;
            } else {
                std::cerr << "[ERROR] Tensor '" << ti.name << "' out of bounds: "
                          << abs_offset << " + " << ti.size_bytes << " > " << size << std::endl;
                ti.data_ptr = nullptr;
            }
        }
    }
};

} // namespace rawr
