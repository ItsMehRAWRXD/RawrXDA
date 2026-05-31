#include "rawr_memory_aperture.h"

#include <algorithm>
#include <cstring>

namespace rawr {

uint64_t AperturePressureController::now_us() {
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>((counter.QuadPart * 1000000ULL) / freq.QuadPart);
}

AperturePressureController::AperturePressureController() = default;

AperturePressureController::~AperturePressureController() {
    free_double_buffer();
}

float AperturePressureController::calculate_pressure(size_t vram_used_bytes,
                                                     float growth_rate_bytes_per_token,
                                                     size_t cached_expert_bytes) const {
    const float base = static_cast<float>(vram_used_bytes) / VRAM_TOTAL_BYTES;
    const float projected = (static_cast<float>(vram_used_bytes) +
                             growth_rate_bytes_per_token * static_cast<float>(LOOKAHEAD_TOKENS)) /
                            VRAM_TOTAL_BYTES;
    const float cache_relief = static_cast<float>(cached_expert_bytes) / VRAM_TOTAL_BYTES;
    float pressure = std::max(base, projected) - cache_relief;
    if (pressure < 0.0f) pressure = 0.0f;
    return pressure;
}

AperturePressureController::OverflowTier AperturePressureController::detect_tier_fast(
    size_t vram_used_bytes,
    float growth_rate_bytes_per_token,
    size_t cached_expert_bytes) {
    const float p = calculate_pressure(vram_used_bytes, growth_rate_bytes_per_token, cached_expert_bytes);
    adjust_primitives(p);
    return current_tier();
}

void AperturePressureController::adjust_primitives(float pressure) {
    pressure_ = pressure;

    // 64GB redline thresholds for 40B Q8_0 pressure control.
    if (pressure > 0.92f) {
        prefetch_depth_ = 8;
        compression_on_ = true;
    } else if (pressure > 0.82f) {
        prefetch_depth_ = 6;
        compression_on_ = false;
    } else if (pressure > 0.70f) {
        prefetch_depth_ = 4;
        compression_on_ = false;
    } else if (pressure > 0.55f) {
        prefetch_depth_ = 2;
        compression_on_ = false;
    } else {
        prefetch_depth_ = 1;
        compression_on_ = false;
    }
}

bool AperturePressureController::allocate_double_buffer(size_t layer_size) {
    free_double_buffer();

    if (layer_size == 0) {
        return false;
    }

    db_.size = layer_size;
    db_.ready = false;

    // Prefer NUMA-local large pages first.
    db_.active = VirtualAllocExNuma(GetCurrentProcess(), nullptr, layer_size,
                                    MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                                    PAGE_READWRITE, 0);
    db_.shadow = VirtualAllocExNuma(GetCurrentProcess(), nullptr, layer_size,
                                    MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                                    PAGE_READWRITE, 0);

    if (!db_.active || !db_.shadow) {
        if (db_.active) {
            VirtualFree(db_.active, 0, MEM_RELEASE);
            db_.active = nullptr;
        }
        if (db_.shadow) {
            VirtualFree(db_.shadow, 0, MEM_RELEASE);
            db_.shadow = nullptr;
        }

        db_.active = LargePageAllocator::allocate_large_pages(layer_size);
        db_.shadow = LargePageAllocator::allocate_large_pages(layer_size);
    }

    if (!db_.active || !db_.shadow) {
        if (db_.active) {
            VirtualFree(db_.active, 0, MEM_RELEASE);
            db_.active = nullptr;
        }
        if (db_.shadow) {
            VirtualFree(db_.shadow, 0, MEM_RELEASE);
            db_.shadow = nullptr;
        }

        db_.active = VirtualAlloc(nullptr, layer_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        db_.shadow = VirtualAlloc(nullptr, layer_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }

    owns_double_buffer_ = (db_.active != nullptr && db_.shadow != nullptr);
    return owns_double_buffer_;
}

void AperturePressureController::set_external_double_buffer(void* active, void* shadow, size_t layer_size) {
    free_double_buffer();
    db_.active = active;
    db_.shadow = shadow;
    db_.size = layer_size;
    db_.ready = false;
    owns_double_buffer_ = false;
}

void AperturePressureController::stream_to_shadow(const void* src, size_t size) {
    if (!src || !db_.shadow || size == 0 || size > db_.size) {
        return;
    }

    RAWR_Aggressive_Stream(src, db_.shadow, size);
    RAWR_PCIe_FlushBarrier(db_.shadow, size);
    db_.ready = true;
}

void* AperturePressureController::commit_shadow() {
    if (!db_.ready || !db_.active || !db_.shadow) {
        return db_.active;
    }

    void* old_active = RAWR_DoubleBuffer_Swap(reinterpret_cast<void**>(&db_.active), db_.shadow);
    db_.shadow = old_active;
    db_.ready = false;
    return db_.active;
}

void AperturePressureController::free_double_buffer() {
    if (!owns_double_buffer_) {
        db_ = {};
        return;
    }

    if (db_.active) {
        VirtualFree(db_.active, 0, MEM_RELEASE);
    }
    if (db_.shadow) {
        VirtualFree(db_.shadow, 0, MEM_RELEASE);
    }
    db_ = {};
    owns_double_buffer_ = false;
}

bool AperturePressureController::cache_expert(uint64_t hash, void* ptr, size_t size, bool pin_memory) {
    if (!hash || !ptr || size == 0) {
        return false;
    }

    const uint64_t now = now_us();

    for (auto& e : expert_cache_) {
        if (e.valid && e.hash == hash) {
            e.ptr = ptr;
            e.size = size;
            e.last_access_us = now;
            if (pin_memory && e.pin_count == 0 && RawrPinMemory(ptr, size)) {
                e.pin_count = 1;
            }
            return true;
        }
    }

    size_t victim = static_cast<size_t>(-1);
    uint64_t oldest = UINT64_MAX;
    for (size_t i = 0; i < std::size(expert_cache_); ++i) {
        if (!expert_cache_[i].valid) {
            victim = i;
            break;
        }
        if (expert_cache_[i].last_access_us < oldest && expert_cache_[i].pin_count == 0) {
            oldest = expert_cache_[i].last_access_us;
            victim = i;
        }
    }

    if (victim == static_cast<size_t>(-1)) {
        return false;
    }

    if (expert_cache_[victim].valid && expert_cache_[victim].pin_count > 0) {
        RawrUnpinMemory(expert_cache_[victim].ptr, expert_cache_[victim].size);
    }

    expert_cache_[victim] = {hash, ptr, size, now, 0, true};
    if (pin_memory && RawrPinMemory(ptr, size)) {
        expert_cache_[victim].pin_count = 1;
    }

    expert_last_access_[hash & 7ULL] = now;
    return true;
}

bool AperturePressureController::probe_expert(uint64_t hash, void** out_ptr) {
    if (!hash) {
        return false;
    }

    const uint64_t now = now_us();
    const uint64_t slot = hash & 7ULL;
    const int warm = RAWR_ExpertCache_Probe(slot, expert_last_access_, now, 200000ULL);

    for (auto& e : expert_cache_) {
        if (e.valid && e.hash == hash) {
            e.last_access_us = now;
            if (out_ptr) *out_ptr = e.ptr;
            expert_last_access_[slot] = now;
            return warm != 0;
        }
    }

    if (out_ptr) *out_ptr = nullptr;
    return false;
}

bool AperturePressureController::evict_expert(uint64_t hash) {
    for (auto& e : expert_cache_) {
        if (e.valid && e.hash == hash) {
            if (e.pin_count > 1) {
                return false;
            }
            if (e.pin_count == 1) {
                RawrUnpinMemory(e.ptr, e.size);
            }
            e = {};
            return true;
        }
    }
    return false;
}

uint32_t AperturePressureController::evict_cold_experts(uint64_t older_than_us) {
    struct Item {
        size_t idx;
        uint64_t age;
    };

    Item items[16] = {};
    size_t n = 0;
    const uint64_t now = now_us();

    for (size_t i = 0; i < std::size(expert_cache_); ++i) {
        const auto& e = expert_cache_[i];
        if (e.valid && e.pin_count == 0) {
            items[n++] = {i, now - e.last_access_us};
        }
    }

    for (size_t i = 0; i < n; ++i) {
        size_t m = i;
        for (size_t j = i + 1; j < n; ++j) {
            if (items[j].age > items[m].age) {
                m = j;
            }
        }
        if (m != i) {
            std::swap(items[m], items[i]);
        }
    }

    uint32_t evicted = 0;
    for (size_t i = 0; i < n; ++i) {
        if (items[i].age >= older_than_us) {
            expert_cache_[items[i].idx] = {};
            ++evicted;
        }
    }

    return evicted;
}

void AperturePressureController::prefetch_swarm_slot(uint32_t slot_id, void* expert_ptr, size_t expert_size) {
    if (!expert_ptr || expert_size == 0) {
        return;
    }
    RAWR_SwarmSlot_Prefetch(expert_ptr, expert_size, slot_id);
}

void AperturePressureController::tune_prefetch(float observed_tps) {
    if (observed_tps >= 110.0f) prefetch_depth_ = 8;
    else if (observed_tps >= 100.0f) prefetch_depth_ = 6;
    else if (observed_tps >= 85.0f) prefetch_depth_ = 4;
    else if (observed_tps >= 65.0f) prefetch_depth_ = 2;
    else prefetch_depth_ = 1;
}

uint32_t AperturePressureController::get_prefetch_depth() const {
    return prefetch_depth_;
}

bool AperturePressureController::compression_enabled() const {
    return compression_on_;
}

float AperturePressureController::pressure() const {
    return pressure_;
}

AperturePressureController::OverflowTier AperturePressureController::current_tier() const {
    if (pressure_ > 0.92f) return TIER_EMERGENCY;
    if (pressure_ > 0.82f) return TIER_STRIDE;
    if (pressure_ > 0.70f) return TIER_HYBRID;
    return TIER_STEADY;
}

bool UnifiedMemoryAperture::initialize() {
    LargePageAllocator::enable_privilege();

    if (!compute_pool_.initialize(4)) {
        std::cerr << "[UnifiedMemory] Warning: compute pool pre-alloc failed" << std::endl;
    }

    overflow_controller_.initialize(static_cast<size_t>(64) * 1024ULL * 1024ULL * 1024ULL);
    return true;
}

bool UnifiedMemoryAperture::open_model(const char* path) {
    GPUDirectAperture::ApertureConfig config;
    config.type = GPUDirectAperture::APERTURE_CPU_UNCACHED;
    config.chunk_size = 512ULL * 1024ULL * 1024ULL;
    config.use_large_pages = true;
    config.prefetch_sequential = true;
    return file_aperture_.open(path, config);
}

const uint8_t* UnifiedMemoryAperture::model_data() const {
    return static_cast<const uint8_t*>(file_aperture_.base());
}

size_t UnifiedMemoryAperture::model_size() const {
    return file_aperture_.size();
}

void* UnifiedMemoryAperture::acquire_compute_buffer(size_t size) {
    return compute_pool_.acquire(size);
}

void UnifiedMemoryAperture::release_compute_buffer(void* ptr) {
    compute_pool_.release(ptr);
}

bool UnifiedMemoryAperture::activate_expert_bypass(void* expert_weights, size_t size) {
    const uint32_t flags = AggressiveOverflowController::FLAG_PREFETCH |
                           AggressiveOverflowController::FLAG_READ_ONLY |
                           AggressiveOverflowController::FLAG_AGGRESSIVE;
    return overflow_controller_.activate_bypass(expert_weights, size, flags);
}

bool UnifiedMemoryAperture::deactivate_expert_bypass(void* expert_weights, size_t size) {
    return overflow_controller_.deactivate_bypass(expert_weights, size);
}

void UnifiedMemoryAperture::prefetch_upcoming(void** tensors, size_t count, size_t tensor_size) {
    overflow_controller_.lookahead_prefetch(tensors, count, tensor_size);
}

void* UnifiedMemoryAperture::allocate(size_t bytes, bool prefer_large_pages) {
    if (bytes == 0) return nullptr;

    if (!aperture_base_) {
        aperture_size_ = std::max<size_t>(bytes * 4, 1024ULL * 1024ULL * 1024ULL);
        aperture_size_ = std::min<size_t>(aperture_size_, 16ULL * 1024ULL * 1024ULL * 1024ULL);

        if (prefer_large_pages) {
            aperture_base_ = static_cast<uint8_t*>(LargePageAllocator::allocate_large_pages(aperture_size_));
        }
        if (!aperture_base_) {
            aperture_base_ = static_cast<uint8_t*>(VirtualAlloc(nullptr, aperture_size_, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        }
        aperture_used_ = 0;
    }

    const size_t aligned = (bytes + 63ULL) & ~63ULL;
    if (!aperture_base_ || (aperture_used_ + aligned > aperture_size_)) {
        pressure_ctrl_.evict_cold_experts(250000);
        return nullptr;
    }

    void* p = aperture_base_ + aperture_used_;
    aperture_used_ += aligned;
    return p;
}

void UnifiedMemoryAperture::deallocate(void* ptr, size_t bytes) {
    if (!aperture_base_ || !ptr || bytes == 0) return;
    uint8_t* end = aperture_base_ + aperture_used_;
    uint8_t* expected = static_cast<uint8_t*>(ptr) + ((bytes + 63ULL) & ~63ULL);
    if (expected == end) {
        aperture_used_ -= ((bytes + 63ULL) & ~63ULL);
    }
}

bool UnifiedMemoryAperture::stream_expert(uint64_t expert_hash, const void* src, size_t bytes, void** out_ptr) {
    if (!src || !out_ptr || bytes == 0) {
        return false;
    }

    void* cached = nullptr;
    if (pressure_ctrl_.probe_expert(expert_hash, &cached) && cached) {
        *out_ptr = cached;
        return true;
    }

    void* dst = allocate(bytes, true);
    if (!dst) {
        return false;
    }

    RAWR_Aggressive_Stream(src, dst, bytes);
    pressure_ctrl_.cache_expert(expert_hash, dst, bytes, true);
    *out_ptr = dst;
    return true;
}

bool UnifiedMemoryAperture::begin_layer_swap(size_t layer_bytes) {
    return pressure_ctrl_.allocate_double_buffer(layer_bytes);
}

void UnifiedMemoryAperture::stream_layer_to_shadow(const void* src, size_t bytes) {
    pressure_ctrl_.stream_to_shadow(src, bytes);
}

void* UnifiedMemoryAperture::commit_layer_swap() {
    return pressure_ctrl_.commit_shadow();
}

AperturePressureController::OverflowTier UnifiedMemoryAperture::pressure() const {
    return pressure_ctrl_.current_tier();
}

void UnifiedMemoryAperture::update_pressure(size_t vram_used_bytes, float growth_rate_bytes_per_token) {
    const size_t cached_bytes = 0;
    const auto tier = pressure_ctrl_.detect_tier_fast(vram_used_bytes, growth_rate_bytes_per_token, cached_bytes);
    if (tier == AperturePressureController::TIER_EMERGENCY) {
        pressure_ctrl_.evict_cold_experts(100000);
    }
}

float UnifiedMemoryAperture::predict_pressure(size_t vram_used_bytes,
                                              float growth_rate_bytes_per_token,
                                              size_t cached_expert_bytes) const {
    return pressure_ctrl_.calculate_pressure(vram_used_bytes, growth_rate_bytes_per_token, cached_expert_bytes);
}

void UnifiedMemoryAperture::prefetch_for_agent(uint32_t slot_id, void* expert_ptr, size_t expert_size) {
    pressure_ctrl_.prefetch_swarm_slot(slot_id, expert_ptr, expert_size);
}

uint32_t UnifiedMemoryAperture::proactive_evict_swarm(void** tensor_ptrs,
                                                      uint64_t* last_access,
                                                      uint32_t* access_count,
                                                      size_t count,
                                                      uint64_t threshold_us) {
    LARGE_INTEGER freq;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    const uint64_t now = static_cast<uint64_t>((counter.QuadPart * 1000000ULL) / freq.QuadPart);

    return RawrProactiveEvictSwarm(tensor_ptrs, last_access, access_count,
                                   count, now, threshold_us, 2);
}

uint8_t UnifiedMemoryAperture::expert_dedup_mask(void** expert_ptrs,
                                                 uint64_t* expert_hashes,
                                                 uint64_t* aperture_hashes,
                                                 size_t num_experts,
                                                 size_t aperture_count) {
    return RawrExpertDedupMask(expert_ptrs, expert_hashes, aperture_hashes, num_experts, aperture_count);
}

void UnifiedMemoryAperture::swarm_assign_slots(uint8_t* agent_expert_ids,
                                               size_t num_agents,
                                               uint8_t* slot_assignments,
                                               uint8_t* slot_expert_cache,
                                               size_t num_slots) {
    RawrSwarmAssignSlots(agent_expert_ids, num_agents, slot_assignments, slot_expert_cache, num_slots);
}

void UnifiedMemoryAperture::bandwidth_aware_stream(void* src,
                                                   void* dst,
                                                   size_t size,
                                                   uint64_t ddr5_bw,
                                                   uint64_t pcie_bw) {
    RawrBandwidthAwareStream(src, dst, size, ddr5_bw, pcie_bw);
}

AggressiveOverflowController& UnifiedMemoryAperture::overflow() {
    return overflow_controller_;
}

AperturePressureController& UnifiedMemoryAperture::pressure_controller() {
    return pressure_ctrl_;
}

void UnifiedMemoryAperture::close() {
    file_aperture_.close();
    pressure_ctrl_.free_double_buffer();
    if (aperture_base_) {
        VirtualFree(aperture_base_, 0, MEM_RELEASE);
        aperture_base_ = nullptr;
        aperture_size_ = 0;
        aperture_used_ = 0;
    }
}

} // namespace rawr
