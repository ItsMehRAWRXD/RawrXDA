// kv_paging.cpp - Implementation of fine-grained KV cache paging
// Part of the Copilot-like inference pipeline.

#include "kv_paging.h"
#include <algorithm>
#include <fstream>

namespace RawrXD {

KVPaging::KVPaging(size_t vram_budget_mb, size_t ram_budget_mb, size_t disk_budget_mb)
    : vram_budget_(vram_budget_mb * 1024 * 1024)
    , ram_budget_(ram_budget_mb * 1024 * 1024)
    , disk_budget_(disk_budget_mb * 1024 * 1024)
    , vram_used_(0)
    , ram_used_(0)
    , disk_used_(0)
    , next_page_id_(1)
{
    stats_ = {};
}

KVPaging::~KVPaging() {
    Clear();
}

std::vector<PageId> KVPaging::AllocatePages(int num_tokens) {
    int num_pages = (num_tokens + KV_PAGE_SIZE - 1) / KV_PAGE_SIZE;
    std::vector<PageId> pages;
    pages.reserve(num_pages);
    
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    for (int i = 0; i < num_pages; i++) {
        PageId page_id = next_page_id_++;
        
        auto page = std::make_unique<KVPage>();
        page->id = page_id;
        page->state = PageState::FREE;
        page->start_token = i * KV_PAGE_SIZE;
        page->end_token = std::min((i + 1) * KV_PAGE_SIZE, num_tokens);
        page->memory_size = KV_PAGE_SIZE * sizeof(float) * 2;  // K + V
        page->access_count = 0;
        page->last_access = std::chrono::steady_clock::now();
        page->created = std::chrono::steady_clock::now();
        page->is_dirty = false;
        
        pages_[page_id] = std::move(page);
        pages.push_back(page_id);
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.total_pages++;
            stats_.free_pages++;
        }
    }
    
    return pages;
}

void KVPaging::FreePages(const std::vector<PageId>& pages) {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    for (PageId page_id : pages) {
        auto it = pages_.find(page_id);
        if (it == pages_.end()) {
            continue;
        }
        
        KVPage& page = *it->second;
        
        // Free resources based on state
        if (page.state == PageState::HOT) {
            FreeVRAM(page_id);
        }
        
        // Remove from page table
        page_table_.erase(page_id);
        
        // Remove from pages
        pages_.erase(it);
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.total_pages--;
            
            switch (page.state) {
                case PageState::HOT:
                    stats_.hot_pages--;
                    break;
                case PageState::WARM:
                    stats_.warm_pages--;
                    break;
                case PageState::COLD:
                    stats_.cold_pages--;
                    break;
                case PageState::FREE:
                    stats_.free_pages--;
                    break;
                default:
                    break;
            }
        }
    }
}

KVPage* KVPaging::AccessPage(PageId page_id) {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return nullptr;
    }
    
    KVPage& page = *it->second;
    
    // Check if page is resident
    if (page.state != PageState::HOT && page.state != PageState::WARM) {
        // Page fault
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.page_faults++;
        }
        
        auto fault_start = std::chrono::steady_clock::now();
        
        // Handle page fault
        if (!HandlePageFault(page_id)) {
            return nullptr;
        }
        
        auto fault_end = std::chrono::steady_clock::now();
        auto fault_latency = std::chrono::duration_cast<std::chrono::microseconds>(
            fault_end - fault_start);
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.avg_page_fault_latency = (stats_.avg_page_fault_latency * (stats_.page_faults - 1) + 
                                            fault_latency) / stats_.page_faults;
        }
    } else {
        // Page hit
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.page_hits++;
            
            if (stats_.page_hits + stats_.page_faults > 0) {
                stats_.hit_rate = static_cast<float>(stats_.page_hits) / 
                                 (stats_.page_hits + stats_.page_faults);
            }
        }
    }
    
    // Update access info
    page.access_count++;
    page.last_access = std::chrono::steady_clock::now();
    
    return &page;
}

const KVPage* KVPaging::GetPage(PageId page_id) const {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return nullptr;
    }
    
    return it->second.get();
}

void KVPaging::WritePage(PageId page_id, const std::vector<float>& data) {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return;
    }
    
    KVPage& page = *it->second;
    page.kv_data = data;
    page.is_dirty = true;
    page.last_access = std::chrono::steady_clock::now();
}

bool KVPaging::PromoteToHot(PageId page_id) {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return false;
    }
    
    KVPage& page = *it->second;
    
    if (page.state == PageState::HOT) {
        return true;  // Already hot
    }
    
    // Check VRAM budget
    if (vram_used_ + page.memory_size > vram_budget_) {
        // Evict to free space
        EvictLRU(page.memory_size);
    }
    
    // Allocate VRAM
    if (!AllocateVRAM(page_id)) {
        return false;
    }
    
    // Update state
    PageState old_state = page.state;
    page.state = PageState::HOT;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.hot_pages++;
        stats_.promotions++;
        
        switch (old_state) {
            case PageState::WARM:
                stats_.warm_pages--;
                break;
            case PageState::COLD:
                stats_.cold_pages--;
                break;
            case PageState::FREE:
                stats_.free_pages--;
                break;
            default:
                break;
        }
    }
    
    return true;
}

bool KVPaging::DemoteToWarm(PageId page_id) {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return false;
    }
    
    KVPage& page = *it->second;
    
    if (page.state == PageState::WARM) {
        return true;  // Already warm
    }
    
    // Free VRAM if hot
    if (page.state == PageState::HOT) {
        FreeVRAM(page_id);
    }
    
    // Update state
    PageState old_state = page.state;
    page.state = PageState::WARM;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.warm_pages++;
        stats_.demotions++;
        
        switch (old_state) {
            case PageState::HOT:
                stats_.hot_pages--;
                break;
            case PageState::COLD:
                stats_.cold_pages--;
                break;
            default:
                break;
        }
    }
    
    return true;
}

bool KVPaging::DemoteToCold(PageId page_id) {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return false;
    }
    
    KVPage& page = *it->second;
    
    if (page.state == PageState::COLD) {
        return true;  // Already cold
    }
    
    // Save to disk if dirty
    if (page.is_dirty) {
        SaveToDisk(page_id);
    }
    
    // Free VRAM if hot
    if (page.state == PageState::HOT) {
        FreeVRAM(page_id);
    }
    
    // Update state
    PageState old_state = page.state;
    page.state = PageState::COLD;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.cold_pages++;
        stats_.demotions++;
        
        switch (old_state) {
            case PageState::HOT:
                stats_.hot_pages--;
                break;
            case PageState::WARM:
                stats_.warm_pages--;
                break;
            default:
                break;
        }
    }
    
    return true;
}

void KVPaging::PrefetchPages(const std::vector<PageId>& pages) {
    for (PageId page_id : pages) {
        PromoteToHot(page_id);
    }
}

void KVPaging::EvictLRU(size_t bytes_to_free) {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    // Find least recently used hot pages
    std::vector<std::pair<PageId, std::chrono::steady_clock::time_point>> hot_pages;
    
    for (auto& pair : pages_) {
        if (pair.second->state == PageState::HOT) {
            hot_pages.push_back({pair.first, pair.second->last_access});
        }
    }
    
    // Sort by last access time
    std::sort(hot_pages.begin(), hot_pages.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });
    
    // Evict until enough space is freed
    size_t freed = 0;
    for (const auto& [page_id, _] : hot_pages) {
        if (freed >= bytes_to_free) {
            break;
        }
        
        auto it = pages_.find(page_id);
        if (it == pages_.end()) {
            continue;
        }
        
        KVPage& page = *it->second;
        
        // Save to disk if dirty
        if (page.is_dirty) {
            SaveToDisk(page_id);
        }
        
        // Free VRAM
        FreeVRAM(page_id);
        
        // Demote to warm
        page.state = PageState::WARM;
        freed += page.memory_size;
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.hot_pages--;
            stats_.warm_pages++;
            stats_.evictions++;
        }
    }
}

void KVPaging::Clear() {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    pages_.clear();
    page_table_.clear();
    free_pages_.clear();
    
    vram_used_ = 0;
    ram_used_ = 0;
    disk_used_ = 0;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_ = {};
    }
}

PageId KVPaging::FindFreePage() {
    std::lock_guard<std::mutex> lock(pages_mutex_);
    
    for (auto& pair : pages_) {
        if (pair.second->state == PageState::FREE) {
            return pair.first;
        }
    }
    
    return 0;  // No free page
}

KVPage* KVPaging::FindPage(PageId page_id) {
    auto it = pages_.find(page_id);
    return it != pages_.end() ? it->second.get() : nullptr;
}

const KVPage* KVPaging::FindPage(PageId page_id) const {
    auto it = pages_.find(page_id);
    return it != pages_.end() ? it->second.get() : nullptr;
}

bool KVPaging::HandlePageFault(PageId page_id) {
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return false;
    }
    
    KVPage& page = *it->second;
    
    if (page.state == PageState::COLD) {
        // Load from disk
        if (!LoadFromDisk(page_id)) {
            return false;
        }
    }
    
    // Promote to hot
    return PromoteToHot(page_id);
}

bool KVPaging::LoadFromDisk(PageId page_id) {
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return false;
    }
    
    const std::string& path = it->second.disk_path;
    
    // TODO: Load from disk
    // std::ifstream file(path, std::ios::binary);
    // if (!file.is_open()) {
    //     return false;
    // }
    // 
    // Read KV data from file
    
    return true;
}

bool KVPaging::SaveToDisk(PageId page_id) {
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return false;
    }
    
    KVPage& page = *it->second;
    
    // Generate disk path
    std::string path = "kv_cache/page_" + std::to_string(page_id) + ".bin";
    
    // TODO: Save to disk
    // std::ofstream file(path, std::ios::binary);
    // if (!file.is_open()) {
    //     return false;
    // }
    // 
    // Write KV data to file
    
    // Update page table
    page_table_[page_id].disk_path = path;
    page.is_dirty = false;
    
    return true;
}

bool KVPaging::AllocateVRAM(PageId page_id) {
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return false;
    }
    
    KVPage& page = *it->second;
    
    // TODO: Allocate GPU memory
    // void* gpu_ptr = vulkan_->AllocateBuffer(page.memory_size);
    // if (!gpu_ptr) {
    //     return false;
    // }
    // 
    // page.vram_offset = reinterpret_cast<size_t>(gpu_ptr);
    // vram_used_ += page.memory_size;
    
    // Update page table
    page_table_[page_id].vram_ptr = reinterpret_cast<void*>(page.vram_offset);
    
    return true;
}

void KVPaging::FreeVRAM(PageId page_id) {
    auto it = pages_.find(page_id);
    if (it == pages_.end()) {
        return;
    }
    
    KVPage& page = *it->second;
    
    // TODO: Free GPU memory
    // vulkan_->FreeBuffer(reinterpret_cast<void*>(page.vram_offset));
    // vram_used_ -= page.memory_size;
    
    page.vram_offset = 0;
    
    // Update page table
    page_table_[page_id].vram_ptr = nullptr;
}

} // namespace RawrXD