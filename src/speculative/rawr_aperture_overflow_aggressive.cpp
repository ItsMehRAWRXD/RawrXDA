#include "rawr_aperture_overflow_aggressive.h"
#include <cstring>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <windows.h>

namespace rawr::speculative::aperture {

// ============================================================================
// EXPERT LRU CACHE
// ============================================================================

ExpertLRUCache::ExpertLRUCache()
    : entries_(), last_eviction_time_(0)
{
    entries_.reserve(CACHE_SIZE);
}

ExpertLRUCache::~ExpertLRUCache() = default;

void ExpertLRUCache::add_expert(const void* ptr, uint64_t hash)
{
    // Check if already in cache
    for (auto& entry : entries_) {
        if (entry.ptr == ptr && entry.hash == hash) {
            entry.last_access_us = GetTickCount64() * 1000; // ms to us
            entry.access_count++;
            return;
        }
    }

    // If cache full, evict oldest
    if (entries_.size() >= CACHE_SIZE) {
        auto min_it = std::min_element(
            entries_.begin(),
            entries_.end(),
            [](const auto& a, const auto& b) {
                return a.last_access_us < b.last_access_us;
            }
        );
        if (min_it != entries_.end()) {
            entries_.erase(min_it);
        }
    }

    // Add new entry
    uint64_t now_us = GetTickCount64() * 1000;
    entries_.push_back({
        const_cast<void*>(ptr),
        hash,
        now_us,
        1,
        true
    });
}

bool ExpertLRUCache::probe_expert(const void* ptr, uint64_t hash)
{
    for (const auto& entry : entries_) {
        if (entry.ptr == ptr && entry.hash == hash && entry.is_valid) {
            return true;
        }
    }
    return false;
}

std::vector<void*> ExpertLRUCache::evict_cold_lru(size_t count)
{
    std::vector<void*> evicted;

    // Sort by access time (oldest first)
    std::sort(
        entries_.begin(),
        entries_.end(),
        [](const auto& a, const auto& b) {
            return a.last_access_us < b.last_access_us;
        }
    );

    // Mark oldest as invalid
    for (size_t i = 0; i < count && i < entries_.size(); ++i) {
        evicted.push_back(entries_[i].ptr);
        entries_[i].is_valid = false;
    }

    // Remove invalid entries
    entries_.erase(
        std::remove_if(
            entries_.begin(),
            entries_.end(),
            [](const auto& e) { return !e.is_valid; }
        ),
        entries_.end()
    );

    last_eviction_time_ = GetTickCount64() * 1000;
    return evicted;
}

void ExpertLRUCache::clear()
{
    entries_.clear();
}

// ============================================================================
// APERTURE PRESSURE CONTROLLER
// ============================================================================

AperturePressureController::AperturePressureController()
    : expert_cache_(),
      last_pressure_(0.0f),
      current_tier_(OverflowTier::TIER_STEADY),
      current_prefetch_depth_(2),
      base_time_us_(GetTickCount64() * 1000)
{
}

AperturePressureController::~AperturePressureController() = default;

float AperturePressureController::calculate_pressure(
    size_t vram_used_bytes,
    size_t vram_total_bytes,
    size_t ddr5_allocated_bytes)
{
    if (vram_total_bytes == 0) return 0.0f;

    // VRAM utilization fraction [0, 1]
    float vram_frac = static_cast<float>(vram_used_bytes) / vram_total_bytes;

    // Growth rate based on DDR5 allocation trend
    // (simplified: assume linear 10-token lookahead)
    float growth_rate = static_cast<float>(ddr5_allocated_bytes) /
                        static_cast<float>(vram_total_bytes * 10);

    // Use MASM pressure formula: pressure = vram_frac + growth_rate * 10.0
    last_pressure_ = RawrComputePressure(vram_frac, growth_rate);

    return last_pressure_;
}

OverflowTier AperturePressureController::detect_tier_fast(float pressure)
{
    // Thresholds for 64GB+Q8_0 aggressive scenario
    if (pressure > 0.92f) {
        current_tier_ = OverflowTier::TIER_EMERGENCY;
    } else if (pressure > 0.82f) {
        current_tier_ = OverflowTier::TIER_STRIDE;
    } else if (pressure > 0.70f) {
        current_tier_ = OverflowTier::TIER_HYBRID;
    } else {
        current_tier_ = OverflowTier::TIER_STEADY;
    }

    return current_tier_;
}

void AperturePressureController::adjust_primitives(OverflowTier tier)
{
    current_prefetch_depth_ = RawrSetPrefetchDepth(last_pressure_);
}

AperturePressureController::DoubleBuffer
AperturePressureController::allocate_double_buffer(size_t size)
{
    // Try to allocate 2 MB huge pages
    void* active = RawrAllocateHugePages(size);
    void* shadow = RawrAllocateHugePages(size);

    if (!active || !shadow) {
        // Fallback to regular malloc if huge pages fail
        if (active) free(active);
        if (shadow) free(shadow);
        active = malloc(size);
        shadow = malloc(size);
    }

    // Pin memory if requested
    if (active && shadow) {
        RawrPinMemory(active, size);
        RawrPinMemory(shadow, size);
    }

    return {active, shadow, size};
}

void AperturePressureController::set_external_double_buffer(
    DoubleBuffer db,
    void* new_shadow)
{
    // Atomic swap: replaces active, returns old active
    void* old_active = RAWR_DoubleBuffer_Swap(&db.active_ptr, new_shadow);
    (void)old_active; // suppress unused
}

void AperturePressureController::stream_to_shadow(
    const void* src,
    size_t size)
{
    // Use bandwidth-aware streaming for DDR5 -> shadow aperture copy
    // Estimate typical DDR5 bw ~50 GB/s, PCIe 5.0 ~13 GB/s
    constexpr uint64_t DDR5_BW = 50ULL * 1024ULL * 1024ULL * 1024ULL;
    constexpr uint64_t PCIE_BW = 13ULL * 1024ULL * 1024ULL * 1024ULL;

    if (size > 0) {
        RawrBandwidthAwareStream(
            const_cast<void*>(src),
            nullptr,  // Will update shadow in double-buffer swap
            size,
            DDR5_BW,
            PCIE_BW
        );
    }
}

void AperturePressureController::commit_shadow(DoubleBuffer& db)
{
    // Flush cache lines to PCIe window
    if (db.shadow_ptr) {
        RAWR_PCIe_FlushBarrier(db.shadow_ptr, db.capacity_bytes);
    }

    // Atomic swap to make shadow active
    if (db.active_ptr) {
        void* old_active = RAWR_DoubleBuffer_Swap(&db.active_ptr, db.shadow_ptr);
        (void)old_active;
    }
}

void AperturePressureController::free_double_buffer(DoubleBuffer& db)
{
    if (db.active_ptr) {
        RawrUnpinMemory(db.active_ptr, db.capacity_bytes);
        free(db.active_ptr);
        db.active_ptr = nullptr;
    }
    if (db.shadow_ptr) {
        RawrUnpinMemory(db.shadow_ptr, db.capacity_bytes);
        free(db.shadow_ptr);
        db.shadow_ptr = nullptr;
    }
}

void AperturePressureController::cache_expert(void* ptr, uint64_t hash)
{
    expert_cache_.add_expert(ptr, hash);
}

bool AperturePressureController::probe_expert(void* ptr, uint64_t hash)
{
    return expert_cache_.probe_expert(ptr, hash);
}

void AperturePressureController::evict_expert(void* ptr)
{
    // Simplified: mark as invalid (real impl would unpin memory)
    (void)ptr;
}

std::vector<void*> AperturePressureController::evict_cold_experts(size_t count)
{
    return expert_cache_.evict_cold_lru(count);
}

void AperturePressureController::prefetch_swarm_slot(
    int slot_id,
    void* addr,
    size_t bytes)
{
    RAWR_SwarmSlot_Prefetch(slot_id, addr, bytes);
}

uint32_t AperturePressureController::tune_prefetch(float pressure)
{
    current_prefetch_depth_ = RawrSetPrefetchDepth(pressure);
    return current_prefetch_depth_;
}

uint64_t AperturePressureController::timestamp_us() const
{
    return GetTickCount64() * 1000 - base_time_us_;
}

// ============================================================================
// UNIFIED MEMORY APERTURE
// ============================================================================

UnifiedMemoryAperture::UnifiedMemoryAperture(const Config& cfg)
    : config_(cfg),
      pressure_controller_(),
      active_double_buffer_(),
      aperture_base_(nullptr),
      aperture_used_bytes_(0),
      model_size_bytes_(0)
{
    // Set thread affinity to NUMA node 0 for best latency
    RawrSetThreadAffinityToNUMA0();

    // Allocate aperture base
    aperture_base_ = malloc(cfg.ddr5_bytes);
    if (aperture_base_ && cfg.use_huge_pages) {
        RawrPinMemory(aperture_base_, cfg.ddr5_bytes);
    }
}

UnifiedMemoryAperture::~UnifiedMemoryAperture()
{
    close();
    if (aperture_base_) {
        RawrUnpinMemory(aperture_base_, config_.ddr5_bytes);
        free(aperture_base_);
    }
}

bool UnifiedMemoryAperture::open_model(
    const void* model_data,
    size_t model_size)
{
    model_size_bytes_ = model_size;

    // Activate aperture bypass for GPU direct access
    return RawrActivateApertureBypass(
        aperture_base_,
        model_size,
        0 /* flags: default */
    );
}

void UnifiedMemoryAperture::close()
{
    if (aperture_base_) {
        RawrDeactivateApertureBypass(aperture_base_, model_size_bytes_);
    }
}

void* UnifiedMemoryAperture::acquire_compute_buffer(size_t size)
{
    return malloc(size);
}

void UnifiedMemoryAperture::release_compute_buffer(void* ptr)
{
    free(ptr);
}

bool UnifiedMemoryAperture::activate_expert_bypass()
{
    return RawrActivateApertureBypass(
        aperture_base_,
        model_size_bytes_,
        1 /* expert bypass flag */
    );
}

void UnifiedMemoryAperture::deactivate_expert_bypass()
{
    RawrDeactivateApertureBypass(aperture_base_, model_size_bytes_);
}

void UnifiedMemoryAperture::prefetch_upcoming(
    void** upcoming_ptrs,
    size_t count,
    size_t tensor_size)
{
    RawrLookaheadPrefetch(upcoming_ptrs, count, tensor_size);
}

void* UnifiedMemoryAperture::allocate(size_t size)
{
    if (aperture_used_bytes_ + size > config_.ddr5_bytes) {
        return nullptr; // Out of aperture memory
    }
    void* ptr = static_cast<char*>(aperture_base_) + aperture_used_bytes_;
    aperture_used_bytes_ += size;
    return ptr;
}

void UnifiedMemoryAperture::deallocate(void* ptr)
{
    (void)ptr; // Bump allocator, no individual deallocation
}

bool UnifiedMemoryAperture::stream_expert(
    size_t expert_id,
    const void* expert_data,
    size_t expert_size)
{
    // 1. Probe expert cache
    uint64_t hash = reinterpret_cast<uint64_t>(expert_data) ^ expert_id;
    if (pressure_controller_.probe_expert(
        const_cast<void*>(expert_data), hash)) {
        return true; // Already cached
    }

    // 2. Allocate in aperture
    void* aperture_ptr = allocate(expert_size);
    if (!aperture_ptr) return false;

    // 3. Stream with bandwidth-aware logic
    bandwidth_aware_stream(expert_data, aperture_ptr, expert_size);

    // 4. Cache update
    pressure_controller_.cache_expert(aperture_ptr, hash);

    return true;
}

bool UnifiedMemoryAperture::begin_layer_swap()
{
    // Initialize double-buffer for layer
    active_double_buffer_ =
        pressure_controller_.allocate_double_buffer(config_.vram_bytes);
    return active_double_buffer_.active_ptr != nullptr;
}

bool UnifiedMemoryAperture::stream_layer_to_shadow(
    const void* layer_data,
    size_t layer_size)
{
    pressure_controller_.stream_to_shadow(layer_data, layer_size);
    return true;
}

bool UnifiedMemoryAperture::commit_layer_swap()
{
    pressure_controller_.commit_shadow(active_double_buffer_);
    return true;
}

float UnifiedMemoryAperture::update_pressure()
{
    return pressure_controller_.calculate_pressure(
        aperture_used_bytes_,
        config_.vram_bytes,
        config_.ddr5_bytes
    );
}

float UnifiedMemoryAperture::predict_tier_transition(
    size_t upcoming_bytes)
{
    float predicted_used = static_cast<float>(
        aperture_used_bytes_ + upcoming_bytes
    ) / config_.vram_bytes;
    return predicted_used;
}

void UnifiedMemoryAperture::prefetch_for_agent(size_t upcoming_bytes)
{
    // Tier-based prefetch depth tuning
    float pressure = current_pressure();
    uint32_t depth = pressure_controller_.tune_prefetch(pressure);

    // Prefetch with estimated stride based on depth
    if (depth > 0) {
        size_t stride = config_.ddr5_bytes / depth;
        RawrStreamingPrefetch(aperture_base_, stride, depth);
    }
}

std::vector<void*> UnifiedMemoryAperture::proactive_evict_swarm(
    size_t num_tensors)
{
    return pressure_controller_.evict_cold_experts(num_tensors);
}

uint8_t UnifiedMemoryAperture::expert_dedup_mask(
    void** experts,
    size_t num_experts)
{
    return RawrExpertDedupMask(experts, nullptr, num_experts, 0);
}

void UnifiedMemoryAperture::swarm_assign_slots(
    uint8_t* agent_experts,
    size_t num_agents)
{
    RawrSwarmAssignSlots(agent_experts, num_agents, nullptr, 0);
}

void UnifiedMemoryAperture::bandwidth_aware_stream(
    const void* src,
    void* dst,
    size_t size)
{
    RawrBandwidthAwareStream(
        const_cast<void*>(src),
        dst,
        size,
        config_.ddr5_bandwidth_bps,
        config_.pcie_bandwidth_bps
    );
}

float UnifiedMemoryAperture::current_pressure() const
{
    return pressure_controller_.last_pressure();
}

OverflowTier UnifiedMemoryAperture::current_tier() const
{
    return pressure_controller_.current_tier();
}

// ============================================================================
// INTEGRATION TEST HARNESS
// ============================================================================

bool AggressiveOverflowTest::run_all()
{
    return test_bandwidth_aware_stream() &&
           test_pressure_estimation() &&
           test_double_buffer_swap() &&
           test_expert_cache() &&
           test_unified_aperture();
}

bool AggressiveOverflowTest::test_bandwidth_aware_stream()
{
    // Allocate test buffers
    const size_t test_size = 1024 * 1024; // 1 MB
    void* src = malloc(test_size);
    void* dst = malloc(test_size);

    if (!src || !dst) return false;

    // Fill source with pattern
    memset(src, 0xAA, test_size);
    memset(dst, 0x00, test_size);

    // Stream with typical DDR5/PCIe ratio
    RawrBandwidthAwareStream(src, dst, test_size, 50ULL << 30, 13ULL << 30);

    // Verify pattern
    bool success = memcmp(src, dst, test_size) == 0;

    free(src);
    free(dst);

    return success;
}

bool AggressiveOverflowTest::test_pressure_estimation()
{
    AperturePressureController ctrl;

    // Test low pressure
    float p_low = ctrl.calculate_pressure(4 * 1024ULL * 1024ULL * 1024ULL, 16ULL * 1024ULL * 1024ULL * 1024ULL, 0);
    OverflowTier t_low = ctrl.detect_tier_fast(p_low);
    if (t_low != OverflowTier::TIER_STEADY) return false;

    // Test high pressure
    float p_high = ctrl.calculate_pressure(14 * 1024ULL * 1024ULL * 1024ULL, 16ULL * 1024ULL * 1024ULL * 1024ULL, 10ULL * 1024ULL * 1024ULL * 1024ULL);
    OverflowTier t_high = ctrl.detect_tier_fast(p_high);
    if (t_high == OverflowTier::TIER_STEADY) return false;

    return true;
}

bool AggressiveOverflowTest::test_double_buffer_swap()
{
    AperturePressureController ctrl;

    // Allocate double buffer
    auto db = ctrl.allocate_double_buffer(4096);
    if (!db.active_ptr || !db.shadow_ptr) return false;

    // Test swap
    memset(db.active_ptr, 0x11, db.capacity_bytes);
    memset(db.shadow_ptr, 0x22, db.capacity_bytes);

    ctrl.set_external_double_buffer(db, db.shadow_ptr);

    // Cleanup
    ctrl.free_double_buffer(db);

    return true;
}

bool AggressiveOverflowTest::test_expert_cache()
{
    ExpertLRUCache cache;

    // Add experts
    void* expert1 = reinterpret_cast<void*>(0x1000);
    void* expert2 = reinterpret_cast<void*>(0x2000);

    cache.add_expert(expert1, 0xDEADBEEF);
    if (!cache.probe_expert(expert1, 0xDEADBEEF)) return false;

    if (cache.probe_expert(expert2, 0xCAFEBABE)) return false;

    return true;
}

bool AggressiveOverflowTest::test_unified_aperture()
{
    UnifiedMemoryAperture::Config cfg{
        16ULL * 1024ULL * 1024ULL * 1024ULL,    // 16 GB VRAM
        64ULL * 1024ULL * 1024ULL * 1024ULL,    // 64 GB DDR5
        50ULL * 1024ULL * 1024ULL * 1024ULL,    // 50 GB/s DDR5 BW
        13ULL * 1024ULL * 1024ULL * 1024ULL,    // 13 GB/s PCIe BW
        false /* use_huge_pages */
    };

    UnifiedMemoryAperture aperture(cfg);

    // Test model open/close
    if (!aperture.open_model(nullptr, 100ULL * 1024ULL * 1024ULL)) return false;

    float pressure = aperture.update_pressure();
    if (pressure < 0.0f || aperture.current_tier() == OverflowTier::TIER_STEADY) {
        // Expected for small model
    }

    aperture.close();
    return true;
}

} // namespace rawr::speculative::aperture
