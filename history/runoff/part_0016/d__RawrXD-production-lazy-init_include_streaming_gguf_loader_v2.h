// streaming_gguf_loader_v2.h
// RawrXD Robust GGUF Loader v2.0 - Production-Hardened API
// Zero-copy memory mapping | Lazy metadata pagination | 800B+ model support
// Copyright (c) 2026 RawrXD Project

#ifndef STREAMING_GGUF_LOADER_V2_H
#define STREAMING_GGUF_LOADER_V2_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <memory>

// Forward declarations for pimpl
class RobustMMFBackend;

// ═════════════════════════════════════════════════════════════════════════════
// GGUF Type System (from spec v3)
// ═════════════════════════════════════════════════════════════════════════════
enum class GGUFType : uint32_t {
    GGUF_TYPE_UINT8 = 0,
    GGUF_TYPE_INT8 = 1,
    GGUF_TYPE_UINT16 = 2,
    GGUF_TYPE_INT16 = 3,
    GGUF_TYPE_UINT32 = 4,
    GGUF_TYPE_INT32 = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL = 7,
    GGUF_TYPE_STRING = 8,
    GGUF_TYPE_ARRAY = 9,
    GGUF_TYPE_UINT64 = 10,
    GGUF_TYPE_INT64 = 11,
    GGUF_TYPE_FLOAT64 = 12
};

// ═════════════════════════════════════════════════════════════════════════════
// Public Data Structures
// ═════════════════════════════════════════════════════════════════════════════

struct GGUFHeader {
    uint32_t magic;              // 0x46554747 "GGUF"
    uint32_t version;            // 2 or 3
    uint64_t tensor_count;       // Number of tensors in file
    uint64_t metadata_kv_count;  // Number of metadata key-value pairs
};

// Metadata storage using variant for type safety
using MetadataValue = std::variant<
    uint8_t, int8_t, uint16_t, int16_t,
    uint32_t, int32_t, float,
    uint64_t, int64_t, double,
    bool, std::string,
    std::vector<std::string>  // For arrays (simplified)
>;

using MetadataMap = std::unordered_map<std::string, MetadataValue>;

// ═════════════════════════════════════════════════════════════════════════════
// Main Model Container
// ═════════════════════════════════════════════════════════════════════════════

struct GGUFModel {
    struct Tensor {
        std::string name;
        uint32_t n_dims;
        uint64_t dims[4];
        uint32_t type;           // GGML type enum
        uint64_t offset_in_file; // Absolute file offset
        size_t size_bytes;       // Calculated tensor size
        void* data;              // Pointer to data (owned or view)
        
        enum Ownership {
            Owned,  // Allocated with _aligned_malloc, must free
            View    // Points into memory-mapped file, read-only
        } ownership;
    };
    
    GGUFHeader header;
    MetadataMap metadata;
    std::vector<Tensor> tensors;
    
    bool is_loaded = false;
    bool use_mmf = false;  // True if using memory-mapped file (lazy mode)
};

// ═════════════════════════════════════════════════════════════════════════════
// Main Loader Class
// ═════════════════════════════════════════════════════════════════════════════

class StreamingGGUFLoaderV2 {
public:
    StreamingGGUFLoaderV2();
    ~StreamingGGUFLoaderV2();
    
    // Prevent copy (owns file handles)
    StreamingGGUFLoaderV2(const StreamingGGUFLoaderV2&) = delete;
    StreamingGGUFLoaderV2& operator=(const StreamingGGUFLoaderV2&) = delete;
    
    /**
     * Load GGUF model from file with optional lazy loading
     * 
     * @param path     UTF-8 file path
     * @param model    Output model structure
     * @param lazy     If true, tensors remain in MMF (zero-copy, view-only)
     *                 If false, tensors copied to aligned heap (owned, writable)
     * @return true on success, false on parse error or corruption
     * 
     * Note: In lazy mode, model.tensors[i].data remains valid only while
     *       StreamingGGUFLoaderV2 instance exists and hasn't been Unload()ed
     */
    bool LoadFromFile(const std::string& path, GGUFModel& model, bool lazy = true);
    
    /**
     * Release all resources associated with a loaded model
     * Frees owned tensors and unmaps file
     */
    void Unload(GGUFModel& model);
    
    /**
     * Get metadata value by key with type checking
     * Returns nullptr if key not found or type mismatch
     */
    template<typename T>
    const T* GetMetadata(const GGUFModel& model, const std::string& key) const {
        auto it = model.metadata.find(key);
        if (it == model.metadata.end()) return nullptr;
        return std::get_if<T>(&it->second);
    }
    
    /**
     * Find tensor by name
     * Returns nullptr if not found
     */
    const GGUFModel::Tensor* FindTensor(const GGUFModel& model, const std::string& name) const {
        for (const auto& t : model.tensors) {
            if (t.name == name) return &t;
        }
        return nullptr;
    }

private:
    std::unique_ptr<RobustMMFBackend> mmf_backend_;
};

// ═════════════════════════════════════════════════════════════════════════════
// Helper Functions
// ═════════════════════════════════════════════════════════════════════════════

/**
 * Get human-readable name for GGUF metadata type
 */
inline const char* GGUFTypeToString(GGUFType type) {
    switch (type) {
        case GGUFType::GGUF_TYPE_UINT8: return "uint8";
        case GGUFType::GGUF_TYPE_INT8: return "int8";
        case GGUFType::GGUF_TYPE_UINT16: return "uint16";
        case GGUFType::GGUF_TYPE_INT16: return "int16";
        case GGUFType::GGUF_TYPE_UINT32: return "uint32";
        case GGUFType::GGUF_TYPE_INT32: return "int32";
        case GGUFType::GGUF_TYPE_FLOAT32: return "float32";
        case GGUFType::GGUF_TYPE_BOOL: return "bool";
        case GGUFType::GGUF_TYPE_STRING: return "string";
        case GGUFType::GGUF_TYPE_ARRAY: return "array";
        case GGUFType::GGUF_TYPE_UINT64: return "uint64";
        case GGUFType::GGUF_TYPE_INT64: return "int64";
        case GGUFType::GGUF_TYPE_FLOAT64: return "float64";
        default: return "unknown";
    }
}

/**
 * Calculate required memory for model (excluding skipped metadata)
 */
inline size_t EstimateModelMemoryUsage(const GGUFModel& model) {
    size_t total = 0;
    for (const auto& t : model.tensors) {
        total += t.size_bytes;
    }
    return total;
}

#endif // STREAMING_GGUF_LOADER_V2_H
