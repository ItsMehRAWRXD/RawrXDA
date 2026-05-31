// rawr_aggressive_aperture.h
// Aggressive asynchronous aperture staging with speculative GART saturation
// Targets 7800 XT PCIe 4.0 x16 bandwidth saturation (31.5 GB/s)

#pragma once
#include "rawr_sovereign_bridge.h"
#include "rawr_architecture_agnostic_runtime.h"
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>

namespace rawr {

// ============================================================================
// SOVEREIGN TENSOR (Aperture-managed weight tensor)
// ============================================================================

struct SovereignTensor {
    void* ddr5_addr = nullptr;      // Base address in DDR5 pool
    void* gart_addr = nullptr;      // GPU-visible aperture address
    size_t size = 0;                // Tensor size in bytes
    bool in_aperture = false;       // Currently mapped to GART
    bool is_pinned = false;           // Memory is pinned
    uint64_t last_access = 0;       // LRU tracking
    uint32_t access_count = 0;      // Frequency tracking
    std::string name;               // Tensor identifier
    
    // Capability flags
    bool is_expert = false;         // MoE expert weight
    bool is_attention = false;      // Attention weight (hot path)
    bool is_ffn = false;           // FFN weight
    
    void Stage() {
        if (!in_aperture && ddr5_addr) {
            // 1. Speculative prefetch (non-temporal)
            RawrPrefetchMemory(ddr5_addr, std::min(size, 256ULL * 1024 * 1024));
            
            // 2. Coherency flush
            RawrFlushCacheLines(ddr5_addr, std::min(size, 512ULL * 1024 * 1024));
            
            // 3. Memory barrier
            RawrMemoryBarrier();
            
            gart_addr = ddr5_addr;  // GART sees same address
            in_aperture = true;
        }
    }
    
    void Release() {
        if (in_aperture) {
            in_aperture = false;
            gart_addr = nullptr;
        }
    }
};

// ============================================================================
// TIERED OVERFLOW MANAGEMENT
// ============================================================================

enum OverflowTier {
    TIER_NORMAL = 0,      // < 75% utilization - standard VRAM
    TIER_WARNING = 1,     // 75-85% - start prefetch
    TIER_THROTTLE = 2,    // 85-95% - enable bypass
    TIER_CRITICAL = 3     // > 95% - direct DDR5 path
};

inline OverflowTier CheckOverflowTier(float utilization) {
    if (utilization > 0.95f) return TIER_CRITICAL;
    if (utilization > 0.85f) return TIER_THROTTLE;
    if (utilization > 0.75f) return TIER_WARNING;
    return TIER_NORMAL;
}

// ============================================================================
// SPECULATIVE PREFETCH QUEUE
// ============================================================================

struct PrefetchRequest {
    SovereignTensor* tensor;
    OverflowTier priority;
    uint64_t timestamp;
    
    bool operator<(const PrefetchRequest& other) const {
        return priority < other.priority;  // Higher tier = higher priority
    }
};

// ============================================================================
// AGGRESSIVE APERTURE MANAGER
// ============================================================================

class AggressiveApertureManager {
private:
    // Thread pool for async staging
    std::vector<std::thread> workers_;
    std::priority_queue<PrefetchRequest> prefetch_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> access_counter_{0};
    
    // Tensor registry
    std::unordered_map<std::string, std::unique_ptr<SovereignTensor>> tensors_;
    std::mutex tensor_mutex_;
    
    // Aperture statistics
    std::atomic<size_t> aperture_used_{0};
    std::atomic<size_t> vram_used_{0};
    std::atomic<size_t> staging_active_{0};
    
    // Configuration
    static constexpr size_t VRAM_BUDGET = 14ULL * 1024 * 1024 * 1024;  // 14GB
    static constexpr size_t APERTURE_BUDGET = 180ULL * 1024 * 1024 * 1024; // 180GB
    static constexpr size_t MAX_CONCURRENT_STAGING = 4;
    
public:
    AggressiveApertureManager(size_t num_workers = 4) {
        running_ = true;
        for (size_t i = 0; i < num_workers; i++) {
            workers_.emplace_back(&AggressiveApertureManager::WorkerThread, this);
        }
    }
    
    ~AggressiveApertureManager() {
        running_ = false;
        queue_cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }
    
    // Register a tensor for aperture management
    SovereignTensor* RegisterTensor(const std::string& name, void* ddr5_addr, 
                                     size_t size, uint64_t caps) {
        std::lock_guard<std::mutex> lock(tensor_mutex_);
        
        auto tensor = std::make_unique<SovereignTensor>();
        tensor->name = name;
        tensor->ddr5_addr = ddr5_addr;
        tensor->size = size;
        tensor->is_pinned = true;  // Assume pinned by sovereign bridge
        
        // Classify by capability
        tensor->is_expert = (caps & CAP_MOE) != 0;
        tensor->is_attention = (caps & (CAP_MHA | CAP_GQA | CAP_MQA)) != 0;
        tensor->is_ffn = (caps & (CAP_SWIGLU | CAP_DENSE_FFN)) != 0;
        
        auto* ptr = tensor.get();
        tensors_[name] = std::move(tensor);
        
        // Determine placement
        if (ShouldUseAperture(size)) {
            aperture_used_ += size;
        } else {
            vram_used_ += size;
        }
        
        return ptr;
    }
    
    // Stage tensor for GPU access (synchronous - blocking)
    void StageTensorForGPU(SovereignTensor* tensor) {
        if (!tensor || tensor->in_aperture) return;
        
        staging_active_++;
        tensor->Stage();
        staging_active_--;
        tensor->last_access = ++access_counter_;
        tensor->access_count++;
    }
    
    // Async staging - queue for background prefetch
    void StageTensorAsync(SovereignTensor* tensor, OverflowTier priority = TIER_WARNING) {
        if (!tensor || tensor->in_aperture) return;
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        prefetch_queue_.push({tensor, priority, ++access_counter_});
        queue_cv_.notify_one();
    }
    
    // Speculative staging for MoE experts
    void SpeculativeStageExperts(const std::vector<SovereignTensor*>& experts, 
                                  const std::vector<float>& router_probs) {
        // Sort experts by router probability (descending)
        std::vector<std::pair<float, SovereignTensor*>> ranked;
        for (size_t i = 0; i < experts.size() && i < router_probs.size(); i++) {
            ranked.push_back({router_probs[i], experts[i]});
        }
        std::sort(ranked.begin(), ranked.end(), 
                  [](auto& a, auto& b) { return a.first > b.first; });
        
        // Stage top-k experts with decreasing priority
        for (size_t i = 0; i < ranked.size() && i < 4; i++) {
            OverflowTier tier = (i < 2) ? TIER_THROTTLE : TIER_WARNING;
            StageTensorAsync(ranked[i].second, tier);
        }
    }
    
    // Pre-execution hook for DAG dispatch
    void* PreExecuteHook(const std::string& tensor_name) {
        std::lock_guard<std::mutex> lock(tensor_mutex_);
        
        auto it = tensors_.find(tensor_name);
        if (it == tensors_.end()) return nullptr;
        
        auto* tensor = it->second.get();
        
        // Fast path: already in aperture
        if (tensor->in_aperture) {
            tensor->last_access = ++access_counter_;
            return tensor->gart_addr;
        }
        
        // Slow path: need to stage
        StageTensorForGPU(tensor);
        return tensor->gart_addr;
    }
    
    // Release aperture for LRU eviction
    void ReleaseAperture(SovereignTensor* tensor) {
        if (tensor && tensor->in_aperture) {
            tensor->Release();
            aperture_used_ -= tensor->size;
        }
    }
    
    // Evict oldest tensors to make room
    void EvictOldest(size_t needed_bytes) {
        std::lock_guard<std::mutex> lock(tensor_mutex_);
        
        std::vector<SovereignTensor*> candidates;
        for (auto& [name, tensor] : tensors_) {
            if (tensor->in_aperture) {
                candidates.push_back(tensor.get());
            }
        }
        
        // Sort by last access (LRU)
        std::sort(candidates.begin(), candidates.end(),
                  [](auto* a, auto* b) { return a->last_access < b->last_access; });
        
        // Evict until we have enough space
        size_t freed = 0;
        for (auto* tensor : candidates) {
            if (freed >= needed_bytes) break;
            ReleaseAperture(tensor);
            freed += tensor->size;
        }
    }
    
    // Getters
    size_t GetApertureUsed() const { return aperture_used_.load(); }
    size_t GetVRAMUsed() const { return vram_used_.load(); }
    size_t GetStagingActive() const { return staging_active_.load(); }
    float GetUtilization() const {
        return static_cast<float>(vram_used_.load()) / VRAM_BUDGET;
    }
    
    // Statistics
    void GetStats(size_t& aperture_used, size_t& vram_used, 
                  size_t& staging_active, float& utilization) {
        aperture_used = aperture_used_.load();
        vram_used = vram_used_.load();
        staging_active = staging_active_.load();
        utilization = GetUtilization();
    }
    
private:
    bool ShouldUseAperture(size_t size) const {
        size_t current_vram = vram_used_.load();
        if (current_vram + size > VRAM_BUDGET) return true;
        if (size > 500ULL * 1024 * 1024) return true;  // Large tensors
        return false;
    }
    
    void WorkerThread() {
        // Pin to NUMA node 0 for local DDR5 access
        RawrSetThreadAffinityToNUMA0();
        
        while (running_) {
            PrefetchRequest req;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] { 
                    return !prefetch_queue_.empty() || !running_; 
                });
                
                if (!running_) break;
                if (prefetch_queue_.empty()) continue;
                
                req = prefetch_queue_.top();
                prefetch_queue_.pop();
            }
            
            // Process request
            if (req.tensor && !req.tensor->in_aperture) {
                // Tier-aware staging
                switch (req.priority) {
                    case TIER_CRITICAL:
                        // Immediate blocking staging
                        StageTensorForGPU(req.tensor);
                        break;
                    case TIER_THROTTLE:
                        // Fast async staging
                        RawrPrefetchMemory(req.tensor->ddr5_addr, 
                                          std::min(req.tensor->size, 128ULL * 1024 * 1024));
                        RawrFlushCacheLines(req.tensor->ddr5_addr, 64 * 1024 * 1024);
                        req.tensor->in_aperture = true;
                        break;
                    case TIER_WARNING:
                    default:
                        // Standard staging
                        RawrPrefetchMemory(req.tensor->ddr5_addr, 64 * 1024 * 1024);
                        req.tensor->in_aperture = true;
                        break;
                }
            }
        }
    }
};

// ============================================================================
// HETEROGENEOUS DISPATCH TABLE
// ============================================================================

// Extended dispatch entry with aperture support
struct ApertureDispatchEntry {
    KernelFn kernel;
    SovereignTensor* weight_tensor;
    bool requires_staging;
    OverflowTier staging_tier;
};

// Global manager instance
inline AggressiveApertureManager& GetApertureManager() {
    static AggressiveApertureManager manager(4);  // 4 worker threads
    return manager;
}

// ============================================================================
// APERTURE-AWARE DAG EXECUTOR
// ============================================================================

class ApertureAwareExecutor {
public:
    // Execute node with automatic aperture staging
    static void ExecuteNode(ExecNode& node, Context* ctx) {
        auto& manager = GetApertureManager();
        
        // Check if this node needs aperture staging
        if (node.caps & RAWR_CAP_APERTURE) {
            // Pre-execution hook: stage weights
            void* weight_ptr = manager.PreExecuteHook(node.weight_name);
            if (weight_ptr) {
                ctx->weights = weight_ptr;  // Use GART-mapped address
            }
        }
        
        // Execute kernel
        if (node.kernel) {
            node.kernel(ctx);
        }
    }
    
    // Speculative expert staging for MoE
    static void SpeculativeExpertStage(const std::vector<std::string>& expert_names,
                                        const std::vector<float>& probs) {
        auto& manager = GetApertureManager();
        
        std::vector<SovereignTensor*> experts;
        for (const auto& name : expert_names) {
            // Lookup tensor (would need registry access)
            // experts.push_back(manager.GetTensor(name));
        }
        
        manager.SpeculativeStageExperts(experts, probs);
    }
};

// ============================================================================
// INITIALIZATION
// ============================================================================

inline bool InitializeAggressiveAperture(size_t aperture_size_gb = 128) {
    // Initialize base sovereign bridge
    if (!InitializeSovereignBridge(aperture_size_gb)) {
        return false;
    }
    
    // Aperture manager is lazily initialized via GetApertureManager()
    return true;
}

inline void ShutdownAggressiveAperture() {
    // Manager destructor handles cleanup
    ShutdownSovereignBridge();
}

} // namespace rawr
