// kv_paging.h - Fine-grained KV cache paging for optimal locality
// Implements paged KV cache with GPU-resident hot segments and LRU eviction
//
// Architecture:
//   - Pages: Fixed-size blocks (e.g., 128 tokens per page)
//   - Hot pages: GPU-resident (frequently accessed)
//   - Warm pages: System RAM (recently accessed)
//   - Cold pages: Disk (rarely accessed)
//
// Key insight:
//   - Not: Store entire KV cache in one buffer
//   - But: Break into pages, keep hot pages in GPU, evict cold pages
//
// This affects long sessions more than benchmarks.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Page size (tokens per page)
constexpr int KV_PAGE_SIZE = 128;

// Page ID
using PageId = uint64_t;

// Page state
enum class PageState : uint8_t {
    FREE = 0,       // Not allocated
    HOT = 1,        // GPU-resident
    WARM = 2,       // System RAM
    COLD = 3,       // Disk
    EVICTING = 4,   // Being evicted
};

// KV page
struct KVPage {
    PageId id;                      // Unique page ID
    PageState state;                // Current state
    int start_token;                // Start token index
    int end_token;                  // End token index
    std::vector<float> kv_data;     // KV cache data
    size_t memory_size;               // Bytes used
    size_t vram_offset;               // Offset in GPU memory (if HOT)
    int access_count;                 // Number of accesses
    std::chrono::steady_clock::time_point last_access;
    std::chrono::steady_clock::time_point created;
    bool is_dirty;                    // Modified since last write
};

// Page table entry
struct PageTableEntry {
    PageId page_id;
    PageState state;
    void* vram_ptr;                 // GPU memory pointer (if HOT)
    void* ram_ptr;                  // System RAM pointer (if WARM)
    std::string disk_path;          // Disk path (if COLD)
};

// Paging statistics
struct PagingStats {
    int total_pages;
    int hot_pages;
    int warm_pages;
    int cold_pages;
    int free_pages;
    size_t vram_used;
    size_t ram_used;
    size_t disk_used;
    int page_faults;                // Access to non-resident page
    int page_hits;                  // Access to resident page
    int evictions;
    int promotions;                 // COLD → WARM → HOT
    int demotions;                  // HOT → WARM → COLD
    float hit_rate;
    std::chrono::microseconds avg_page_fault_latency;
};

// KV paging manager
class KVPaging {
public:
    KVPaging(
        size_t vram_budget_mb = 512,
        size_t ram_budget_mb = 2048,
        size_t disk_budget_mb = 8192
    );
    ~KVPaging();
    
    // Allocate pages for sequence
    std::vector<PageId> AllocatePages(int num_tokens);
    
    // Free pages
    void FreePages(const std::vector<PageId>& pages);
    
    // Access page (may trigger page fault)
    KVPage* AccessPage(PageId page_id);
    
    // Get page (read-only, may trigger page fault)
    const KVPage* GetPage(PageId page_id) const;
    
    // Write to page (marks as dirty)
    void WritePage(PageId page_id, const std::vector<float>& data);
    
    // Promote page to HOT (GPU-resident)
    bool PromoteToHot(PageId page_id);
    
    // Demote page to WARM (System RAM)
    bool DemoteToWarm(PageId page_id);
    
    // Demote page to COLD (Disk)
    bool DemoteToCold(PageId page_id);
    
    // Prefetch pages into HOT
    void PrefetchPages(const std::vector<PageId>& pages);
    
    // Evict least recently used pages
    void EvictLRU(size_t bytes_to_free);
    
    // Get page state
    PageState GetPageState(PageId page_id) const;
    
    // Check if page is resident
    bool IsResident(PageId page_id) const;
    
    // Get statistics
    PagingStats GetStats() const;
    
    // Set budgets
    void SetVRAMBudget(size_t budget_mb);
    void SetRAMBudget(size_t budget_mb);
    void SetDiskBudget(size_t budget_mb);
    
    // Clear all pages
    void Clear();
    
private:
    // Find free page
    PageId FindFreePage();
    
    // Find page by ID
    KVPage* FindPage(PageId page_id);
    const KVPage* FindPage(PageId page_id) const;
    
    // Handle page fault (load from lower tier)
    bool HandlePageFault(PageId page_id);
    
    // Load page from disk
    bool LoadFromDisk(PageId page_id);
    
    // Save page to disk
    bool SaveToDisk(PageId page_id);
    
    // Allocate GPU memory for page
    bool AllocateVRAM(PageId page_id);
    
    // Free GPU memory for page
    void FreeVRAM(PageId page_id);
    
    // Members
    size_t vram_budget_;
    size_t ram_budget_;
    size_t disk_budget_;
    size_t vram_used_;
    size_t ram_used_;
    size_t disk_used_;
    
    mutable std::mutex pages_mutex_;
    std::unordered_map<PageId, std::unique_ptr<KVPage>> pages_;
    
    // Free page pool
    std::vector<PageId> free_pages_;
    
    // Page table
    std::unordered_map<PageId, PageTableEntry> page_table_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    PagingStats stats_;
    
    // Next page ID
    PageId next_page_id_;
};

// Inline implementations

inline PageState KVPaging::GetPageState(PageId page_id) const {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return PageState::FREE;
    }
    return it->second->state;
}

inline bool KVPaging::IsResident(PageId page_id) const {
    PageState state = GetPageState(page_id);
    return state == PageState::HOT || state == PageState::WARM;
}

inline PagingStats KVPaging::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline void KVPaging::SetVRAMBudget(size_t budget_mb) {
    vram_budget_ = budget_mb * 1024 * 1024;
}

inline void KVPaging::SetRAMBudget(size_t budget_mb) {
    ram_budget_ = budget_mb * 1024 * 1024;
}

inline void KVPaging::SetDiskBudget(size_t budget_mb) {
    disk_budget_ = budget_mb * 1024 * 1024;
}

} // namespace RawrXD