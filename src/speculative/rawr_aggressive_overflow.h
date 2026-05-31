// rawr_aggressive_overflow.h
// Aggressive DDR5-to-GPU aperture bypass with tiered overflow management
// Optimized for 64GB RAM + 16GB VRAM = 80GB unified pool

#pragma once
#include "rawr_sovereign_bridge.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <algorithm>
#include <type_traits>
#include <cstring>
#include <iostream>

// Aggressive MASM primitives from rawr_aperture_bypass.asm.
extern "C" {
    uint32_t RawrCheckOverflowTierAggressive(uint32_t util_bits);
    uint32_t RawrPredictiveTierPromotion(uint32_t util_bits, uint32_t growth_bits,
                                         uint64_t ddr5_bw, uint64_t pcie_bw);
    void RawrStreamingPrefetch(void* ptr, size_t size, uint32_t tier);
    size_t RawrEmergencyTensorCompress(void* src, void* dst, size_t size, uint32_t tier);
    uint32_t RawrProactiveColdEviction(void** tensor_ptrs, uint64_t* last_access,
                                       size_t count, uint64_t current_time,
                                       uint64_t threshold_ns, uint32_t tier);
    uint32_t RawrBandwidthAwareThrottle(uint32_t util_bits, uint64_t ddr5_bw,
                                        uint64_t pcie_bw, uint32_t current_tier);
    void RawrAggressiveStream(const void* src, void* dst, size_t bytes);
}

namespace rawr {

// ============================================================================
// TIERED OVERFLOW THRESHOLDS (64GB System)
// ============================================================================

enum class OverflowTier {
    NORMAL = 0,      // TIER_STEADY: VRAM handles all
    WARNING = 1,     // TIER_HYBRID: Keep KV hot, stage weights via aperture
    CRITICAL = 2,    // TIER_STRIDE: Stream via aggressive primitives
    PANIC = 3        // TIER_EMERGENCY: Compression + eviction + swap
};

struct TierConfig {
    float threshold_percent;
    size_t prefetch_depth;
    bool enable_compression;
    bool enable_nvme_swap;
    bool force_non_temporal;
    uint32_t pin_timeout_ms;
};

inline constexpr TierConfig TIER_CONFIGS[4] = {
    {0.60f, 1, false, false, false, 0},      // NORMAL
    {0.80f, 2, false, false, true, 100},     // WARNING
    {0.95f, 4, false, false, true, 50},      // CRITICAL
    {1.00f, 8, true, true, true, 0}          // PANIC
};

// ============================================================================
// DYNAMIC PINNING WITH TIMEOUT
// ============================================================================

struct PinnedBlock {
    void* ptr;
    size_t size;
    std::chrono::steady_clock::time_point pin_time;
    uint32_t timeout_ms;
    std::atomic<bool> active{true};
};

class DynamicPinManager {
private:
    std::vector<PinnedBlock> pinned_blocks_;
    mutable std::mutex mutex_;
    std::thread cleanup_thread_;
    std::atomic<bool> running_{false};
    
    void CleanupLoop() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            std::lock_guard<std::mutex> lock(mutex_);
            auto now = std::chrono::steady_clock::now();
            
            for (auto& block : pinned_blocks_) {
                if (!block.active) continue;
                
                if (block.timeout_ms > 0) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - block.pin_time).count();
                    
                    if (elapsed > block.timeout_ms) {
                        RawrUnpinMemory(block.ptr, block.size);
                        block.active = false;
                    }
                }
            }
            
            // Remove inactive blocks
            pinned_blocks_.erase(
                std::remove_if(pinned_blocks_.begin(), pinned_blocks_.end(),
                    [](const PinnedBlock& b) { return !b.active; }),
                pinned_blocks_.end()
            );
        }
    }
    
public:
    void Start() {
        if (!running_) {
            running_ = true;
            cleanup_thread_ = std::thread(&DynamicPinManager::CleanupLoop, this);
        }
    }
    
    void Stop() {
        if (running_) {
            running_ = false;
            if (cleanup_thread_.joinable()) {
                cleanup_thread_.join();
            }
            // Unpin all remaining blocks
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& block : pinned_blocks_) {
                if (block.active) {
                    RawrUnpinMemory(block.ptr, block.size);
                }
            }
            pinned_blocks_.clear();
        }
    }
    
    void PinWithTimeout(void* ptr, size_t size, uint32_t timeout_ms) {
        if (!RawrPinMemory(ptr, size)) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        pinned_blocks_.push_back({ptr, size, 
            std::chrono::steady_clock::now(), timeout_ms, true});
    }
    
    void Unpin(void* ptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& block : pinned_blocks_) {
            if (block.ptr == ptr && block.active) {
                RawrUnpinMemory(block.ptr, block.size);
                block.active = false;
                break;
            }
        }
    }
    
    size_t GetPinnedBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& block : pinned_blocks_) {
            if (block.active) total += block.size;
        }
        return total;
    }
    
    ~DynamicPinManager() {
        Stop();
    }
};

// ============================================================================
// ASYNC PREFETCH WORKER WITH LOOKAHEAD
// ============================================================================

class AggressivePrefetchWorker {
private:
    struct PrefetchRequest {
        void* ptr;
        size_t size;
        int priority;  // Higher = more urgent
    };
    
    std::thread worker_thread_;
    std::atomic<bool> running_{false};
    std::queue<PrefetchRequest> queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<size_t> lookahead_depth_{2};  // Default to optimal depth=2
    std::atomic<uint32_t> overflow_tier_{0};
    
    void WorkerLoop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { return !queue_.empty() || !running_; });
            
            if (!running_) break;
            
            // Process batch up to lookahead_depth
            std::vector<PrefetchRequest> batch;
            size_t count = 0;
            while (!queue_.empty() && count < lookahead_depth_.load()) {
                batch.push_back(queue_.front());
                queue_.pop();
                count++;
            }
            lock.unlock();
            
            // Execute prefetches
            for (const auto& req : batch) {
                RawrStreamingPrefetch(req.ptr, req.size, overflow_tier_.load());
            }
        }
    }
    
public:
    void Start() {
        if (!running_) {
            running_ = true;
            worker_thread_ = std::thread(&AggressivePrefetchWorker::WorkerLoop, this);
            RawrSetThreadAffinityToNUMA0();
        }
    }
    
    void Stop() {
        if (running_) {
            running_ = false;
            cv_.notify_all();
            if (worker_thread_.joinable()) {
                worker_thread_.join();
            }
        }
    }
    
    void SetLookaheadDepth(size_t depth) {
        lookahead_depth_.store(depth);
    }

    void SetTier(OverflowTier tier) {
        overflow_tier_.store(static_cast<uint32_t>(tier));
    }
    
    void QueuePrefetch(void* ptr, size_t size, int priority = 0) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push({ptr, size, priority});
        cv_.notify_one();
    }
    
    size_t GetQueueSize() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return queue_.size();
    }
    
    void ClearQueue() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::queue<PrefetchRequest> empty;
        std::swap(queue_, empty);
    }
    
    ~AggressivePrefetchWorker() {
        Stop();
    }
};

// ============================================================================
// COMPRESSION FALLBACK (LZ4 for PANIC tier)
// ============================================================================

class CompressionFallback {
private:
    void* compress_buffer_ = nullptr;
    size_t buffer_size_ = 0;
    
public:
    bool Initialize(size_t buffer_size) {
        buffer_size_ = buffer_size;
        compress_buffer_ = VirtualAlloc(NULL, buffer_size, 
                                       MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        return compress_buffer_ != nullptr;
    }
    
    // Simple RLE compression for expert weights (fast decompression)
    size_t CompressWeights(const float* weights, size_t count, uint8_t* output) {
        // Placeholder: real implementation would use LZ4 or similar
        // For now, just copy with quantization
        size_t out_pos = 0;
        for (size_t i = 0; i < count; i += 4) {
            // Pack 4 floats into 4 int8 values (aggressive quantization)
            for (size_t j = 0; j < 4 && (i + j) < count; j++) {
                float val = weights[i + j];
                int8_t q = static_cast<int8_t>(val * 127.0f);
                output[out_pos++] = static_cast<uint8_t>(q);
            }
        }
        return out_pos;
    }
    
    void DecompressToAperture(const uint8_t* compressed, size_t compressed_size,
                               float* output, size_t output_count) {
        // Dequantize back to floats
        for (size_t i = 0; i < output_count && i < compressed_size; i++) {
            int8_t q = static_cast<int8_t>(compressed[i]);
            output[i] = static_cast<float>(q) / 127.0f;
        }
    }
    
    ~CompressionFallback() {
        if (compress_buffer_) {
            VirtualFree(compress_buffer_, 0, MEM_RELEASE);
        }
    }
};

// ============================================================================
// AGGRESSIVE OVERFLOW MANAGER
// ============================================================================

class AggressiveOverflowManager {
private:
    size_t total_ram_ = 64ULL * 1024 * 1024 * 1024;  // 64GB
    size_t vram_budget_ = 14ULL * 1024 * 1024 * 1024; // 14GB (leave 2GB overhead)
    size_t aperture_budget_ = 44ULL * 1024 * 1024 * 1024; // 44GB for aperture
    
    std::atomic<size_t> ram_used_{0};
    std::atomic<size_t> vram_used_{0};
    std::atomic<size_t> aperture_used_{0};
    
    DynamicPinManager pin_manager_;
    AggressivePrefetchWorker prefetch_worker_;
    CompressionFallback compression_;
    
    OverflowTier current_tier_ = OverflowTier::NORMAL;
    std::atomic<bool> nvme_swap_enabled_{false};
    std::atomic<bool> compression_enabled_{false};

    // Predictive pressure tracking.
    std::atomic<float> growth_rate_{0.0f};
    std::atomic<size_t> last_vram_used_{0};

    static uint32_t FloatToBits(float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
    
public:
    bool Initialize() {
        pin_manager_.Start();
        prefetch_worker_.Start();
        
        // Initialize compression buffer (2GB for PANIC tier)
        if (!compression_.Initialize(2ULL * 1024 * 1024 * 1024)) {
            return false;
        }
        
        return true;
    }
    
    void Shutdown() {
        pin_manager_.Stop();
        prefetch_worker_.Stop();
    }

    // Predict where VRAM pressure will be in ~10 tokens.
    float CalculatePressure(size_t vram_used, float growth_rate) const {
        constexpr float lookahead_tokens = 10.0f;
        float projected = static_cast<float>(vram_used) + (growth_rate * lookahead_tokens);
        return projected / static_cast<float>(vram_budget_);
    }

    void AdjustPrimitives(float pressure) {
        if (pressure > 0.95f) {
            prefetch_worker_.SetLookaheadDepth(8);
            compression_enabled_.store(true);
        } else if (pressure > 0.80f) {
            prefetch_worker_.SetLookaheadDepth(4);
            compression_enabled_.store(false);
        } else {
            prefetch_worker_.SetLookaheadDepth(2);
            compression_enabled_.store(false);
        }
    }
    
    // Get current overflow tier based on RAM usage
    OverflowTier GetCurrentTier() const {
        float usage = static_cast<float>(ram_used_.load()) / static_cast<float>(total_ram_);
        uint32_t asm_tier = RawrCheckOverflowTierAggressive(FloatToBits(usage));

        OverflowTier tier_from_usage = OverflowTier::NORMAL;
        if (usage > TIER_CONFIGS[3].threshold_percent) tier_from_usage = OverflowTier::PANIC;
        else if (usage > TIER_CONFIGS[2].threshold_percent) tier_from_usage = OverflowTier::CRITICAL;
        else if (usage > TIER_CONFIGS[1].threshold_percent) tier_from_usage = OverflowTier::WARNING;

        OverflowTier tier_from_asm = static_cast<OverflowTier>(std::min<uint32_t>(asm_tier, 3));
        return (static_cast<int>(tier_from_asm) > static_cast<int>(tier_from_usage))
            ? tier_from_asm : tier_from_usage;
    }
    
    // Update tier and apply optimizations
    void UpdateTier() {
        size_t vram_now = vram_used_.load();
        size_t vram_prev = last_vram_used_.exchange(vram_now);
        float instant_growth = static_cast<float>(vram_now > vram_prev ? (vram_now - vram_prev) : 0);
        float smoothed_growth = (growth_rate_.load() * 0.7f) + (instant_growth * 0.3f);
        growth_rate_.store(smoothed_growth);

        float pressure = CalculatePressure(vram_now, smoothed_growth);
        OverflowTier new_tier = GetCurrentTier();
        if (pressure > 0.95f) new_tier = OverflowTier::PANIC;
        else if (pressure > 0.80f && new_tier < OverflowTier::CRITICAL) new_tier = OverflowTier::CRITICAL;

        if (new_tier == current_tier_) return;
        
        current_tier_ = new_tier;
        const auto& config = TIER_CONFIGS[static_cast<int>(new_tier)];
        
        // Apply tier-specific optimizations
        prefetch_worker_.SetLookaheadDepth(config.prefetch_depth);
        prefetch_worker_.SetTier(new_tier);
        AdjustPrimitives(pressure);
        
        if (new_tier == OverflowTier::PANIC && !nvme_swap_enabled_.load()) {
            EnableNVMeSwap();
        }
        
        // Log tier change
        std::cerr << "[OverflowManager] Tier changed to " 
                  << static_cast<int>(new_tier) 
                  << " (RAM usage: " 
                  << (100.0f * ram_used_.load() / total_ram_) << "%)" << std::endl;
    }
    
    // Allocate tensor with tier-aware strategy
    void* AllocateTensor(size_t size, bool is_critical = false) {
        UpdateTier();
        
        const auto& config = TIER_CONFIGS[static_cast<int>(current_tier_)];
        
        // Try VRAM first for critical tensors
        if (is_critical && vram_used_.load() + size < vram_budget_) {
            vram_used_ += size;
            // Return VRAM allocation (would use CUDA/HIP in real impl)
            return nullptr; // Placeholder
        }
        
        // Use aperture pool
        auto& bridge = GetSovereignBridge();
        void* ptr = bridge.AllocateApertureSpace(size);
        
        if (ptr) {
            aperture_used_ += size;
            ram_used_ += size;
            
            // Apply pinning with timeout for WARNING+ tiers
            if (current_tier_ >= OverflowTier::WARNING && config.pin_timeout_ms > 0) {
                pin_manager_.PinWithTimeout(ptr, size, config.pin_timeout_ms);
            }
        }
        
        return ptr;
    }
    
    // Stage tensor for GPU with aggressive prefetch
    void* StageTensor(void* ptr, size_t size, bool is_expert = false) {
        const auto& config = TIER_CONFIGS[static_cast<int>(current_tier_)];
        
        // Queue prefetch with priority
        int priority = is_expert ? 10 : 5;
        prefetch_worker_.QueuePrefetch(ptr, size, priority);

        if (compression_enabled_.load() && config.enable_compression) {
            std::vector<uint8_t> compressed(size);
            size_t compressed_size = RawrEmergencyTensorCompress(
                ptr, compressed.data(), size, static_cast<uint32_t>(current_tier_));
            if (compressed_size > 0 && compressed_size < size) {
                std::memcpy(ptr, compressed.data(), compressed_size);
            }
        }
        
        // Immediate activation for CRITICAL/PANIC
        if (current_tier_ >= OverflowTier::CRITICAL) {
            auto& bridge = GetSovereignBridge();
            char* pool = static_cast<char*>(bridge.PoolBase());
            bool in_pool = pool &&
                (ptr >= pool) &&
                (ptr < (pool + bridge.PoolSize()));

            if (!in_pool) {
                void* dst = bridge.AllocateApertureSpace(size);
                if (dst) {
                    RawrAggressiveStream(ptr, dst, size);
                    ptr = dst;
                    aperture_used_ += size;
                    ram_used_ += size;
                }
            }

            RawrFlushCacheLines(ptr, std::min<size_t>(size, 256ULL * 1024 * 1024));
            RawrMemoryBarrier();
            return bridge.ActivateAperture(ptr, size);
        }
        
        return ptr;
    }

    // Primitive fast-path for float/int8/int16/etc blocks.
    template<typename T>
    T* StagePrimitive(T* ptr, size_t count, bool is_expert = false) {
        static_assert(std::is_trivially_copyable<T>::value,
            "StagePrimitive requires trivially-copyable primitive types");
        return static_cast<T*>(StageTensor(static_cast<void*>(ptr), count * sizeof(T), is_expert));
    }

    template<typename T>
    void StagePrimitiveBatch(T** ptrs, const size_t* counts, size_t n, bool is_expert = false) {
        static_assert(std::is_trivially_copyable<T>::value,
            "StagePrimitiveBatch requires trivially-copyable primitive types");
        if (!ptrs || !counts) return;
        for (size_t i = 0; i < n; ++i) {
            if (!ptrs[i]) continue;
            (void)StagePrimitive(ptrs[i], counts[i], is_expert);
        }
    }
    
    // Get prefetch depth for current tier
    size_t GetPrefetchDepth() const {
        return TIER_CONFIGS[static_cast<int>(current_tier_)].prefetch_depth;
    }
    
    // Statistics
    struct Stats {
        size_t ram_used;
        size_t ram_total;
        size_t vram_used;
        size_t vram_total;
        size_t aperture_used;
        size_t pinned_bytes;
        size_t prefetch_queue_size;
        OverflowTier tier;
    };
    
    Stats GetStats() const {
        return {
            ram_used_.load(),
            total_ram_,
            vram_used_.load(),
            vram_budget_,
            aperture_used_.load(),
            pin_manager_.GetPinnedBytes(),
            prefetch_worker_.GetQueueSize(),
            current_tier_
        };
    }
    
private:
    void EnableNVMeSwap() {
        nvme_swap_enabled_.store(true);
        std::cerr << "[OverflowManager] NVMe swap enabled for PANIC tier" << std::endl;
        // Would initialize swap file here
    }
};

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

inline AggressiveOverflowManager& GetOverflowManager() {
    static AggressiveOverflowManager manager;
    return manager;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

inline bool InitializeAggressiveOverflow() {
    return GetOverflowManager().Initialize();
}

inline void ShutdownAggressiveOverflow() {
    GetOverflowManager().Shutdown();
}

inline void* AllocateTensorAggressive(size_t size, bool is_critical = false) {
    return GetOverflowManager().AllocateTensor(size, is_critical);
}

inline void* StageTensorAggressive(void* ptr, size_t size, bool is_expert = false) {
    return GetOverflowManager().StageTensor(ptr, size, is_expert);
}

template<typename T>
inline T* StagePrimitiveAggressive(T* ptr, size_t count, bool is_expert = false) {
    return GetOverflowManager().StagePrimitive(ptr, count, is_expert);
}

inline OverflowTier GetCurrentOverflowTier() {
    return GetOverflowManager().GetCurrentTier();
}

} // namespace rawr
