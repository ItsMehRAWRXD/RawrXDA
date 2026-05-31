#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <span>
#include <atomic>
#include <windows.h>

namespace RawrXD {

// ============================================================================
// Unlimited Streaming Model Loader
// Handles models of ANY size by:
// - Memory-mapping the GGUF file instead of loading
// - Lazy tensor access without materialization
// - Progressive KV cache with disk spillover
// - Context window adaptation
// ============================================================================

class UnlimitedStreamingLoader {
public:
    UnlimitedStreamingLoader();
    ~UnlimitedStreamingLoader();

    // Open a GGUF file with memory mapping for any size
    bool OpenMapped(const std::string& filepath);
    void Close();

    // Get tensor view directly from mapped file (zero-copy)
    std::span<const uint8_t> GetTensorView(const std::string& tensor_name);
    
    // Stream tensor in chunks without full materialization
    struct StreamChunk {
        uint64_t offset;      // Offset in file
        uint64_t size;        // Bytes to read
        const uint8_t* ptr;   // Mapped memory ptr if available
    };
    std::vector<StreamChunk> GetTensorStreamChunks(const std::string& tensor_name, size_t chunk_size = 4*1024*1024);

    // Get metadata without loading tensors
    uint32_t GetContextLength() const { return context_length_; }
    uint64_t GetTotalModelSize() const { return file_size_; }
    uint32_t GetLayerCount() const { return layer_count_; }
    
    // Adaptive context: shrink if memory pressure, expand if capacity
    void SetMaxMemoryMB(uint32_t mb) { max_memory_mb_ = mb; }
    uint32_t AdaptiveContextWindow(uint32_t requested_ctx) const;

private:
    struct MappedRegion {
        HANDLE file_handle = INVALID_HANDLE_VALUE;
        HANDLE map_handle = nullptr;
        void* view_ptr = nullptr;
        uint64_t view_size = 0;
        uint64_t file_offset = 0;
    };

    struct TensorInfo {
        std::string name;
        uint64_t file_offset = 0;
        uint64_t size = 0;
        uint32_t type = 0;
        std::vector<uint32_t> shape;
    };

    // File mapping infrastructure
    std::string filepath_;
    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
    uint64_t file_size_ = 0;
    std::vector<MappedRegion> mapped_regions_;
    static constexpr uint64_t MAP_WINDOW_SIZE = 512 * 1024 * 1024; // 512MB window

    // Metadata cache
    std::unordered_map<std::string, TensorInfo> tensor_index_;
    uint32_t context_length_ = 2048;
    uint32_t layer_count_ = 0;
    uint32_t max_memory_mb_ = 16384; // Default 16GB
    
    // Implementation helpers
    bool MapFileRegion(uint64_t file_offset, uint64_t size, MappedRegion& region);
    bool UnmapFileRegion(MappedRegion& region);
    const uint8_t* GetMappedPointer(uint64_t file_offset, uint64_t size);
    bool ParseGGUFHeaderMapped();
    bool BuildTensorIndexMapped();
};

} // namespace RawrXD
