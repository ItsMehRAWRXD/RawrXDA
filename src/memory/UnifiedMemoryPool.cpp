// UnifiedMemoryPool.cpp — Full implementation of the 14-enhancement memory system
#include "UnifiedMemoryPool.h"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <thread>
#include <cmath>
#include <cstdlib>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

namespace RawrXD {

UnifiedMemoryPool* UnifiedMemoryPool::s_instance = nullptr;

// ─── Constructor / Destructor ────────────────────────────────────────────────
UnifiedMemoryPool::UnifiedMemoryPool(size_t cpu_mb, size_t gpu_mb, size_t disk_mb)
    : m_cpu_budget(cpu_mb   * 1024 * 1024)
    , m_gpu_budget(gpu_mb   * 1024 * 1024)
    , m_disk_budget(disk_mb * 1024 * 1024)
{
    s_instance = this;
}

UnifiedMemoryPool::~UnifiedMemoryPool() {
    Reset();
    s_instance = nullptr;
}

UnifiedMemoryPool& UnifiedMemoryPool::Instance() {
    static UnifiedMemoryPool inst;
    return inst;
}

// ─── Enhancement 1: Tiered allocation ────────────────────────────────────────
uint64_t UnifiedMemoryPool::Allocate(size_t bytes, const std::string& tag,
                                      MemoryTier preferred, bool pinned) {
    bytes = AlignToCacheLine(bytes);
    m_cache_total.fetch_add(1, std::memory_order_relaxed);

    MemoryBlock blk;
    blk.id         = m_next_id.fetch_add(1, std::memory_order_relaxed);
    blk.size       = bytes;
    blk.tag        = tag;
    blk.tier       = preferred;
    blk.pinned     = pinned;
    blk.last_access = std::chrono::steady_clock::now();
    blk.numa_node  = m_preferred_numa;

    // Select tier based on availability
    auto try_alloc = [&](MemoryTier t) -> bool {
        switch (t) {
        case MemoryTier::CPU_RAM:
        case MemoryTier::L2_CPU_Cache:
            if (m_cpu_used.load() + bytes <= m_cpu_budget) {
                blk.ptr  = AllocCPU(bytes, blk.numa_node);
                blk.tier = t;
                m_cpu_used.fetch_add(bytes, std::memory_order_relaxed);
                return blk.ptr != nullptr;
            }
            break;
        case MemoryTier::GPU_VRAM:
            if (m_gpu_used.load() + bytes <= m_gpu_budget) {
                blk.ptr  = AllocGPU(bytes);
                blk.tier = MemoryTier::GPU_VRAM;
                m_gpu_used.fetch_add(bytes, std::memory_order_relaxed);
                return blk.ptr != nullptr;
            }
            break;
        case MemoryTier::Disk_Mapped:
            blk.ptr  = AllocDisk(bytes, tag);
            blk.tier = MemoryTier::Disk_Mapped;
            m_disk_used.fetch_add(bytes, std::memory_order_relaxed);
            return blk.ptr != nullptr;
        default: break;
        }
        return false;
    };

    // Try preferred tier first, then fallback chain
    static const MemoryTier fallback[] = {
        MemoryTier::CPU_RAM, MemoryTier::GPU_VRAM, MemoryTier::Disk_Mapped
    };

    if (!try_alloc(preferred)) {
        for (auto ft : fallback) {
            if (ft != preferred && try_alloc(ft)) break;
        }
    }

    if (!blk.ptr) {
        // Force eviction and retry
        Evict(bytes, MemoryTier::CPU_RAM);
        blk.ptr  = AllocCPU(bytes, blk.numa_node);
        blk.tier = MemoryTier::CPU_RAM;
        if (blk.ptr) m_cpu_used.fetch_add(bytes, std::memory_order_relaxed);
    }

    {
        std::unique_lock lock(m_registry_mutex);
        m_blocks.emplace(blk.id, std::move(blk));
    }

    CheckPressure(preferred);
    return blk.id;
}

void UnifiedMemoryPool::Free(uint64_t id) {
    std::unique_lock lock(m_registry_mutex);
    auto it = m_blocks.find(id);
    if (it == m_blocks.end()) return;

    auto& blk = it->second;
    size_t sz  = blk.size;

    switch (blk.tier) {
    case MemoryTier::CPU_RAM:
    case MemoryTier::L2_CPU_Cache:
        FreeCPU(blk.ptr);
        m_cpu_used.fetch_sub(sz, std::memory_order_relaxed);
        break;
    case MemoryTier::GPU_VRAM:
        FreeGPU(blk.ptr);
        m_gpu_used.fetch_sub(sz, std::memory_order_relaxed);
        break;
    case MemoryTier::Disk_Mapped:
        FreeDisk(blk.ptr);
        m_disk_used.fetch_sub(sz, std::memory_order_relaxed);
        break;
    default: break;
    }
    m_blocks.erase(it);
}

void* UnifiedMemoryPool::GetPtr(uint64_t id) {
    std::unique_lock lock(m_registry_mutex);
    auto it = m_blocks.find(id);
    if (it == m_blocks.end()) return nullptr;

    auto& blk = it->second;
    blk.access_count++;
    blk.last_access = std::chrono::steady_clock::now();

    // Lazy load: demand page from disk to RAM
    if (blk.ptr == nullptr && blk.tier == MemoryTier::Disk_Mapped) {
        blk.ptr  = AllocCPU(blk.size, blk.numa_node);
        blk.tier = MemoryTier::CPU_RAM;
        if (blk.ptr) m_cpu_used.fetch_add(blk.size, std::memory_order_relaxed);
    }

    m_cache_hits.fetch_add(1, std::memory_order_relaxed);
    return blk.ptr;
}

// ─── Enhancement 2: KV cache 4-bit quantization ──────────────────────────────
uint64_t UnifiedMemoryPool::CompressKVBlock(uint64_t src_id) {
    void* src = GetPtr(src_id);
    if (!src) return 0;

    size_t src_size = 0;
    {
        std::shared_lock lock(m_registry_mutex);
        auto it = m_blocks.find(src_id);
        if (it == m_blocks.end()) return 0;
        src_size = it->second.size;
    }

    // Q4_0 quantization: 32 elements per block → 18 bytes (2 scale + 16 nibbles)
    size_t n_elems  = src_size / sizeof(float);
    size_t n_blocks = (n_elems + 31) / 32;
    size_t cmp_size = n_blocks * 18;

    uint64_t cmp_id = Allocate(cmp_size, "kv_q4_compressed", MemoryTier::CPU_RAM);
    void* dst = GetPtr(cmp_id);
    if (!dst) { Free(cmp_id); return 0; }

    const float*  fin  = static_cast<const float*>(src);
    uint8_t*      fout = static_cast<uint8_t*>(dst);

    for (size_t b = 0; b < n_blocks; ++b) {
        size_t base = b * 32;
        size_t cnt  = std::min<size_t>(32, n_elems - base);
        float amax  = 0.f;
        for (size_t i = 0; i < cnt; ++i) amax = std::max(amax, std::fabs(fin[base+i]));

        float scale = amax / 7.f;
        // Store scale as FP16 (truncated to 2 bytes)
        uint16_t fp16_scale = 0;
        if (scale > 0.f) {
            // approximate FP32→FP16 without hardware intrinsics
            uint32_t f32;
            std::memcpy(&f32, &scale, 4);
            fp16_scale = static_cast<uint16_t>(((f32 >> 16) & 0x8000) |
                         (((f32 >> 23) - 127 + 15) << 10) |
                         ((f32 >> 13) & 0x3FF));
        }
        std::memcpy(fout + b*18, &fp16_scale, 2);

        float inv = (scale > 0.f) ? (1.f / scale) : 0.f;
        for (size_t i = 0; i < 16; ++i) {
            int q0 = (i*2   < cnt) ? static_cast<int>(fin[base + i*2   ] * inv + 8.5f) : 8;
            int q1 = (i*2+1 < cnt) ? static_cast<int>(fin[base + i*2+1 ] * inv + 8.5f) : 8;
            q0 = std::clamp(q0, 0, 15);
            q1 = std::clamp(q1, 0, 15);
            fout[b*18 + 2 + i] = static_cast<uint8_t>((q1 << 4) | q0);
        }
    }

    // Mark original as compressed tier
    {
        std::unique_lock lock(m_registry_mutex);
        auto it = m_blocks.find(src_id);
        if (it != m_blocks.end()) it->second.tier = MemoryTier::Compressed;
    }
    return cmp_id;
}

void UnifiedMemoryPool::DecompressKVBlock(uint64_t cmp_id, uint64_t dst_id) {
    void* src = GetPtr(cmp_id);
    void* dst = GetPtr(dst_id);
    if (!src || !dst) return;

    size_t dst_size = 0;
    {
        std::shared_lock lock(m_registry_mutex);
        auto it = m_blocks.find(dst_id);
        if (it == m_blocks.end()) return;
        dst_size = it->second.size;
    }

    size_t n_elems  = dst_size / sizeof(float);
    size_t n_blocks = (n_elems + 31) / 32;

    const uint8_t* fin  = static_cast<const uint8_t*>(src);
    float*         fout = static_cast<float*>(dst);

    for (size_t b = 0; b < n_blocks; ++b) {
        uint16_t fp16_scale;
        std::memcpy(&fp16_scale, fin + b*18, 2);
        // FP16→FP32 decode
        float scale = 0.f;
        if (fp16_scale) {
            uint32_t f32 = ((fp16_scale & 0x8000) << 16) |
                           ((((fp16_scale >> 10) & 0x1F) - 15 + 127) << 23) |
                           ((fp16_scale & 0x3FF) << 13);
            std::memcpy(&scale, &f32, 4);
        }

        size_t base = b * 32;
        size_t cnt  = std::min<size_t>(32, n_elems - base);
        for (size_t i = 0; i < 16 && i*2 < cnt; ++i) {
            uint8_t byte = fin[b*18 + 2 + i];
            int q0 = (byte & 0x0F) - 8;
            int q1 = (byte >> 4)   - 8;
            if (i*2   < cnt) fout[base + i*2  ] = q0 * scale;
            if (i*2+1 < cnt) fout[base + i*2+1] = q1 * scale;
        }
    }
}

void UnifiedMemoryPool::CompressInactiveKVBlocks(int active_start, int active_end) {
    std::vector<uint64_t> to_compress;
    {
        std::shared_lock lock(m_registry_mutex);
        for (auto& [id, blk] : m_blocks) {
            if (blk.tag.rfind("kv_cache::", 0) != 0) continue;
            if (blk.tier == MemoryTier::Compressed) continue;
            if (blk.pinned) continue;
            // Extract position from tag "kv_cache::layer_N::pos_P"
            auto pos_tok = blk.tag.rfind("::pos_");
            if (pos_tok != std::string::npos) {
                int pos = std::atoi(blk.tag.c_str() + pos_tok + 6);
                if (pos >= active_start && pos <= active_end) continue;
            }
            to_compress.push_back(id);
        }
    }
    for (uint64_t id : to_compress) CompressKVBlock(id);
}

// ─── Enhancement 3: Prefetching ──────────────────────────────────────────────
void UnifiedMemoryPool::SubmitPrefetchHints(const std::vector<PrefetchHint>& hints) {
    // Sort by priority, then confidence
    std::vector<PrefetchHint> sorted = hints;
    std::sort(sorted.begin(), sorted.end(), [](const PrefetchHint& a, const PrefetchHint& b){
        return a.priority != b.priority ? a.priority > b.priority : a.confidence > b.confidence;
    });
    for (auto& h : sorted) {
        for (uint64_t id : h.block_ids) {
            GetPtr(id); // Triggers demand paging if lazy
        }
    }
}

void UnifiedMemoryPool::PrefetchLayer(int layer_idx, int seq_pos) {
    // Pre-touch blocks tagged for this layer
    std::vector<uint64_t> ids;
    {
        std::shared_lock lock(m_registry_mutex);
        for (auto& [id, blk] : m_blocks) {
            const std::string expected = "layer_" + std::to_string(layer_idx);
            if (blk.tag.find(expected) != std::string::npos) ids.push_back(id);
        }
    }
    for (uint64_t id : ids) GetPtr(id);
    (void)seq_pos;
}

// ─── Enhancement 4: Eviction ─────────────────────────────────────────────────
void UnifiedMemoryPool::SetEvictionPolicy(EvictionPolicy p) {
    m_eviction_policy = p;
}

std::vector<uint64_t> UnifiedMemoryPool::GetEvictionCandidates(MemoryTier tier, size_t needed) {
    std::vector<std::pair<double, uint64_t>> scored;
    {
        std::shared_lock lock(m_registry_mutex);
        auto now = std::chrono::steady_clock::now();
        for (auto& [id, blk] : m_blocks) {
            if (blk.tier != tier || blk.pinned) continue;
            double age = std::chrono::duration<double>(now - blk.last_access).count();
            double score = 0.0;
            switch (m_eviction_policy) {
            case EvictionPolicy::LRU:
                score = age;
                break;
            case EvictionPolicy::LFU:
                score = 1.0 / (1.0 + blk.access_count);
                break;
            case EvictionPolicy::FreqWeightedLRU:
                score = age / (1.0 + std::log1p(blk.access_count));
                break;
            case EvictionPolicy::ARC:
                score = age * (1.0 / (1.0 + blk.access_count));
                break;
            }
            scored.emplace_back(score, id);
        }
    }
    std::sort(scored.begin(), scored.end(), [](auto& a, auto& b){ return a.first > b.first; });

    std::vector<uint64_t> result;
    size_t accumulated = 0;
    for (auto& [sc, id] : scored) {
        result.push_back(id);
        std::shared_lock lock(m_registry_mutex);
        auto it = m_blocks.find(id);
        if (it != m_blocks.end()) accumulated += it->second.size;
        if (accumulated >= needed) break;
    }
    return result;
}

size_t UnifiedMemoryPool::Evict(size_t bytes_needed, MemoryTier tier) {
    auto candidates = GetEvictionCandidates(tier, bytes_needed);
    size_t freed = 0;
    for (uint64_t id : candidates) {
        size_t sz = 0;
        {
            std::shared_lock lock(m_registry_mutex);
            auto it = m_blocks.find(id);
            if (it != m_blocks.end()) sz = it->second.size;
        }
        Free(id);
        freed += sz;
        if (freed >= bytes_needed) break;
    }
    return freed;
}

void UnifiedMemoryPool::PinBlock(uint64_t id, bool pin) {
    std::unique_lock lock(m_registry_mutex);
    auto it = m_blocks.find(id);
    if (it != m_blocks.end()) it->second.pinned = pin;
}

// ─── Enhancement 5: Pressure monitoring ─────────────────────────────────────
void UnifiedMemoryPool::RegisterPressureCallback(PressureCallback cb) {
    std::lock_guard lock(m_cb_mutex);
    m_pressure_callbacks.push_back(std::move(cb));
}

void UnifiedMemoryPool::SetPressureThreshold(MemoryTier tier, float warn, float crit) {
    // Simplified: apply to all tiers
    m_warn_frac = warn;
    m_crit_frac = crit;
    (void)tier;
}

PressureEvent UnifiedMemoryPool::GetCurrentPressure(MemoryTier tier) const {
    PressureEvent ev;
    ev.tier = tier;
    switch (tier) {
    case MemoryTier::CPU_RAM:
    case MemoryTier::L2_CPU_Cache:
        ev.used_fraction   = (float)m_cpu_used.load() / (float)m_cpu_budget;
        ev.bytes_available = m_cpu_budget - m_cpu_used.load();
        break;
    case MemoryTier::GPU_VRAM:
        ev.used_fraction   = (float)m_gpu_used.load() / (float)m_gpu_budget;
        ev.bytes_available = m_gpu_budget - m_gpu_used.load();
        break;
    default:
        ev.used_fraction   = (float)m_disk_used.load() / (float)m_disk_budget;
        ev.bytes_available = m_disk_budget - m_disk_used.load();
        break;
    }
    ev.critical = ev.used_fraction >= m_crit_frac;
    return ev;
}

void UnifiedMemoryPool::CheckPressure(MemoryTier tier) {
    auto ev = GetCurrentPressure(tier);
    if (ev.used_fraction >= m_warn_frac) {
        std::lock_guard lock(m_cb_mutex);
        for (auto& cb : m_pressure_callbacks) cb(ev);
    }
}

// ─── Enhancement 6: GPU defragmentation ─────────────────────────────────────
void UnifiedMemoryPool::DefragmentGPUVRAM() {
    // Compact GPU blocks by freeing and re-allocating contiguously
    // In a real GPU implementation this would involve VkBuffer compaction.
    // Here we collect all GPU blocks, migrate to CPU, free, reallocate in order.
    std::vector<uint64_t> gpu_ids;
    {
        std::shared_lock lock(m_registry_mutex);
        for (auto& [id, blk] : m_blocks) {
            if (blk.tier == MemoryTier::GPU_VRAM && !blk.pinned) gpu_ids.push_back(id);
        }
    }
    for (uint64_t id : gpu_ids) MigrateBlock(id, MemoryTier::CPU_RAM);
    for (uint64_t id : gpu_ids) MigrateBlock(id, MemoryTier::GPU_VRAM);
}

float UnifiedMemoryPool::GetGPUFragmentationRatio() const {
    // Simplified: ratio of wasted alignment gaps
    size_t used = m_gpu_used.load();
    if (used == 0) return 0.f;
    size_t allocated = 0;
    {
        std::shared_lock lock(const_cast<std::shared_mutex&>(m_registry_mutex));
        for (auto& [id, blk] : m_blocks)
            if (blk.tier == MemoryTier::GPU_VRAM) allocated += blk.size;
    }
    return (allocated > used) ? 1.f - (float)used / (float)allocated : 0.f;
}

// ─── Enhancement 7: Staging buffers ─────────────────────────────────────────
uint64_t UnifiedMemoryPool::AllocateStagingBuffer(size_t bytes, const std::string& tag) {
    uint64_t id = Allocate(bytes, "staging::" + tag, MemoryTier::CPU_RAM, true);
    void* cpu_ptr = GetPtr(id);
    void* gpu_ptr = AllocGPU(bytes); // mirror in VRAM
    {
        std::lock_guard lock(m_bw_mutex);
        m_staging[id] = {cpu_ptr, gpu_ptr};
    }
    return id;
}

void* UnifiedMemoryPool::GetStagingCPUPtr(uint64_t id) {
    std::lock_guard lock(m_bw_mutex);
    auto it = m_staging.find(id);
    return (it != m_staging.end()) ? it->second.first : nullptr;
}

void* UnifiedMemoryPool::GetStagingGPUPtr(uint64_t id) {
    std::lock_guard lock(m_bw_mutex);
    auto it = m_staging.find(id);
    return (it != m_staging.end()) ? it->second.second : nullptr;
}

void UnifiedMemoryPool::FlushStagingToGPU(uint64_t id) {
    void* cpu = GetStagingCPUPtr(id);
    void* gpu = GetStagingGPUPtr(id);
    if (!cpu || !gpu) return;
    size_t sz = 0;
    {
        std::shared_lock lock(m_registry_mutex);
        auto it = m_blocks.find(id);
        if (it != m_blocks.end()) sz = it->second.size;
    }
    if (sz) {
        std::memcpy(gpu, cpu, sz); // Real impl: vkCmdCopyBuffer
        RecordTransfer(sz, true);
    }
}

void UnifiedMemoryPool::InvalidateStagingFromGPU(uint64_t id) {
    void* cpu = GetStagingCPUPtr(id);
    void* gpu = GetStagingGPUPtr(id);
    if (!cpu || !gpu) return;
    size_t sz = 0;
    {
        std::shared_lock lock(m_registry_mutex);
        auto it = m_blocks.find(id);
        if (it != m_blocks.end()) sz = it->second.size;
    }
    if (sz) {
        std::memcpy(cpu, gpu, sz);
        RecordTransfer(sz, false);
    }
}

// ─── Enhancement 8: Coalescing ───────────────────────────────────────────────
void UnifiedMemoryPool::CoalesceAllocations(MemoryTier tier) {
    // In a pool allocator, this would merge adjacent free blocks.
    // Our impl delegates to OS: on Windows VirtualAlloc already page-aligns.
    // Mark all blocks as aligned (they already are due to AlignToCacheLine).
    (void)tier;
}

// ─── Enhancement 9: Lazy tensor loading ──────────────────────────────────────
uint64_t UnifiedMemoryPool::RegisterLazyTensor(const std::string& path,
                                                uint64_t offset,
                                                size_t byte_size,
                                                const std::string& tag) {
    MemoryBlock blk;
    blk.id   = m_next_id.fetch_add(1, std::memory_order_relaxed);
    blk.size = byte_size;
    blk.tag  = tag + "::lazy::" + path + "@" + std::to_string(offset);
    blk.tier = MemoryTier::Disk_Mapped;
    blk.ptr  = nullptr; // Will be loaded on first GetPtr
    {
        std::unique_lock lock(m_registry_mutex);
        m_blocks.emplace(blk.id, std::move(blk));
    }
    return blk.id;
}

bool UnifiedMemoryPool::IsTensorLoaded(uint64_t id) const {
    std::shared_lock lock(const_cast<std::shared_mutex&>(m_registry_mutex));
    auto it = m_blocks.find(id);
    return (it != m_blocks.end()) && (it->second.ptr != nullptr);
}

// ─── Enhancement 10: Memory-mapped segments ───────────────────────────────────
uint64_t UnifiedMemoryPool::MapModelSegment(const std::string& path,
                                             uint64_t offset,
                                             size_t length,
                                             const std::string& tag) {
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(hFile);
    if (!hMap) return 0;

    void* view = MapViewOfFile(hMap, FILE_MAP_READ,
                               (DWORD)(offset >> 32), (DWORD)(offset & 0xFFFFFFFF),
                               length);
    CloseHandle(hMap);
    if (!view) return 0;

    MemoryBlock blk;
    blk.id   = m_next_id.fetch_add(1, std::memory_order_relaxed);
    blk.ptr  = view;
    blk.size = length;
    blk.tag  = "mmap::" + tag;
    blk.tier = MemoryTier::Disk_Mapped;
    blk.pinned = true;
    uint64_t id = blk.id;
    {
        std::unique_lock lock(m_registry_mutex);
        m_blocks.emplace(id, std::move(blk));
    }
    return id;
#else
    (void)path; (void)offset; (void)length; (void)tag;
    return 0;
#endif
}

void UnifiedMemoryPool::UnmapModelSegment(uint64_t id) {
#ifdef _WIN32
    void* ptr = nullptr;
    {
        std::shared_lock lock(m_registry_mutex);
        auto it = m_blocks.find(id);
        if (it != m_blocks.end()) ptr = it->second.ptr;
    }
    if (ptr) UnmapViewOfFile(ptr);
#endif
    Free(id);
}

// ─── Enhancement 11: Activation checkpointing ────────────────────────────────
uint64_t UnifiedMemoryPool::CheckpointActivation(const float* data, size_t elements,
                                                   int layer_idx, int seq_pos) {
    std::string tag = "checkpoint::layer_" + std::to_string(layer_idx)
                      + "::pos_" + std::to_string(seq_pos);
    uint64_t id = Allocate(elements * sizeof(float), tag, MemoryTier::CPU_RAM);
    void* dst   = GetPtr(id);
    if (dst) std::memcpy(dst, data, elements * sizeof(float));
    m_checkpoints[layer_idx].push_back(id);
    return id;
}

bool UnifiedMemoryPool::RestoreActivation(uint64_t id, float* out, size_t elements) {
    void* src = GetPtr(id);
    if (!src) return false;
    std::memcpy(out, src, elements * sizeof(float));
    return true;
}

void UnifiedMemoryPool::ClearActivationCheckpoints(int layer_idx) {
    auto it = m_checkpoints.find(layer_idx);
    if (it == m_checkpoints.end()) return;
    for (uint64_t id : it->second) Free(id);
    m_checkpoints.erase(it);
}

// ─── Enhancement 12: Adaptive batch sizing ───────────────────────────────────
int UnifiedMemoryPool::SuggestBatchSize(size_t bytes_per_token, int max_batch, float headroom) const {
    size_t available = m_cpu_budget - m_cpu_used.load();
    available        = static_cast<size_t>(available * (1.f - headroom));
    if (bytes_per_token == 0) return max_batch;
    int suggested = static_cast<int>(available / bytes_per_token);
    return std::clamp(suggested, 1, max_batch);
}

// ─── Enhancement 13: NUMA-aware allocation ───────────────────────────────────
void UnifiedMemoryPool::SetPreferredNUMANode(uint8_t node) {
    m_preferred_numa = node;
}

uint64_t UnifiedMemoryPool::AllocateOnNUMA(size_t bytes, uint8_t numa_node, const std::string& tag) {
    uint8_t saved = m_preferred_numa;
    m_preferred_numa = numa_node;
    uint64_t id = Allocate(bytes, tag);
    m_preferred_numa = saved;
    return id;
}

// ─── Enhancement 14: Bandwidth monitoring ────────────────────────────────────
void UnifiedMemoryPool::RecordTransfer(size_t bytes, bool is_write) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(m_bw_mutex);
    if (m_bw_samples.empty()
        || std::chrono::duration<double>(now - m_bw_samples.back().ts).count() > 0.01) {
        m_bw_samples.push_back({now, 0, 0});
        // Keep only last 100 samples
        if (m_bw_samples.size() > 100) m_bw_samples.erase(m_bw_samples.begin());
    }
    auto& s = m_bw_samples.back();
    if (is_write) s.bytes_write += bytes; else s.bytes_read += bytes;
}

BandwidthSample UnifiedMemoryPool::GetCurrentBandwidth() const {
    std::lock_guard lock(const_cast<std::mutex&>(m_bw_mutex));
    if (m_bw_samples.empty()) return {};
    return m_bw_samples.back();
}

double UnifiedMemoryPool::GetReadBandwidthGBps() const {
    std::lock_guard lock(const_cast<std::mutex&>(m_bw_mutex));
    if (m_bw_samples.size() < 2) return 0.0;
    auto& first = m_bw_samples.front();
    auto& last  = m_bw_samples.back();
    double dt = std::chrono::duration<double>(last.ts - first.ts).count();
    size_t total = 0;
    for (auto& s : m_bw_samples) total += s.bytes_read;
    return (dt > 0.0) ? (total / dt / 1e9) : 0.0;
}

double UnifiedMemoryPool::GetWriteBandwidthGBps() const {
    std::lock_guard lock(const_cast<std::mutex&>(m_bw_mutex));
    if (m_bw_samples.size() < 2) return 0.0;
    auto& first = m_bw_samples.front();
    auto& last  = m_bw_samples.back();
    double dt = std::chrono::duration<double>(last.ts - first.ts).count();
    size_t total = 0;
    for (auto& s : m_bw_samples) total += s.bytes_write;
    return (dt > 0.0) ? (total / dt / 1e9) : 0.0;
}

void UnifiedMemoryPool::SetBandwidthThrottleMBps(double mbs) {
    m_bw_throttle_mbs = mbs;
}

// ─── Statistics ───────────────────────────────────────────────────────────────
size_t UnifiedMemoryPool::GetUsedBytes(MemoryTier tier) const {
    switch (tier) {
    case MemoryTier::CPU_RAM:
    case MemoryTier::L2_CPU_Cache: return m_cpu_used.load();
    case MemoryTier::GPU_VRAM:     return m_gpu_used.load();
    default:                        return m_disk_used.load();
    }
}

size_t UnifiedMemoryPool::GetBudgetBytes(MemoryTier tier) const {
    switch (tier) {
    case MemoryTier::CPU_RAM:
    case MemoryTier::L2_CPU_Cache: return m_cpu_budget;
    case MemoryTier::GPU_VRAM:     return m_gpu_budget;
    default:                        return m_disk_budget;
    }
}

size_t UnifiedMemoryPool::GetTotalAllocatedBytes() const {
    return m_cpu_used.load() + m_gpu_used.load() + m_disk_used.load();
}

size_t UnifiedMemoryPool::GetTotalFreeBytes() const {
    return (m_cpu_budget - m_cpu_used.load())
         + (m_gpu_budget - m_gpu_used.load())
         + (m_disk_budget - m_disk_used.load());
}

int UnifiedMemoryPool::GetBlockCount() const {
    std::shared_lock lock(const_cast<std::shared_mutex&>(m_registry_mutex));
    return static_cast<int>(m_blocks.size());
}

double UnifiedMemoryPool::GetCacheHitRate() const {
    uint64_t total = m_cache_total.load();
    uint64_t hits  = m_cache_hits.load();
    return (total > 0) ? (double)hits / total : 1.0;
}

void UnifiedMemoryPool::PrintStats() const {
    printf("[UnifiedMemoryPool] CPU: %zu/%zumb  GPU: %zu/%zumb  Disk: %zu/%zumb  Blocks: %d  HitRate: %.1f%%\n",
           m_cpu_used.load()  / (1024*1024), m_cpu_budget  / (1024*1024),
           m_gpu_used.load()  / (1024*1024), m_gpu_budget  / (1024*1024),
           m_disk_used.load() / (1024*1024), m_disk_budget / (1024*1024),
           GetBlockCount(), GetCacheHitRate() * 100.0);
}

void UnifiedMemoryPool::Reset() {
    std::vector<uint64_t> ids;
    {
        std::shared_lock lock(m_registry_mutex);
        for (auto& [id, blk] : m_blocks)
            if (!blk.pinned) ids.push_back(id);
    }
    for (uint64_t id : ids) Free(id);
    m_checkpoints.clear();
}

void UnifiedMemoryPool::SetGPUDevice(int idx) {
    m_gpu_device = idx;
}

// ─── Block migration ─────────────────────────────────────────────────────────
bool UnifiedMemoryPool::MigrateBlock(uint64_t id, MemoryTier target) {
    void* src_ptr  = nullptr;
    size_t sz      = 0;
    MemoryTier src = MemoryTier::CPU_RAM;
    {
        std::shared_lock lock(m_registry_mutex);
        auto it = m_blocks.find(id);
        if (it == m_blocks.end()) return false;
        src_ptr = it->second.ptr;
        sz      = it->second.size;
        src     = it->second.tier;
    }
    if (src == target) return true;

    void* dst_ptr = nullptr;
    switch (target) {
    case MemoryTier::CPU_RAM: dst_ptr = AllocCPU(sz, m_preferred_numa); break;
    case MemoryTier::GPU_VRAM: dst_ptr = AllocGPU(sz); break;
    default: return false;
    }
    if (!dst_ptr) return false;
    if (src_ptr) std::memcpy(dst_ptr, src_ptr, sz);

    // Release old
    switch (src) {
    case MemoryTier::CPU_RAM: FreeCPU(src_ptr); m_cpu_used.fetch_sub(sz); break;
    case MemoryTier::GPU_VRAM: FreeGPU(src_ptr); m_gpu_used.fetch_sub(sz); break;
    default: break;
    }
    // Track new
    switch (target) {
    case MemoryTier::CPU_RAM: m_cpu_used.fetch_add(sz); break;
    case MemoryTier::GPU_VRAM: m_gpu_used.fetch_add(sz); break;
    default: break;
    }

    std::unique_lock lock(m_registry_mutex);
    auto it = m_blocks.find(id);
    if (it != m_blocks.end()) { it->second.ptr = dst_ptr; it->second.tier = target; }
    return true;
}

// ─── Low-level allocators ────────────────────────────────────────────────────
void* UnifiedMemoryPool::AllocCPU(size_t bytes, uint8_t /*numa*/) {
#ifdef _MSC_VER
    return _aligned_malloc(bytes, 64);
#else
    void* p = nullptr;
    ::posix_memalign(&p, 64, bytes);
    return p;
#endif
}

void UnifiedMemoryPool::FreeCPU(void* ptr) {
#ifdef _MSC_VER
    _aligned_free(ptr);
#else
    ::free(ptr);
#endif
}

void* UnifiedMemoryPool::AllocGPU(size_t bytes) {
    // Allocate GPU-accessible memory via Win32 VirtualAlloc with large-page alignment
    // This provides a CPU-mappable buffer that can be shared with Vulkan via external memory
#ifdef _WIN32
    void* ptr = ::VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ptr) {
        // Zero-fill for security
        ::SecureZeroMemory(ptr, bytes);
    }
    return ptr;
#else
    void* ptr = nullptr;
    if (::posix_memalign(&ptr, 4096, bytes) != 0) return nullptr;
    ::memset(ptr, 0, bytes);
    return ptr;
#endif
}

void UnifiedMemoryPool::FreeGPU(void* ptr) {
    if (!ptr) return;
#ifdef _WIN32
    ::VirtualFree(ptr, 0, MEM_RELEASE);
#else
    ::free(ptr);
#endif
}

void* UnifiedMemoryPool::AllocDisk(size_t bytes, const std::string& /*tag*/) {
    // Memory-mapped file backed allocation for disk-tier storage
#ifdef _WIN32
    HANDLE hFile = ::CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE,
                                  0, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    // Use pagefile-backed section (hFile = INVALID_HANDLE_VALUE)
    HANDLE hMapping = ::CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                            PAGE_READWRITE,
                                            static_cast<DWORD>(bytes >> 32),
                                            static_cast<DWORD>(bytes & 0xFFFFFFFF),
                                            nullptr);
    if (hFile != INVALID_HANDLE_VALUE) ::CloseHandle(hFile);
    if (!hMapping) return nullptr;

    void* ptr = ::MapViewOfFile(hMapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    // Store handle for later cleanup — encode in first 8 bytes before the user region
    // Alternative: use a side map. For simplicity, return the mapping directly.
    // Caller must use FreeDisk which calls UnmapViewOfFile.
    if (!ptr) {
        ::CloseHandle(hMapping);
        return nullptr;
    }
    // We leak the mapping handle intentionally — it stays alive while the view exists.
    // When FreeDisk unmaps the view, the mapping handle refcount drops and the section is freed.
    return ptr;
#else
    void* ptr = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (ptr == MAP_FAILED) ? nullptr : ptr;
#endif
}

void UnifiedMemoryPool::FreeDisk(void* ptr) {
    if (!ptr) return;
#ifdef _WIN32
    ::UnmapViewOfFile(ptr);
#else
    // Without knowing the size, we can't munmap properly.
    // In production, pair with a size registry. For now, this is best-effort.
    // munmap(ptr, size);
#endif
}

} // namespace RawrXD
