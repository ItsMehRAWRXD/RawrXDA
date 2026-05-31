// ============================================================================
// sovereign_pager.cpp — Win32 pager (IOCP + VirtualAlloc tiers)
// ============================================================================

#include "sovereign_pager.h"

#include <algorithm>
#include <cstring>

namespace sov
{

namespace
{
bool set_file_pointer(HANDLE h, int64_t off)
{
    LARGE_INTEGER li{};
    li.QuadPart = off;
    return SetFilePointerEx(h, li, nullptr, FILE_BEGIN) != FALSE;
}
}  // namespace

bool SovereignPager::try_enable_large_pages()
{
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
    {
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(nullptr, L"SeLockMemoryPrivilege", &tp.Privileges[0].Luid))
    {
        CloseHandle(tok);
        return false;
    }
    AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    const DWORD gle = GetLastError();
    CloseHandle(tok);
    return gle == ERROR_SUCCESS;
}

bool SovereignPager::Init(uint64_t hot_budget_mb, uint64_t warm_budget_mb, const wchar_t* disk_path, uint32_t numa_node)
{
    (void)try_enable_large_pages();

    numa_node_ = numa_node;
    hot_budget_pages_ = hot_budget_mb / kPageMiB;
    warm_budget_pages_ = warm_budget_mb / kPageMiB;

    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    if (!iocp_)
    {
        return false;
    }

    if (disk_path && disk_path[0] != L'\0')
    {
        h_disk_ =
            CreateFileW(disk_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
    }

    if (h_disk_ == INVALID_HANDLE_VALUE)
    {
        wchar_t tmp[MAX_PATH]{};
        if (GetTempPathW(static_cast<DWORD>(std::size(tmp)), tmp) > 0)
        {
            wcscat_s(tmp, L"sov_pager_staging.bin");
            h_disk_ = CreateFileW(tmp, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
        }
    }

    if (h_disk_ == INVALID_HANDLE_VALUE)
    {
        CloseHandle(iocp_);
        iocp_ = nullptr;
        return false;
    }

    if (!CreateIoCompletionPort(h_disk_, iocp_, 0, 1))
    {
        CloseHandle(h_disk_);
        h_disk_ = INVALID_HANDLE_VALUE;
        CloseHandle(iocp_);
        iocp_ = nullptr;
        return false;
    }

    return true;
}

bool SovereignPager::RegisterModel(uint32_t layers, uint32_t experts_per_layer, const uint64_t* expert_disk_offsets_mb,
                                   const uint32_t* expert_page_counts)
{
    if (!expert_disk_offsets_mb || !expert_page_counts)
    {
        return false;
    }
    if (layers == 0u || experts_per_layer == 0u || layers > kMaxLayers || experts_per_layer > kMaxExperts)
    {
        return false;
    }

    num_layers_ = layers;
    experts_per_layer_ = experts_per_layer;

    for (uint32_t L = 0; L < layers; ++L)
    {
        for (uint32_t E = 0; E < experts_per_layer; ++E)
        {
            const uint32_t idx = L * experts_per_layer + E;
            expert_off_mb_[L][E] = expert_disk_offsets_mb[idx];
            expert_pages_[L][E] = expert_page_counts[idx];

            Expert& ex = experts_[L][E];
            ex.id = E;
            ex.layer = L;
            ex.page0 = 0;
            ex.page_count = expert_page_counts[idx];
            ex.resident = Tier::Null;
            ex.last_tick = 0;
            ex.route_prob = 0.f;
            ex.pinned = false;
        }
    }
    return true;
}

bool SovereignPager::page_index_busy(uint32_t idx) const
{
    const uint32_t byte = idx / 8u;
    const uint32_t bit = idx % 8u;
    return (page_busy_[byte] & static_cast<uint8_t>(1u << bit)) != 0;
}

void SovereignPager::set_page_busy(uint32_t idx, bool busy)
{
    const uint32_t byte = idx / 8u;
    const uint32_t bit = idx % 8u;
    if (busy)
    {
        page_busy_[byte] = static_cast<uint8_t>(page_busy_[byte] | static_cast<uint8_t>(1u << bit));
    }
    else
    {
        page_busy_[byte] = static_cast<uint8_t>(page_busy_[byte] & static_cast<uint8_t>(~(1u << bit)));
    }
}

bool SovereignPager::find_contiguous_free(uint32_t need, uint32_t* out_start) const
{
    if (need == 0u || need > kMaxPages)
    {
        return false;
    }
    for (uint32_t s = 0; s + need <= kMaxPages; ++s)
    {
        bool ok = true;
        for (uint32_t k = 0; k < need; ++k)
        {
            if (page_index_busy(s + k))
            {
                ok = false;
                break;
            }
        }
        if (ok)
        {
            *out_start = s;
            return true;
        }
    }
    return false;
}

bool SovereignPager::mark_contiguous_busy(uint32_t start, uint32_t need, bool busy)
{
    for (uint32_t k = 0; k < need; ++k)
    {
        set_page_busy(start + k, busy);
    }
    return true;
}

bool SovereignPager::read_page_sync(Page& p, uint64_t file_offset_bytes)
{
    if (h_disk_ == INVALID_HANDLE_VALUE || !p.virt)
    {
        return false;
    }
    if (!set_file_pointer(h_disk_, static_cast<int64_t>(file_offset_bytes)))
    {
        return false;
    }
    DWORD got = 0;
    if (!ReadFile(h_disk_, p.virt, kPageBytes, &got, nullptr))
    {
        return false;
    }
    return got == kPageBytes;
}

void SovereignPager::lru_remove(Page* p)
{
    const size_t ti = static_cast<size_t>(p->tier);
    if (ti >= kTierSlotCount)
    {
        return;
    }
    if (p->lru_prev)
    {
        p->lru_prev->lru_next = p->lru_next;
    }
    if (p->lru_next)
    {
        p->lru_next->lru_prev = p->lru_prev;
    }
    if (lru_head_[ti] == p)
    {
        lru_head_[ti] = p->lru_next;
    }
    if (lru_tail_[ti] == p)
    {
        lru_tail_[ti] = p->lru_prev;
    }
    p->lru_prev = p->lru_next = nullptr;
}

void SovereignPager::lru_insert_front(Page* p)
{
    const size_t ti = static_cast<size_t>(p->tier);
    if (ti >= kTierSlotCount)
    {
        return;
    }
    p->lru_next = lru_head_[ti];
    p->lru_prev = nullptr;
    if (lru_head_[ti])
    {
        lru_head_[ti]->lru_prev = p;
    }
    lru_head_[ti] = p;
    if (!lru_tail_[ti])
    {
        lru_tail_[ti] = p;
    }
}

void SovereignPager::lru_move_front(Page* p)
{
    lru_remove(p);
    lru_insert_front(p);
}

bool SovereignPager::evict_one_page()
{
    for (int ti = static_cast<int>(Tier::MappedDisk); ti >= static_cast<int>(Tier::WarmRAM); --ti)
    {
        Page* victim = lru_tail_[static_cast<size_t>(ti)];
        while (victim)
        {
            Page* prev = victim->lru_prev;
            if (victim->refs == 0 && !victim->io_pending && !victim->pinned)
            {
                lru_remove(victim);
                if (victim->virt)
                {
                    VirtualFree(victim->virt, 0, MEM_RELEASE);
                    victim->virt = nullptr;
                }
                if (victim->hMap)
                {
                    CloseHandle(victim->hMap);
                    victim->hMap = nullptr;
                }
                const uint32_t idx = static_cast<uint32_t>(victim - pages_);
                set_page_busy(idx, false);
                resident_bytes_[static_cast<size_t>(ti)] -=
                    resident_bytes_[static_cast<size_t>(ti)] >= kPageBytes ? kPageBytes : 0;
                victim->tier = Tier::ColdDisk;
                return true;
            }
            victim = prev;
        }
    }
    return false;
}

bool SovereignPager::AcquireExpert(uint32_t layer, uint32_t expert, Tier min_tier)
{
    if (layer >= num_layers_ || expert >= experts_per_layer_)
    {
        return false;
    }
    Expert& ex = experts_[layer][expert];
    if (ex.resident != Tier::Null && static_cast<uint8_t>(ex.resident) >= static_cast<uint8_t>(min_tier))
    {
        ex.last_tick = ++global_tick_;
        for (uint32_t i = 0; i < ex.page_count; ++i)
        {
            Page& p = pages_[ex.page0 + i];
            p.seq = static_cast<uint32_t>(global_tick_);
            lru_move_front(&p);
        }
        return true;
    }

    if (ex.resident != Tier::Null && static_cast<uint8_t>(ex.resident) < static_cast<uint8_t>(min_tier))
    {
        for (uint32_t i = 0; i < ex.page_count; ++i)
        {
            Page& p = pages_[ex.page0 + i];
            lru_remove(&p);
            const size_t ti = static_cast<size_t>(p.tier);
            if (ti < kTierSlotCount && resident_bytes_[ti] >= kPageBytes)
            {
                resident_bytes_[ti] -= kPageBytes;
            }
            if (p.virt)
            {
                VirtualFree(p.virt, 0, MEM_RELEASE);
            }
            set_page_busy(ex.page0 + i, false);
        }
        ex.resident = Tier::Null;
    }

    const uint32_t needed = ex.page_count;
    if (needed == 0u)
    {
        return false;
    }

    uint32_t start = 0;
    while (!find_contiguous_free(needed, &start))
    {
        if (!evict_one_page())
        {
            return false;
        }
    }

    mark_contiguous_busy(start, needed, true);
    ex.page0 = start;
    ex.last_tick = ++global_tick_;

    const uint64_t disk_off_mb = expert_off_mb_[layer][expert];

    for (uint32_t i = 0; i < needed; ++i)
    {
        Page& p = pages_[start + i];
        p = Page{};
        p.tier = (min_tier >= Tier::HotRAM) ? Tier::HotRAM : Tier::WarmRAM;
        p.fileOffMiB = disk_off_mb + static_cast<uint64_t>(i) * static_cast<uint64_t>(kPageMiB);
        p.refs = 1;
        p.seq = static_cast<uint32_t>(global_tick_);
        p.io_pending = false;
        p.pinned = ex.pinned;

        const DWORD allocType = MEM_RESERVE | MEM_COMMIT;
        const DWORD prot = PAGE_READWRITE;
        if (p.tier == Tier::HotRAM)
        {
            p.virt = VirtualAllocExNuma(GetCurrentProcess(), nullptr, kPageBytes, allocType | MEM_LARGE_PAGES, prot,
                                        static_cast<DWORD>(numa_node_));
            if (!p.virt)
            {
                p.virt = VirtualAllocExNuma(GetCurrentProcess(), nullptr, kPageBytes, allocType, prot,
                                            static_cast<DWORD>(numa_node_));
            }
        }
        else
        {
            p.virt = VirtualAllocExNuma(GetCurrentProcess(), nullptr, kPageBytes, allocType, prot,
                                        static_cast<DWORD>(numa_node_));
        }
        if (!p.virt)
        {
            mark_contiguous_busy(start, needed, false);
            ex.resident = Tier::Null;
            return false;
        }

        const uint64_t file_off_bytes = p.fileOffMiB * 1024ull * 1024ull;
        if (!read_page_sync(p, file_off_bytes))
        {
            VirtualFree(p.virt, 0, MEM_RELEASE);
            p.virt = nullptr;
            mark_contiguous_busy(start, needed, false);
            ex.resident = Tier::Null;
            return false;
        }

        lru_insert_front(&p);
        resident_bytes_[static_cast<size_t>(p.tier)] += kPageBytes;
        io_issued_.fetch_add(1, std::memory_order_relaxed);
        io_completed_.fetch_add(1, std::memory_order_relaxed);
    }

    ex.resident = min_tier;
    return true;
}

void SovereignPager::ReleaseExpert(uint32_t layer, uint32_t expert)
{
    if (layer >= num_layers_ || expert >= experts_per_layer_)
    {
        return;
    }
    Expert& ex = experts_[layer][expert];
    for (uint32_t i = 0; i < ex.page_count; ++i)
    {
        Page& p = pages_[ex.page0 + i];
        if (p.refs > 0)
        {
            --p.refs;
        }
    }
    bool any = false;
    for (uint32_t i = 0; i < ex.page_count; ++i)
    {
        if (pages_[ex.page0 + i].refs != 0u)
        {
            any = true;
            break;
        }
    }
    if (!any)
    {
        ex.resident = Tier::Null;
    }
}

void* SovereignPager::ExpertWeights(uint32_t layer, uint32_t expert, size_t* out_bytes)
{
    if (layer >= num_layers_ || expert >= experts_per_layer_)
    {
        return nullptr;
    }
    Expert& ex = experts_[layer][expert];
    if (ex.resident == Tier::Null)
    {
        if (!AcquireExpert(layer, expert, Tier::WarmRAM))
        {
            return nullptr;
        }
    }
    if (out_bytes)
    {
        *out_bytes = static_cast<size_t>(ex.page_count) * static_cast<size_t>(kPageBytes);
    }
    return pages_[ex.page0].virt;
}

void SovereignPager::PrefetchLayer(uint32_t next_layer, const float* router_probs, uint32_t topk)
{
    if (next_layer >= num_layers_ || topk == 0u)
    {
        return;
    }

    struct Score
    {
        uint32_t id = 0;
        float prob = 0.f;
    };
    Score scores[kMaxExperts]{};
    const uint32_t epl = experts_per_layer_;
    for (uint32_t e = 0; e < epl; ++e)
    {
        scores[e].id = e;
        scores[e].prob = router_probs ? router_probs[e] : 1.0f;
    }
    for (uint32_t i = 0; i < topk && i < epl; ++i)
    {
        uint32_t best = i;
        for (uint32_t j = i + 1; j < epl; ++j)
        {
            if (scores[j].prob > scores[best].prob)
            {
                best = j;
            }
        }
        const Score tmp = scores[i];
        scores[i] = scores[best];
        scores[best] = tmp;
    }

    const uint32_t k = std::min<uint32_t>(std::min<uint32_t>(topk, kPrefetchAhead), epl);
    for (uint32_t i = 0; i < k; ++i)
    {
        const uint32_t eid = scores[i].id;
        Expert& ex = experts_[next_layer][eid];
        if (ex.resident != Tier::Null)
        {
            continue;
        }
        (void)AcquireExpert(next_layer, eid, Tier::WarmRAM);
    }
}

void SovereignPager::BackgroundEvict()
{
    const uint64_t warm_used =
        resident_bytes_[static_cast<size_t>(Tier::WarmRAM)] + resident_bytes_[static_cast<size_t>(Tier::HotRAM)];
    const uint64_t warm_limit = (warm_budget_pages_ + hot_budget_pages_) * kPageBytes;
    uint64_t cur = warm_used;
    while (cur > warm_limit && evict_one_page())
    {
        cur = resident_bytes_[static_cast<size_t>(Tier::WarmRAM)] + resident_bytes_[static_cast<size_t>(Tier::HotRAM)];
    }
}

uint64_t SovereignPager::bytes_resident(Tier t) const
{
    const size_t ti = static_cast<size_t>(t);
    return ti < kTierSlotCount ? resident_bytes_[ti] : 0ull;
}

void SovereignPager::Shutdown()
{
    shutdown_.store(true, std::memory_order_release);

    for (uint32_t i = 0; i < kMaxPages; ++i)
    {
        if (!page_index_busy(i))
        {
            continue;
        }
        Page& p = pages_[i];
        if (p.virt)
        {
            VirtualFree(p.virt, 0, MEM_RELEASE);
            p.virt = nullptr;
        }
        if (p.hMap)
        {
            CloseHandle(p.hMap);
            p.hMap = nullptr;
        }
        set_page_busy(i, false);
    }

    if (h_disk_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(h_disk_);
        h_disk_ = INVALID_HANDLE_VALUE;
    }
    if (iocp_)
    {
        CloseHandle(iocp_);
        iocp_ = nullptr;
    }

    std::memset(pages_, 0, sizeof(pages_));
    std::memset(page_busy_, 0, sizeof(page_busy_));
    std::memset(lru_head_, 0, sizeof(lru_head_));
    std::memset(lru_tail_, 0, sizeof(lru_tail_));
    std::memset(resident_bytes_, 0, sizeof(resident_bytes_));
    num_layers_ = 0;
    experts_per_layer_ = 0;
}

}  // namespace sov
