// ============================================================================
// vision_gpu_staging.hpp — GPU Zero-Copy Image Staging Buffer
// ============================================================================
// Provides a persistent mapped GPU buffer for staging image data from CPU
// to GPU without intermediate copies. Uses platform-specific APIs:
//
//   Windows: CreateFileMapping + MapViewOfFile (shared memory between CPU/GPU)
//   Vulkan:  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
//   DirectX: D3D12_HEAP_TYPE_UPLOAD with persistent map
//
// The staging buffer is allocated once at initialization and reused across
// multiple image encode operations. Images are written directly to the mapped
// pointer, avoiding a CPU-side temp buffer → GPU copy.
//
// Lifecycle:
//   1. initialize() — allocate persistent mapped buffer
//   2. stageImage() / stagePatches() — memcpy image data to mapped ptr
//   3. commitToDevice() — flush/fence to ensure GPU sees the data
//   4. reset() — reset write offset for next image
//   5. shutdown() — unmap and free
//
// Integration: Called by VisionEncoder before GPU-side preprocessing/inference.
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "vision_encoder.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace RawrXD {
namespace Vision {

// ============================================================================
// Staging Buffer Configuration
// ============================================================================
struct StagingConfig {
    uint64_t bufferSizeBytes;      // Total staging buffer size
    bool     persistent;            // Keep mapped for lifetime
    bool     coherent;              // Auto-flush (no explicit flush needed)
    bool     useVulkan;             // Use Vulkan staging (if available)
    bool     useDirectX;            // Use D3D12 upload heap (if available)
    bool     useSharedMemory;       // Windows shared memory fallback

    StagingConfig()
        : bufferSizeBytes(16 * 1024 * 1024)  // 16 MB
        , persistent(true)
        , coherent(true)
        , useVulkan(false)
        , useDirectX(false)
        , useSharedMemory(true)     // Default to shared memory
    {}
};

// ============================================================================
// Staging Allocation — A sub-allocation within the staging buffer
// ============================================================================
struct StagingAllocation {
    void*    mappedPtr;         // CPU-accessible pointer
    uint64_t offset;            // Offset within staging buffer
    uint64_t size;              // Size of this allocation
    uint64_t gpuAddress;        // GPU-side virtual address (if applicable)
    bool     committed;         // Whether data has been committed to GPU
    bool     valid;

    StagingAllocation()
        : mappedPtr(nullptr), offset(0), size(0), gpuAddress(0),
          committed(false), valid(false)
    {}
};

// ============================================================================
// Staging Buffer State
// ============================================================================
enum class StagingBackend : uint8_t {
    None         = 0,
    SharedMemory = 1,   // Windows CreateFileMapping
    Vulkan       = 2,   // VkBuffer + VkDeviceMemory
    DirectX12    = 3    // ID3D12Resource (UPLOAD heap)
};

// ============================================================================
// Staging Statistics
// ============================================================================
struct StagingStats {
    uint64_t totalStaged;          // Total staging operations
    uint64_t totalBytesStaged;     // Cumulative bytes staged
    uint64_t totalCommits;         // GPU commits performed
    uint64_t totalResets;          // Buffer resets
    uint64_t currentUsedBytes;     // Current write offset
    uint64_t bufferCapacity;       // Total buffer size
    double   avgStagingLatencyUs;  // Average staging latency (microseconds)
    double   avgCommitLatencyUs;   // Average commit latency
    StagingBackend backend;        // Active backend
};

// ============================================================================
// VisionGPUStaging — GPU zero-copy staging buffer manager
// ============================================================================
class VisionGPUStaging {
public:
    static VisionGPUStaging& instance();

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    // Initialize the staging buffer. Allocates persistent mapped memory.
    VisionResult initialize(const StagingConfig& config);

    // Check if staging is initialized and ready.
    bool isReady() const;

    // Shutdown: unmap, deallocate, release handles.
    VisionResult shutdown();

    // -----------------------------------------------------------------------
    // Staging Operations
    // -----------------------------------------------------------------------

    // Stage an image buffer into GPU-accessible memory.
    // Returns a StagingAllocation with the mapped pointer to the staged data.
    VisionResult stageImage(const ImageBuffer& image,
                             StagingAllocation& allocation);

    // Stage normalized float patches for GPU inference.
    // patches: FP32 [numPatches × patchDim]
    VisionResult stagePatches(const float* patches,
                               uint32_t numPatches,
                               uint32_t patchDim,
                               StagingAllocation& allocation);

    // Stage arbitrary data into staging buffer.
    VisionResult stageRaw(const void* data, uint64_t sizeBytes,
                           StagingAllocation& allocation);

    // -----------------------------------------------------------------------
    // Commit & Synchronization
    // -----------------------------------------------------------------------

    // Flush staged data to ensure GPU visibility.
    // For coherent buffers, this is a no-op.
    // For non-coherent, performs an explicit flush.
    VisionResult flushStagedData(const StagingAllocation& allocation);

    // Commit all pending staged data to GPU.
    // Inserts a memory barrier / fence.
    VisionResult commitToDevice();

    // Wait for GPU to finish reading staged data.
    VisionResult waitForGPU();

    // -----------------------------------------------------------------------
    // Buffer Management
    // -----------------------------------------------------------------------

    // Reset write offset to beginning (reuse buffer for next frame).
    VisionResult resetBuffer();

    // Get current write offset and remaining space.
    uint64_t getRemainingBytes() const;
    uint64_t getCurrentOffset() const;

    // -----------------------------------------------------------------------
    // Direct Access
    // -----------------------------------------------------------------------

    // Get the base mapped pointer (for direct writing).
    void* getMappedPointer() const;

    // Get backend type.
    StagingBackend getBackend() const;

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    StagingStats getStats() const;
    void resetStats();

private:
    VisionGPUStaging();
    ~VisionGPUStaging();
    VisionGPUStaging(const VisionGPUStaging&) = delete;
    VisionGPUStaging& operator=(const VisionGPUStaging&) = delete;

    // Backend initialization
    VisionResult initSharedMemory(uint64_t size);
    VisionResult initVulkan(uint64_t size);
    VisionResult initDirectX(uint64_t size);

    // Backend cleanup
    void cleanupSharedMemory();
    void cleanupVulkan();
    void cleanupDirectX();

    // Suballocation: carve out a region from the staging buffer
    VisionResult suballocate(uint64_t sizeBytes, uint64_t alignment,
                              StagingAllocation& allocation);

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex mutex_;
    StagingConfig config_;
    bool initialized_ = false;
    StagingBackend backend_ = StagingBackend::None;

    // Mapped buffer
    void*    basePtr_ = nullptr;      // CPU-mapped base pointer
    uint64_t bufferSize_ = 0;
    uint64_t writeOffset_ = 0;        // Current write position (bump allocator)

    // Platform handles
#ifdef _WIN32
    HANDLE fileMappingHandle_ = nullptr;
#endif
    void*  vulkanDll_ = nullptr;       // HMODULE for vulkan-1.dll
    void*  vulkanDevice_ = nullptr;    // VkDevice (opaque)
    void*  vulkanBuffer_ = nullptr;    // VkBuffer (opaque)
    void*  vulkanMemory_ = nullptr;    // VkDeviceMemory (opaque)
    void*  d3d12Resource_ = nullptr;   // ID3D12Resource (opaque)

    // Statistics
    std::atomic<uint64_t> totalStaged_{0};
    std::atomic<uint64_t> totalBytesStaged_{0};
    std::atomic<uint64_t> totalCommits_{0};
    std::atomic<uint64_t> totalResets_{0};
    double stagingLatencyAccum_ = 0.0;    // Microseconds
    double commitLatencyAccum_ = 0.0;
};

} // namespace Vision
} // namespace RawrXD
