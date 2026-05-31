// ============================================================================
// sovereign_pager.h — Win32-native tiered pager (RAM + overlapped disk I/O)
// ============================================================================
// v1: Hot/Warm RAM pools, intrusive LRU, IOCP for speculative prefetches.
// AcquireExpert uses synchronous ReadFile into committed pages (deterministic).
//
// Instances are large (~fixed pools); allocate with std::make_unique / new, not
// on the default thread stack.
//
// GPU: `ExpertWeights()` returns CPU RAM; bind to Vulkan by copying into a
// host-visible `VkBuffer` or staging to `DEVICE_LOCAL` (see flash_attention_vulkan_fp8.h).
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace sov
{

static constexpr uint32_t kPageMiB = 64;
static constexpr uint32_t kPageBytes = kPageMiB * 1024u * 1024u;
static constexpr uint32_t kMaxExperts = 512;
static constexpr uint32_t kMaxLayers = 128;
static constexpr uint32_t kMaxPages = 4096;
static constexpr uint32_t kPrefetchAhead = 4;

// Numeric order: hotter / closer to compute == larger uint8_t (used in AcquireExpert).
enum class Tier : uint8_t
{
    Null = 0,
    ColdDisk = 1,
    MappedDisk = 2,
    WarmRAM = 3,
    HotRAM = 4,
    GpuVRAM = 5
};

static constexpr size_t kTierSlotCount = 6;

struct Page
{
    void* virt = nullptr;
    HANDLE hMap = nullptr;
    uint64_t fileOffMiB = 0;
    Tier tier = Tier::Null;
    uint32_t seq = 0;
    uint32_t refs = 0;
    bool io_pending = false;
    bool pinned = false;
    Page* lru_prev = nullptr;
    Page* lru_next = nullptr;
};

struct Expert
{
    uint32_t id = 0;
    uint32_t layer = 0;
    uint32_t page0 = 0;
    uint32_t page_count = 0;
    Tier resident = Tier::Null;
    uint64_t last_tick = 0;
    float route_prob = 0.f;
    bool pinned = false;
};

struct OverlappedEx
{
    OVERLAPPED ol{};
    uint32_t page_idx = 0;
    uint32_t expert_key = 0;
    bool is_prefetch = false;
};

class SovereignPager
{
  public:
    bool Init(uint64_t hot_budget_mb, uint64_t warm_budget_mb, const wchar_t* disk_path, uint32_t numa_node = 0);
    bool RegisterModel(uint32_t layers, uint32_t experts_per_layer, const uint64_t* expert_disk_offsets_mb,
                       const uint32_t* expert_page_counts);
    bool AcquireExpert(uint32_t layer, uint32_t expert, Tier min_tier);
    void ReleaseExpert(uint32_t layer, uint32_t expert);
    void* ExpertWeights(uint32_t layer, uint32_t expert, size_t* out_bytes);
    void PrefetchLayer(uint32_t next_layer, const float* router_probs, uint32_t topk);
    void BackgroundEvict();
    void Shutdown();

    uint64_t bytes_resident(Tier t) const;
    uint64_t io_issued() const { return io_issued_.load(std::memory_order_relaxed); }
    uint64_t io_completed() const { return io_completed_.load(std::memory_order_relaxed); }

  private:
    bool try_enable_large_pages();
    bool evict_one_page();
    void lru_remove(Page* p);
    void lru_insert_front(Page* p);
    void lru_move_front(Page* p);
    bool page_index_busy(uint32_t idx) const;
    void set_page_busy(uint32_t idx, bool busy);
    bool find_contiguous_free(uint32_t need, uint32_t* out_start) const;
    bool mark_contiguous_busy(uint32_t start, uint32_t need, bool busy);
    bool read_page_sync(Page& p, uint64_t file_offset_bytes);

    HANDLE iocp_ = nullptr;
    HANDLE h_disk_ = INVALID_HANDLE_VALUE;

    uint64_t hot_budget_pages_ = 0;
    uint64_t warm_budget_pages_ = 0;
    uint32_t numa_node_ = 0;

    Page pages_[kMaxPages]{};
    uint8_t page_busy_[(kMaxPages + 7) / 8]{};

    Expert experts_[kMaxLayers][kMaxExperts]{};
    uint32_t num_layers_ = 0;
    uint32_t experts_per_layer_ = 0;

    Page* lru_head_[kTierSlotCount]{};
    Page* lru_tail_[kTierSlotCount]{};

    uint64_t expert_off_mb_[kMaxLayers][kMaxExperts]{};
    uint32_t expert_pages_[kMaxLayers][kMaxExperts]{};

    uint64_t resident_bytes_[kTierSlotCount]{};
    std::atomic<uint64_t> io_issued_{0};
    std::atomic<uint64_t> io_completed_{0};
    uint64_t global_tick_ = 0;
    std::atomic<bool> shutdown_{false};
};

}  // namespace sov
