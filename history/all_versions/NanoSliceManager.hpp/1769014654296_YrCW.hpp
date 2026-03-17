#pragma once

#include <atomic>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <QDebug>

namespace RawrXD {

// Zen4 7800X3D optimized constants
constexpr size_t NANOSLICE_SIZE = 4ULL * 1024 * 1024;      // 4MB (L3 cache friendly)
constexpr size_t NANOSLICE_ALIGN = 64;                     // Cache line alignment
constexpr size_t MAX_L1_SLICES = 8;                        // 8 * 4MB = 32MB (L3)
constexpr size_t MAX_L2_SLICES = 128;                      // 128 * 4MB = 512MB (DDR5)
constexpr size_t MAX_L3_SLICES = 1536;                     // 1536 * 4MB = 6GB (NVMe)
constexpr size_t MAX_VRAM_SLICES = 896;                    // 14GB of 16GB VRAM
constexpr size_t PREFETCH_L1_DISTANCE = 8;                 // Cache lines
constexpr size_t PREFETCH_L2_DISTANCE = 32;
constexpr size_t PREFETCH_L3_DISTANCE = 128;

enum class SliceState : uint8_t {
    UNLOADED = 0,
    LOADING = 1,
    LOADED = 2,
    EVICTING = 3,
    IN_VRAM = 4,
    IN_PAGEFILE = 5
};

struct alignas(NANOSLICE_ALIGN) NanoSlice {
    uint8_t data[NANOSLICE_SIZE];  // Aligned storage
    std::atomic<SliceState> state{SliceState::UNLOADED};
    std::atomic<uint32_t> access_counter{0};
    std::atomic<uint64_t> last_access_cycle{0};
    void* vram_ptr{nullptr};       // GPU address
    void* pagefile_ptr{nullptr};   // NVMe-mapped address
    uint64_t compressed_size{0};   // Size after compression
    
    // Zen4 branch prediction metadata
    struct {
        uint16_t btb_index{0};        // Branch Target Buffer
        uint16_t prediction_history{0}; // Last 16 predictions
    } zen4_metadata;
    
    // Pre-computed hash for fast lookup
    uint64_t slice_hash{0};
};

// Forward declarations
class TencentCompressionProvider;

class NanoSliceManager {
public:
    explicit NanoSliceManager();
    ~NanoSliceManager();
    
    // Delete copy/move
    NanoSliceManager(const NanoSliceManager&) = delete;
    NanoSliceManager& operator=(const NanoSliceManager&) = delete;
    NanoSliceManager(NanoSliceManager&&) = delete;
    NanoSliceManager& operator=(NanoSliceManager&&) = delete;
    
    // Core operations - force inline for zero call overhead
    __forceinline void* LoadSlice(uint64_t tensor_id, uint64_t offset, void* target);
    __forceinline void* MapSlice(uint64_t tensor_id, uint64_t offset);
    __forceinline bool PrefetchSlice(uint64_t tensor_id, uint64_t offset);
    
    // Memory tier management
    bool EvictToVram(uint64_t tensor_id, uint64_t offset);
    bool EvictToPagefile(uint64_t tensor_id, uint64_t offset);
    bool PromoteToL1(uint64_t tensor_id, uint64_t offset);
    bool HandleMemoryPressure(size_t required_bytes);
    
    // Memory usage tracking
    size_t GetActiveRamUsage() const;
    size_t GetActiveVramUsage() const;
    size_t GetActivePagefileUsage() const;
    
    // Hardware performance monitoring
    struct Zen4Metrics {
        uint64_t l1_hits{0};
        uint64_t l2_hits{0};
        uint64_t l3_hits{0};
        uint64_t vram_hits{0};
        uint64_t pagefile_hits{0};
        uint64_t dtlb_misses{0};
        double effective_bandwidth{0.0};
        double avg_load_latency{0.0};
        double prediction_accuracy{0.0};
        uint64_t evictions{0};
    };
    Zen4Metrics GetZen4Metrics() const;
    
    // Predictive prefetch using Markov chains
    void TrainPrefetchModel(uint64_t tensor_id, uint64_t offset);
    void PredictivePrefetch(uint64_t tensor_id, uint64_t current_offset);
    
private:
    // Key generation (tensor_id + offset)
    __forceinline uint64_t GenerateKey(uint64_t tensor_id, uint64_t offset) {
        return (tensor_id << 32) | (offset / NANOSLICE_SIZE);
    }
    
    // Zen4-optimized AVX-512 memcpy
    __forceinline void Avx512Memcpy(void* __restrict dest, const void* __restrict src, size_t size);
    
    // Non-temporal streaming store for evictions
    __forceinline void StreamingStore(void* __restrict dest, const void* __restrict src, size_t size);
    
    // Update LRU and access patterns
    void UpdateAccessPattern(uint64_t key, NanoSlice* slice);
    
    // Memory pressure handling
    NanoSlice* FindEvictionCandidate();
    bool CompressSlice(NanoSlice* slice);
    
    // Hardware performance counters
    void ReadPerformanceCounters();
    
    // Data structures
    struct {
        std::unordered_map<uint64_t, NanoSlice> slices;
        mutable std::shared_mutex mutex;
    } slice_cache_;
    
    struct MarkovState {
        uint64_t offset_probabilities[8]{};  // Top 8 likely next offsets
        uint8_t access_pattern[16]{};        // Last 16 accesses (0=miss, 1=hit)
        double transition_matrix[8][8]{};    // 8x8 transition probabilities
    };
    std::unordered_map<uint64_t, MarkovState> markov_model_;
    mutable std::mutex markov_mutex_;
    
    // LRU queues for each tier
    std::vector<uint64_t> l1_lru_;
    std::vector<uint64_t> l2_lru_;
    std::vector<uint64_t> vram_lru_;
    mutable std::mutex lru_mutex_;
    
    // Statistics
    struct {
        std::atomic<uint64_t> l1_hits{0};
        std::atomic<uint64_t> l2_hits{0};
        std::atomic<uint64_t> l3_hits{0};
        std::atomic<uint64_t> vram_hits{0};
        std::atomic<uint64_t> pagefile_hits{0};
        std::atomic<uint64_t> l1_misses{0};
        std::atomic<uint64_t> l2_misses{0};
        std::atomic<uint64_t> l3_misses{0};
        std::atomic<uint64_t> dtlb_misses{0};
        std::atomic<uint64_t> total_loads{0};
        std::atomic<uint64_t> total_latency{0};
        std::atomic<uint64_t> correct_predictions{0};
        std::atomic<uint64_t> total_predictions{0};
        std::atomic<uint64_t> evictions{0};
    } stats_;
    
    // Subsystem pointers
    TencentCompressionProvider* tencent_provider_;
    
    // Prefetching thread
    std::thread prefetch_thread_;
    std::queue<std::pair<uint64_t, uint64_t>> prefetch_queue_;
    std::mutex pf_mutex_;
    std::condition_variable pf_cv_;
    std::atomic<bool> shutdown_{false};
    
    void PrefetchWorker();
    
    // Find slice by key
    NanoSlice* FindSlice(uint64_t tensor_id, uint64_t offset);
};

} // namespace RawrXD
