#pragma once

#include "streaming_gguf_loader.h"
#include <atomic>
#include <memory>
#include <thread>
#include <mutex>
#include <array>
#include <chrono>
#include <vector>
#include <condition_variable>
#include <span>
#include <cstddef>

// ============================================================================
// ENHANCED STREAMING GGUF LOADER - Predictive, NVMe-optimized, parallelized
// ============================================================================

// Predictive cache entry for access pattern prediction
struct PredictiveAccessEntry {
    uint32_t zone_id = 0;
    float confidence = 0.0f;           // 0.0-1.0 prediction score
    uint64_t last_access_tick = 0;
    uint32_t access_frequency = 0;
    
    PredictiveAccessEntry() = default;
};

// NVMe I/O context (Windows 11 22H2+ with direct I/O support)
struct NVMeIOContext {
    void* hDevice = nullptr;           // NVMe device handle
    uint32_t sq_tail = 0;              // Submission queue tail
    uint32_t cq_head = 0;              // Completion queue head
    void* sq_base = nullptr;           // SQ virtual address
    void* cq_base = nullptr;           // CQ virtual address
    void* prp_list = nullptr;          // Physical region pages
    void* doorbell_base = nullptr;     // SQ tail doorbell
    bool enabled = false;
    
    NVMeIOContext() = default;
};

// IORING context (Windows 11 async batch I/O)
struct IORingContext {
    void* hRing = nullptr;
    void* hCompletionEvent = nullptr;
    std::atomic<uint32_t> pending_ops{0};
    uint32_t max_ops = 64;
    bool enabled = false;
    
    IORingContext() = default;
};

// Tensor shard for parallel device loading
struct TensorShard {
    int32_t device_id = -1;            // GPU 0-N or CPU -1
    void* device_memory = nullptr;     // Device buffer
    void* host_staging = nullptr;      // Pinned host buffer
    uint64_t slice_offset = 0;         // Offset in tensor
    uint64_t slice_size = 0;           // Size of shard
    void* event_handle = nullptr;      // Completion event
    std::atomic<bool> completed{false};
    
    TensorShard() = default;
};

// Enhanced zone with predictive metadata
struct EnhancedZoneInfo : public TensorZoneInfo {
    float predictive_score = 0.0f;
    uint32_t prefetch_priority = 0;
    uint32_t compression_codec = 0;    // 0=none, 1=deflate, 2=lz4, 3=zstd
    uint64_t compressed_size = 0;
    uint16_t nvme_cmd_id = 0;
    void* huge_page_ptr = nullptr;     // 2MB aligned if available
    std::atomic<bool> prefetch_in_progress{false};
    
    EnhancedZoneInfo() = default;
};

// Constants
namespace EnhancedLoaderConstants {
    constexpr uint32_t ZONE_BUDGET_DEFAULT = 536870912;      // 512MB
    constexpr uint32_t ZONE_BUDGET_TIGHT = 402653184;        // 384MB for 70B-120B
    constexpr uint32_t ZONE_BUDGET_ULTRA = 134217728;        // 128MB for 800B+
    constexpr uint32_t ZONE_BUDGET_AGGRESSIVE = 67108864;    // 64MB extreme
    
    constexpr uint32_t PREDICTIVE_WINDOW = 8;                // Lookahead zones
    constexpr uint32_t NVME_QUEUE_DEPTH = 64;                // NVMe SQ/CQ depth
    constexpr uint64_t HUGE_PAGE_SIZE = 2097152;             // 2MB huge pages
    constexpr uint32_t TENSOR_PARALLEL_MAX = 8;              // Max GPU/CPU shards
    
    constexpr uint32_t PREDICTOR_TABLE_SIZE = 256;           // Hash table entries
    constexpr uint32_t ACCESS_HISTORY_SIZE = 16;             // Pattern history depth
    
    constexpr float CONFIDENCE_THRESHOLD = 0.7f;             // Sequential pattern threshold
    constexpr float SEQUENTIAL_WEIGHT = 0.5f;                // LSTM-style weights
    constexpr float FREQUENCY_WEIGHT = 0.25f;
}

// ============================================================================
// ENHANCED STREAMING GGUF LOADER CLASS
// ============================================================================

class EnhancedStreamingGGUFLoader : public StreamingGGUFLoader {
public:
    EnhancedStreamingGGUFLoader();
    ~EnhancedStreamingGGUFLoader();

    // ---- Core API (enhanced from base) ----
    bool Open(const std::string& filepath) override;
    bool Close() override;
    bool GetTensorData(const std::string& tensor_name, std::vector<uint8_t>& data); // Match base signature
    
    // ---- ZERO-COPY ACCESS (Enhanced with predictive prefetch) ----
    // Returns view into zone memory with automatic prefetch of next predicted zones
    std::span<const std::byte> GetTensorView(
        const std::string& tensor_name,
        size_t offset = 0,
        size_t length = SIZE_MAX
    );
    
    // Async prefetch for tensor (non-blocking, call IsTensorResident to poll)
    void PrefetchTensorAsync(const std::string& tensor_name);
    
    // ---- Predictive Caching ----
    void UpdateAccessPattern(uint32_t zone_id);
    std::vector<uint32_t> PredictNextZones(uint32_t max_count = EnhancedLoaderConstants::PREDICTIVE_WINDOW);
    float GetPredictionConfidence(uint32_t zone_id) const;
    uint32_t GetAccessFrequency(uint32_t zone_id) const;
    
    // ---- Zone Prefetching ----
    bool PrefetchZoneAsync(uint32_t zone_id);
    bool WaitForPrefetch(uint32_t zone_id, uint32_t timeout_ms = 5000);
    std::vector<uint32_t> GetPrefetchingZones() const;
    
    // ---- NVMe Direct I/O ----
    bool EnableNVMeDirectIO();
    bool DisableNVMeDirectIO();
    bool IsNVMeEnabled() const { return nvme_context_.enabled; }
    
    // ---- IORING Batch I/O ----
    bool EnableIOring();
    bool DisableIOring();
    bool IsIOringEnabled() const { return ioring_context_.enabled; }
    
    // ---- Huge Pages ----
    bool AllocateHugePages(uint64_t total_size_mb = 1024);
    void* AllocateHugePage(uint64_t size);
    bool ReleaseHugePages();
    uint64_t GetHugePageUsage() const { return huge_page_used_; }
    
    // ---- Tensor Parallelism ----
    int DetectComputeDevices();
    bool LoadTensorParallel(const std::string& tensor_name, std::vector<uint8_t>& data, int preferred_device = -1);
    int GetComputeDeviceCount() const { return compute_device_count_; }
    
    // ---- Adaptive Compression ----
    void SetCompressionPreference(uint32_t preference);  // 0=none, 1=fast, 2=balanced, 3=max
    uint32_t GetCompressionCodec() const { return compression_preference_; }
    
    // ---- Performance Monitoring ----
    struct PerformanceMetrics {
        uint64_t total_tensor_loads = 0;
        uint64_t cache_hits = 0;
        uint64_t cache_misses = 0;
        uint64_t prefetch_hits = 0;
        uint64_t prefetch_count = 0;  // Total prefetch operations
        uint64_t total_io_bytes = 0;
        double avg_load_time_us = 0.0;
        double peak_io_throughput_gbps = 0.0;
    };
    
    PerformanceMetrics GetMetrics() const { return metrics_; }
    void ResetMetrics();
    
private:
    // ---- Predictive Cache ----
    std::array<PredictiveAccessEntry, EnhancedLoaderConstants::PREDICTOR_TABLE_SIZE> predictor_table_;
    std::array<uint32_t, EnhancedLoaderConstants::ACCESS_HISTORY_SIZE> access_history_;
    uint32_t history_index_ = 0;
    mutable std::mutex predictor_mutex_;
    
    // Helper: Calculate prediction from access pattern
    float CalculatePredictionConfidence(const std::array<uint32_t, 3>& recent_accesses);
    
    // ---- I/O Optimization ----
    NVMeIOContext nvme_context_;
    IORingContext ioring_context_;
    
    // ---- Huge Pages ----
    void* huge_page_pool_ = nullptr;
    std::vector<bool> huge_page_bitmap_;
    uint64_t huge_page_total_ = 0;
    uint64_t huge_page_used_ = 0;
    std::mutex huge_page_mutex_;
    
    // ---- Tensor Parallelism ----
    std::array<TensorShard, EnhancedLoaderConstants::TENSOR_PARALLEL_MAX> tensor_shards_;
    int compute_device_count_ = 0;
    std::mutex parallel_load_mutex_;
    
    // ---- Prefetch Management ----
    std::unordered_map<uint32_t, std::atomic<bool>> prefetch_in_progress_;
    std::thread prefetch_thread_;
    std::atomic<bool> prefetch_shutdown_{false};
    std::queue<uint32_t> prefetch_queue_;
    std::mutex prefetch_queue_mutex_;
    std::condition_variable prefetch_cv_;
    
    void PrefetchWorkerThread();
    
    // ---- Compression ----
    uint32_t compression_preference_ = 0;
    bool DecompressZone(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& output, uint32_t codec);
    
    // ---- Performance ----
    PerformanceMetrics metrics_;
    mutable std::mutex metrics_mutex_;
    
    // ---- Helper Methods ----
    void InitializeNVMeIfAvailable();
    void InitializeIORingIfAvailable();
    void InitializeHugePagePool();
    void InitializePredictor();
    
    bool LoadWithNVMe(uint32_t zone_id, std::vector<uint8_t>& data);
    bool LoadWithIOring(uint32_t zone_id, std::vector<uint8_t>& data);
    bool LoadWithParallel(const std::string& tensor_name, std::vector<uint8_t>& data, int preferred_device);
};

// ============================================================================
// HELPER UTILITIES
// ============================================================================

namespace EnhancedLoaderUtils {
    // Decompress with hardware acceleration (AVX-512 where available)
    bool DecompressDeflate(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& output);
    bool DecompressLZ4(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& output);
    bool DecompressZSTD(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& output);
    
    // NVMe command helpers
    bool IsNVMeAvailable();
    void* OpenNVMeDevice();
    
    // IORING helpers (Windows 11 22H2+)
    bool IsIORingAvailable();
    void* CreateIORing(uint32_t queue_depth);
    
    // Huge page helpers
    bool IsHugePagesAvailable();
    void* AllocateHugePage(uint64_t size);
    
    // Device detection
    int DetectGPUDevices();
    int DetectComputeDevices();
    
    // Performance timing
    inline uint64_t GetTicks() {
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }
    
    inline double TicksToMicroseconds(uint64_t ticks) {
        return static_cast<double>(ticks) / 1000.0;
    }
}

// Header guard end
// (Removed duplicate #endif to fix C1020 error)

