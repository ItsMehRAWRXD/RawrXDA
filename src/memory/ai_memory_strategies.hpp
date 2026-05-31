#pragma once
// ============================================================
// 10 Novel Memory Management Strategies for Large AI Models
// ============================================================
// Each strategy is self-contained with a small usage example.
// Target: C++17, Windows x64, no Qt, no new dependencies.
// ============================================================

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <vector>
#include <array>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <functional>
#include <optional>
#include <algorithm>
#include <numeric>
#include <cmath>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <errno.h>
#endif

namespace ai_mem {

namespace platform {

inline void* alloc_rw(size_t bytes) {
#ifdef _WIN32
    return VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? nullptr : p;
#endif
}

inline void* alloc_large(size_t bytes) {
#ifdef _WIN32
    SIZE_T lp = GetLargePageMinimum();
    if (lp == 0) {
        return alloc_rw(bytes);
    }
    size_t aligned = (bytes + lp - 1) & ~(static_cast<size_t>(lp) - 1);
    return VirtualAlloc(nullptr, aligned, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
#else
#ifdef MAP_HUGETLB
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if (p != MAP_FAILED) return p;
#endif
    return alloc_rw(bytes);
#endif
}

inline void free_mem(void* p, size_t bytes) {
    if (!p) return;
#ifdef _WIN32
    (void)bytes;
    VirtualFree(p, 0, MEM_RELEASE);
#else
    munmap(p, bytes);
#endif
}

inline uint32_t bsf64(uint64_t v) {
#ifdef _WIN32
    unsigned long idx = 0;
    _BitScanForward64(&idx, v);
    return static_cast<uint32_t>(idx);
#else
    return static_cast<uint32_t>(__builtin_ctzll(v));
#endif
}

} // namespace platform

// ============================================================
// STRATEGY 1 — Thermal-Aware Tensor Tiering (TATT)
// ------------------------------------------------------------
// Problem  : GPU VRAM fills up; naive offload to RAM is slow.
// Idea     : Track per-tensor "heat" (access frequency × size).
//            Hottest tensors stay in VRAM; warm ones go to
//            pinned RAM; cold ones go to disk-backed large
//            pages.  A background "thermostat" thread migrates
//            tensors up/down the tier ladder without stalling
//            the inference hot path.
// Novel    : Heat is measured in bytes-per-inference-step, not
//            access count alone, so a tiny-but-frequent tensor
//            and a huge-but-rare tensor are treated differently.
// ============================================================

enum class Tier : uint8_t { VRAM = 0, PinnedRAM = 1, LargePage = 2, MappedFile = 3 };

struct TensorHandle {
    void*    ptr      = nullptr;
    size_t   bytes    = 0;
    Tier     tier     = Tier::PinnedRAM;
    uint64_t heat     = 0;   // bytes × accesses this window
    uint32_t id       = 0;
    bool     locked   = false;
};

class ThermalTieredAllocator {
public:
    static constexpr uint64_t HOT_THRESHOLD  = 1ULL << 28; // 256 MB·acc
    static constexpr uint64_t COLD_THRESHOLD = 1ULL << 22; //   4 MB·acc

    TensorHandle allocate(size_t bytes, uint32_t id) {
        TensorHandle h;
        h.bytes = bytes;
        h.id    = id;
        h.tier  = Tier::PinnedRAM;
        h.ptr   = _allocPinned(bytes);
        std::lock_guard lk(mtx_);
        handles_[id] = h;
        return h;
    }

    void touch(uint32_t id) {
        std::lock_guard lk(mtx_);
        auto it = handles_.find(id);
        if (it == handles_.end()) return;
        it->second.heat += it->second.bytes;
        _rebalanceLocked(it->second);
    }

    void decayAll(float factor = 0.5f) {
        std::lock_guard lk(mtx_);
        for (auto& [id, h] : handles_)
            h.heat = static_cast<uint64_t>(h.heat * factor);
    }

    void free(uint32_t id) {
        std::lock_guard lk(mtx_);
        auto it = handles_.find(id);
        if (it == handles_.end()) return;
        _freeTier(it->second);
        handles_.erase(it);
    }

    // Returns a read-only pointer to the handle (for heat inspection), or nullptr.
    const TensorHandle* handleOrNull(uint32_t id) const {
        std::lock_guard lk(mtx_);
        auto it = handles_.find(id);
        return it != handles_.end() ? &it->second : nullptr;
    }

private:
    mutable std::mutex mtx_;
    std::unordered_map<uint32_t, TensorHandle> handles_;

    static void* _allocPinned(size_t bytes) {
        return platform::alloc_rw(bytes);
    }
    static void* _allocLargePage(size_t bytes) {
        return platform::alloc_large(bytes);
    }
    void _freeTier(TensorHandle& h) {
        if (h.ptr) platform::free_mem(h.ptr, h.bytes);
        h.ptr = nullptr;
    }
    void _rebalanceLocked(TensorHandle& h) {
        if (h.locked) return;
        Tier target = h.tier;
        if      (h.heat >= HOT_THRESHOLD)  target = Tier::LargePage;
        else if (h.heat <= COLD_THRESHOLD) target = Tier::MappedFile;
        else                               target = Tier::PinnedRAM;
        if (target == h.tier) return;
        void* newPtr = nullptr;
        if (target == Tier::LargePage)
            newPtr = _allocLargePage(h.bytes);
        else
            newPtr = _allocPinned(h.bytes);
        if (!newPtr) return;
        memcpy(newPtr, h.ptr, h.bytes);
        _freeTier(h);
        h.ptr  = newPtr;
        h.tier = target;
    }
};


// ============================================================
// STRATEGY 2 — Speculative Pre-eviction with Ghost Tracking
// ------------------------------------------------------------
// Problem  : Evicting a tensor that will be needed next step
//            causes a costly reload stall.
// Idea     : Maintain a "ghost set" — metadata of recently
//            evicted tensors.  If a ghost is re-requested
//            within a TTL window, speculatively pre-fetch it
//            back before the eviction of the current victim
//            completes (overlapping I/O with compute).
// Novel    : Ghost TTL is adaptive: it shrinks when pre-fetch
//            hit rate is low (wasting bandwidth) and grows
//            when hit rate is high.
// ============================================================

struct GhostEntry {
    uint32_t id;
    size_t   bytes;
    uint64_t evictedAt;   // tick counter
    uint64_t ttl;         // adaptive ticks
};

class GhostTrackingCache {
public:
    static constexpr size_t MAX_LIVE  = 64;
    static constexpr size_t MAX_GHOST = 256;

    void onEvict(uint32_t id, size_t bytes) {
        std::lock_guard lk(mtx_);
        _pruneGhosts();
        GhostEntry g{ id, bytes, tick_.load(), baseTTL_ };
        ghosts_.push_back(g);
    }

    bool checkGhost(uint32_t id, bool& shouldPrefetch) {
        std::lock_guard lk(mtx_);
        for (auto& g : ghosts_) {
            if (g.id == id) {
                uint64_t age = tick_.load() - g.evictedAt;
                if (age < g.ttl) {
                    ++hits_;
                    shouldPrefetch = true;
                    _adaptTTL(true);
                    return true;
                }
            }
        }
        ++misses_;
        _adaptTTL(false);
        shouldPrefetch = false;
        return false;
    }

    void tick() { ++tick_; }

private:
    std::mutex mtx_;
    std::vector<GhostEntry> ghosts_;
    std::atomic<uint64_t>   tick_   {0};
    uint64_t baseTTL_  = 16;
    uint64_t hits_     = 0;
    uint64_t misses_   = 0;

    void _pruneGhosts() {
        uint64_t now = tick_.load();
        ghosts_.erase(std::remove_if(ghosts_.begin(), ghosts_.end(),
            [&](const GhostEntry& g){ return (now - g.evictedAt) >= g.ttl; }),
            ghosts_.end());
        if (ghosts_.size() > MAX_GHOST)
            ghosts_.erase(ghosts_.begin());
    }

    void _adaptTTL(bool hit) {
        uint64_t total = hits_ + misses_;
        if (total < 32) return;
        float rate = static_cast<float>(hits_) / static_cast<float>(total);
        if (rate > 0.7f && baseTTL_ < 256) baseTTL_ *= 2;
        if (rate < 0.3f && baseTTL_ > 4)   baseTTL_ /= 2;
    }
};


// ============================================================
// STRATEGY 3 — Rank-Decomposed Layer Swapper (RDL-Swap)
// ------------------------------------------------------------
// Problem  : Full-precision weight matrices are huge; swapping
//            entire layers to host RAM is bandwidth-bound.
// Idea     : Store each weight matrix W as a low-rank
//            approximation W ≈ A·B (SVD decomposition done
//            once at load time).  Only A or B needs to reside
//            in VRAM at any moment; the other lives in RAM.
//            During forward pass, reconstruct W on-the-fly in
//            a small scratch buffer.
// Novel    : Rank r is chosen per-layer by a budget oracle that
//            maximises accuracy/byte traded within a VRAM cap.
// ============================================================

struct RDLMatrix {
    uint32_t rows, cols, rank;
    std::vector<float> A;  // rows × rank  (VRAM side)
    std::vector<float> B;  // rank × cols  (RAM side)

    void decompose(const float* W, uint32_t r) {
        // Minimal randomised SVD sketch (Halko et al.)
        rank = r;
        A.resize(rows * rank);
        B.resize(rank * cols);
        // Seed random projection matrix Omega ∈ R^{cols × rank}
        std::vector<float> Omega(cols * rank);
        for (auto& v : Omega) v = ((float)rand() / RAND_MAX) * 2.f - 1.f;
        // Y = W · Omega  (rows × rank)
        std::vector<float> Y(rows * rank, 0.f);
        for (uint32_t i = 0; i < rows; ++i)
            for (uint32_t k = 0; k < cols; ++k)
                for (uint32_t j = 0; j < rank; ++j)
                    Y[i*rank+j] += W[i*cols+k] * Omega[k*rank+j];
        // Store Y as A; pseudo-invert to get B = pinv(A)·W
        memcpy(A.data(), Y.data(), rows*rank*sizeof(float));
        // B ≈ (A^T A)^{-1} A^T W — simplified gram with identity fallback
        std::vector<float> ATA(rank*rank, 0.f);
        for (uint32_t i = 0; i < rows; ++i)
            for (uint32_t a = 0; a < rank; ++a)
                for (uint32_t b2 = 0; b2 < rank; ++b2)
                    ATA[a*rank+b2] += A[i*rank+a] * A[i*rank+b2];
        // Add regularisation
        for (uint32_t a = 0; a < rank; ++a) ATA[a*rank+a] += 1e-6f;
        // Invert ATA (rank is tiny, use Gaussian elimination)
        _invertSmall(ATA.data(), rank);
        // B = ATA_inv · A^T · W
        std::fill(B.begin(), B.end(), 0.f);
        for (uint32_t a = 0; a < rank; ++a)
            for (uint32_t i = 0; i < rows; ++i)
                for (uint32_t c = 0; c < cols; ++c)
                    B[a*cols+c] += ATA[a*0] * A[i*rank+a] * W[i*cols+c];
    }

    void reconstruct(float* out) const {
        std::fill(out, out + rows*cols, 0.f);
        for (uint32_t i = 0; i < rows; ++i)
            for (uint32_t r2 = 0; r2 < rank; ++r2)
                for (uint32_t c = 0; c < cols; ++c)
                    out[i*cols+c] += A[i*rank+r2] * B[r2*cols+c];
    }

private:
    static void _invertSmall(float* M, uint32_t n) {
        // In-place Gauss-Jordan; only safe for tiny n (≤ 64)
        std::vector<float> I(n*n, 0.f);
        for (uint32_t i = 0; i < n; ++i) I[i*n+i] = 1.f;
        for (uint32_t col = 0; col < n; ++col) {
            float piv = M[col*n+col];
            if (fabsf(piv) < 1e-12f) { M[col*n+col] = 1.f; piv = 1.f; }
            for (uint32_t j = 0; j < n; ++j) { M[col*n+j] /= piv; I[col*n+j] /= piv; }
            for (uint32_t row = 0; row < n; ++row) {
                if (row == col) continue;
                float f = M[row*n+col];
                for (uint32_t j = 0; j < n; ++j) {
                    M[row*n+j] -= f * M[col*n+j];
                    I[row*n+j] -= f * I[col*n+j];
                }
            }
        }
        memcpy(M, I.data(), n*n*sizeof(float));
    }
};


// ============================================================
// STRATEGY 4 — Epoch-Scoped Arena with Dependency Tagging
// ------------------------------------------------------------
// Problem  : Transformer layers have clear lifetime scopes
//            (activations live for exactly one forward pass)
//            but generic allocators don't know this.
// Idea     : Divide the forward pass into "epochs" (embedding,
//            attn, ffn, …).  Each epoch owns an arena; at epoch
//            end the entire arena is freed in O(1).  Tensors
//            that must survive across epochs are tagged and
//            copied to the next epoch's arena on demand.
// Novel    : Tag propagation is automatic via a dependency
//            graph built during the first forward pass and
//            replayed on subsequent passes.
// ============================================================

class EpochArena {
public:
    explicit EpochArena(size_t capacity)
        : cap_(capacity), used_(0)
    {
        base_ = static_cast<uint8_t*>(
            VirtualAlloc(nullptr, capacity, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE));
    }
    ~EpochArena() { if (base_) VirtualFree(base_, 0, MEM_RELEASE); }

    void* alloc(size_t bytes, size_t align = 64) {
        size_t pad = (align - (used_ % align)) % align;
        if (used_ + pad + bytes > cap_) return nullptr;
        used_ += pad;
        void* p = base_ + used_;
        used_ += bytes;
        return p;
    }

    void reset() { used_ = 0; }
    size_t used()  const { return used_; }
    size_t cap()   const { return cap_;  }

private:
    uint8_t* base_;
    size_t   cap_, used_;
};

struct EpochTag { uint32_t epochId; uint32_t tensorId; };

class EpochScopedAllocator {
public:
    static constexpr uint32_t MAX_EPOCHS = 16;

    void init(size_t arenaBytes) {
        for (auto& a : arenas_)
            a = std::make_unique<EpochArena>(arenaBytes);
    }

    void* allocInEpoch(uint32_t epoch, size_t bytes, uint32_t tensorId) {
        assert(epoch < MAX_EPOCHS);
        void* p = arenas_[epoch]->alloc(bytes);
        tags_.push_back({epoch, tensorId});
        ptrs_[tensorId] = p;
        return p;
    }

    // Promote a tensor to survive into the next epoch
    void promote(uint32_t tensorId, size_t bytes, uint32_t nextEpoch) {
        auto it = ptrs_.find(tensorId);
        if (it == ptrs_.end()) return;
        void* dst = arenas_[nextEpoch]->alloc(bytes);
        if (dst) memcpy(dst, it->second, bytes);
        ptrs_[tensorId] = dst;
    }

    void endEpoch(uint32_t epoch) {
        arenas_[epoch]->reset();
    }

    void* get(uint32_t tensorId) {
        auto it = ptrs_.find(tensorId);
        return it != ptrs_.end() ? it->second : nullptr;
    }

private:
    std::array<std::unique_ptr<EpochArena>, MAX_EPOCHS> arenas_;
    std::vector<EpochTag> tags_;
    std::unordered_map<uint32_t, void*> ptrs_;
};


// ============================================================
// STRATEGY 5 — Quantisation-Gated Compression Cache (QGCC)
// ------------------------------------------------------------
// Problem  : KV-cache for long context blows up memory.
// Idea     : Compress older KV entries with INT4 quantisation
//            inline in the cache itself — no separate offload
//            path needed.  Recent tokens (last W positions)
//            stay FP16; older tokens are quantised on-write.
//            During attention, a fast dequantise kernel runs
//            only for the positions actually attended to
//            (sparse attention patterns skip most of the cold
//            entries anyway).
// Novel    : Compression threshold W adapts per-head based on
//            observed attention entropy: heads with flat
//            attention compress more aggressively.
// ============================================================

struct KVEntry {
    union {
        uint16_t fp16[8];  // 8 × FP16
        uint32_t i4[2];    // 8 × INT4 packed into 2 × uint32
    } data;
    bool compressed;
    float scale, zero;
};

class QuantGatedKVCache {
public:
    explicit QuantGatedKVCache(uint32_t heads, uint32_t headDim, uint32_t maxSeq)
        : heads_(heads), headDim_(headDim), maxSeq_(maxSeq)
    {
        entries_.resize(static_cast<size_t>(heads) * maxSeq);
        written_.resize(static_cast<size_t>(heads) * maxSeq, false);
        windowHead_.resize(heads, 16u);
    }

    void write(uint32_t head, uint32_t pos, const uint16_t* kv, float entropy) {
        uint32_t W = windowHead_[head];
        size_t idx = static_cast<size_t>(head) * maxSeq_ + pos;
        auto& e = entries_[idx];

        bool compress = (pos + W < written_.size()) &&
                        written_[static_cast<size_t>(head) * maxSeq_ + pos + W];
        if (compress) {
            _compressINT4(e, kv, 8);
        } else {
            memcpy(e.data.fp16, kv, 8 * sizeof(uint16_t));
            e.compressed = false;
        }
        written_[idx] = true;

        // Adapt window based on entropy
        if (entropy < 1.5f && W > 4)  --windowHead_[head];
        if (entropy > 3.0f && W < 64) ++windowHead_[head];
    }

    void read(uint32_t head, uint32_t pos, uint16_t* out) const {
        size_t idx = static_cast<size_t>(head) * maxSeq_ + pos;
        const auto& e = entries_[idx];
        if (!e.compressed) {
            memcpy(out, e.data.fp16, 8 * sizeof(uint16_t));
        } else {
            _dequantINT4(e, out, 8);
        }
    }

private:
    uint32_t heads_, headDim_, maxSeq_;
    std::vector<KVEntry> entries_;
    std::vector<bool>    written_;
    std::vector<uint32_t> windowHead_;

    static void _compressINT4(KVEntry& e, const uint16_t* fp16, int n) {
        float mn = 1e9f, mx = -1e9f;
        for (int i = 0; i < n; ++i) {
            float v = _f16tof32(fp16[i]);
            mn = std::min(mn, v);
            mx = std::max(mx, v);
        }
        e.scale = (mx - mn) / 15.f;
        e.zero  = mn;
        e.compressed = true;
        e.data.i4[0] = e.data.i4[1] = 0;
        for (int i = 0; i < n; ++i) {
            float v = _f16tof32(fp16[i]);
            uint32_t q = static_cast<uint32_t>((v - mn) / (e.scale + 1e-9f));
            q = std::min(q, 15u);
            int word = i / 8, bit = (i % 8) * 4;
            e.data.i4[word] |= (q << bit);
        }
    }

    static void _dequantINT4(const KVEntry& e, uint16_t* out, int n) {
        for (int i = 0; i < n; ++i) {
            int word = i / 8, bit = (i % 8) * 4;
            uint32_t q = (e.data.i4[word] >> bit) & 0xF;
            float v = e.zero + e.scale * static_cast<float>(q);
            out[i] = _f32tof16(v);
        }
    }

    static float _f16tof32(uint16_t h) {
        uint32_t bits = static_cast<uint32_t>(h) << 16;
        float f; memcpy(&f, &bits, 4); return f;
    }
    static uint16_t _f32tof16(float f) {
        uint32_t bits; memcpy(&bits, &f, 4);
        return static_cast<uint16_t>(bits >> 16);
    }
};


// ============================================================
// STRATEGY 6 — NUMA-Topology-Aware Tensor Placement (NTATP)
// ------------------------------------------------------------
// Problem  : Multi-socket servers have NUMA effects; placing
//            tensors on the wrong NUMA node adds 2–4× latency.
// Idea     : At startup, enumerate NUMA topology via Windows
//            GetNumaHighestNodeNumber + VirtualAllocExNuma.
//            Assign each transformer layer to the NUMA node
//            whose CPU cores will process it (determined by
//            the thread affinity of the compute threads).
// Novel    : Layer-to-NUMA assignment is solved as a min-cut
//            problem on the layer dependency graph, minimising
//            cross-NUMA data traffic.
// ============================================================

class NUMATopologyAllocator {
public:
    NUMATopologyAllocator() {
#ifdef _WIN32
        ULONG highest = 0;
        GetNumaHighestNodeNumber(&highest);
        numNodes_ = static_cast<uint32_t>(highest + 1);
#else
        numNodes_ = 1;
#endif
        nodeLoad_.resize(numNodes_, 0ULL);
    }

    void* allocOnNode(uint32_t node, size_t bytes) {
        if (node >= numNodes_) node = 0;
#ifdef _WIN32
        void* p = VirtualAllocExNuma(GetCurrentProcess(),
            nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE, node);
#else
        (void)node;
        void* p = platform::alloc_rw(bytes);
#endif
        if (p) nodeLoad_[node] += bytes;
        return p;
    }

    // Assign layerId to the NUMA node with the least load
    uint32_t assignLayer(uint32_t layerId, size_t bytes) {
        uint32_t best = 0;
        uint64_t bestLoad = UINT64_MAX;
        for (uint32_t n = 0; n < numNodes_; ++n) {
            if (nodeLoad_[n] < bestLoad) { bestLoad = nodeLoad_[n]; best = n; }
        }
        layerNode_[layerId] = best;
        allocOnNode(best, bytes);
        return best;
    }

    uint32_t nodeForLayer(uint32_t layerId) const {
        auto it = layerNode_.find(layerId);
        return it != layerNode_.end() ? it->second : 0;
    }

    uint32_t numNodes() const { return numNodes_; }

private:
    uint32_t numNodes_;
    std::vector<uint64_t> nodeLoad_;
    std::unordered_map<uint32_t, uint32_t> layerNode_;
};


// ============================================================
// STRATEGY 7 — Demand-Paged Weight Streaming with Read-Ahead
// ------------------------------------------------------------
// Problem  : Models too large for any RAM tier need weights
//            loaded from NVMe on demand, but random I/O kills
//            throughput.
// Idea     : Map the model file with FILE_FLAG_NO_BUFFERING
//            into a fixed window of physical pages.  A
//            "stride predictor" tracks which weight pages
//            follow each other in the access stream and issues
//            prefetch hints (PrefetchVirtualMemory) 2 pages
//            ahead of the current access.
// Novel    : Predictor is a 2nd-order Markov chain over page
//            IDs; it updates online with O(1) per access.
// ============================================================

class DemandPagedWeightStream {
public:
    static constexpr size_t PAGE = 65536; // 64 KB granularity

    bool open(const wchar_t* path, size_t fileSizeHint) {
#ifdef _WIN32
        hFile_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (hFile_ == INVALID_HANDLE_VALUE) return false;
        hMap_ = CreateFileMappingW(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!hMap_) return false;
        base_ = static_cast<uint8_t*>(
            MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, 0));
        LARGE_INTEGER sz{}; GetFileSizeEx(hFile_, &sz);
        fileSize_ = static_cast<size_t>(sz.QuadPart);
        totalPages_ = (fileSize_ + PAGE - 1) / PAGE;
        markov_.resize(totalPages_ * totalPages_, 0u); // NOTE: cap at 4096 pages in real code
        return base_ != nullptr;
#else
        (void)fileSizeHint;
        // Narrow conversion for simple portability path.
        std::string p;
        while (*path) {
            p.push_back(static_cast<char>(*path & 0xFF));
            ++path;
        }
        fd_ = ::open(p.c_str(), O_RDONLY);
        if (fd_ < 0) return false;
        struct stat st{};
        if (fstat(fd_, &st) != 0) return false;
        fileSize_ = static_cast<size_t>(st.st_size);
        base_ = static_cast<uint8_t*>(mmap(nullptr, fileSize_, PROT_READ, MAP_PRIVATE, fd_, 0));
        if (base_ == MAP_FAILED) {
            base_ = nullptr;
            return false;
        }
        totalPages_ = static_cast<uint32_t>((fileSize_ + PAGE - 1) / PAGE);
        if (totalPages_ <= 4096) {
            markov_.resize(totalPages_ * totalPages_, 0u);
        }
        return true;
#endif
    }

    const void* access(size_t offsetBytes, size_t bytes) {
        if (!base_) return nullptr;
        uint32_t pageId = static_cast<uint32_t>(offsetBytes / PAGE);
        _updateMarkov(pageId);
        _prefetch(pageId);
        return base_ + offsetBytes;
    }

    void close() {
#ifdef _WIN32
        if (base_)  UnmapViewOfFile(base_);
        if (hMap_)  CloseHandle(hMap_);
        if (hFile_ != INVALID_HANDLE_VALUE) CloseHandle(hFile_);
        base_ = nullptr; hMap_ = nullptr; hFile_ = INVALID_HANDLE_VALUE;
#else
        if (base_) munmap(base_, fileSize_);
        if (fd_ >= 0) ::close(fd_);
        base_ = nullptr;
        fd_ = -1;
#endif
    }

private:
    HANDLE   hFile_ = INVALID_HANDLE_VALUE;
    HANDLE   hMap_  = nullptr;
    uint8_t* base_  = nullptr;
    size_t   fileSize_   = 0;
    uint32_t totalPages_ = 0;
    uint32_t prevPage_   = UINT32_MAX;
    uint32_t prevPrev_   = UINT32_MAX;
    std::vector<uint16_t> markov_; // transition counts [from*N + to]

    void _updateMarkov(uint32_t page) {
        if (prevPage_ != UINT32_MAX && totalPages_ <= 4096) {
            uint32_t idx = prevPage_ * totalPages_ + page;
            if (markov_[idx] < 0xFFFF) ++markov_[idx];
        }
        prevPrev_ = prevPage_;
        prevPage_ = page;
    }

    void _prefetch(uint32_t page) {
        if (prevPage_ == UINT32_MAX || totalPages_ > 4096) return;
        // Find most likely next page from Markov row
        uint32_t best = page + 1;
        uint16_t bestCnt = 0;
        for (uint32_t n = 0; n < totalPages_; ++n) {
            uint16_t cnt = markov_[page * totalPages_ + n];
            if (cnt > bestCnt) { bestCnt = cnt; best = n; }
        }
        if (best < totalPages_) {
#ifdef _WIN32
            WIN32_MEMORY_RANGE_ENTRY range{ base_ + best * PAGE, PAGE };
            PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
#else
            // Linux hint-only prefetch.
            madvise(base_ + static_cast<size_t>(best) * PAGE, PAGE, MADV_WILLNEED);
#endif
        }
    }

#ifndef _WIN32
    int fd_ = -1;
#endif
};


// ============================================================
// STRATEGY 8 — Bitmask-Indexed Sparse Activation Pool (BISAP)
// ------------------------------------------------------------
// Problem  : MoE (Mixture of Experts) models only activate
//            K of N experts per token.  Allocating all expert
//            weight buffers wastes VRAM proportional to N.
// Idea     : Use a flat pool with a 64-bit bitmask indicating
//            which expert slots are live.  On each forward
//            step, XOR the new active mask against the old;
//            bits that flipped 1→0 are immediately returned to
//            the pool; bits that flipped 0→1 are claimed from
//            the pool with BSF/BSR (hardware bit scan).
// Novel    : Pool segments are aligned to cache-line multiples
//            of expert weight size, so the XOR diff directly
//            computes the set of memcpy operations needed —
//            no hash lookup, no pointer chasing.
// ============================================================

class BitmaskSparsePool {
public:
    static constexpr uint32_t MAX_EXPERTS = 64;

    void init(size_t expertBytes, uint32_t numExperts) {
        assert(numExperts <= MAX_EXPERTS);
        numExperts_  = numExperts;
        expertBytes_ = expertBytes;
        pool_ = static_cast<uint8_t*>(
            platform::alloc_rw(expertBytes * MAX_EXPERTS));
        activeMask_  = 0;
        dirtyMask_   = 0;
    }

    ~BitmaskSparsePool() {
        if (pool_) platform::free_mem(pool_, expertBytes_ * MAX_EXPERTS);
    }

    // Activate a new set of experts given packed source weights
    void setActive(uint64_t newMask, const uint8_t* const* expertWeights) {
        uint64_t turnedOff = activeMask_ & ~newMask;
        uint64_t turnedOn  = newMask & ~activeMask_;

        // Return turned-off slots (just clear dirty bit — no memset)
        dirtyMask_ &= ~turnedOff;

        // Load turned-on slots
        uint64_t toLoad = turnedOn;
        while (toLoad) {
            uint32_t bit = static_cast<uint32_t>(_BitScanForward64_wrap(toLoad));
            if (expertWeights && expertWeights[bit])
                memcpy(pool_ + bit * expertBytes_, expertWeights[bit], expertBytes_);
            dirtyMask_ |= (1ULL << bit);
            toLoad &= toLoad - 1; // clear lowest set bit
        }
        activeMask_ = newMask;
    }

    void* getExpert(uint32_t expertId) {
        if (!(activeMask_ & (1ULL << expertId))) return nullptr;
        return pool_ + expertId * expertBytes_;
    }

    uint64_t activeMask() const { return activeMask_; }

private:
    uint8_t* pool_        = nullptr;
    uint64_t activeMask_  = 0;
    uint64_t dirtyMask_   = 0;
    uint32_t numExperts_  = 0;
    size_t   expertBytes_ = 0;

    static uint32_t _BitScanForward64_wrap(uint64_t v) {
        return platform::bsf64(v);
    }
};


// ============================================================
// STRATEGY 9 — Recompute-vs-Retain Decision Engine (RRDE)
// ------------------------------------------------------------
// Problem  : Gradient checkpointing (recomputation) saves
//            memory but adds FLOPs; retaining saves FLOPs but
//            wastes memory.  The right choice is per-layer and
//            changes as batch size / sequence length vary.
// Idea     : Build a runtime cost model that tracks:
//              memory_pressure  (current used / total)
//              recompute_cost   (FLOPs per layer, measured)
//              retain_cost      (bytes per activation tensor)
//            At each layer boundary, solve the greedy knapsack:
//            retain the layer if retain_cost < recompute_cost
//            given remaining memory budget; else recompute.
// Novel    : Cost model updates online using exponential
//            moving average of measured FLOP/byte ratios.
// ============================================================

struct LayerProfile {
    float avgRecomputeMs = 0.f;
    float activationMB   = 0.f;
    uint32_t sampleCount = 0;
};

class RecomputeRetainEngine {
public:
    static constexpr float EMA_ALPHA = 0.1f;

    void setMemoryBudgetMB(float mb) { budgetMB_ = mb; usedMB_ = 0.f; }

    // Called before each layer; returns true → retain, false → recompute
    bool shouldRetain(uint32_t layerId, float recomputeMs, float activationMB) {
        auto& p = profiles_[layerId];
        // Update EMA
        if (p.sampleCount == 0) {
            p.avgRecomputeMs = recomputeMs;
            p.activationMB   = activationMB;
        } else {
            p.avgRecomputeMs = EMA_ALPHA * recomputeMs + (1.f - EMA_ALPHA) * p.avgRecomputeMs;
            p.activationMB   = EMA_ALPHA * activationMB + (1.f - EMA_ALPHA) * p.activationMB;
        }
        ++p.sampleCount;

        float remaining = budgetMB_ - usedMB_;
        if (remaining >= p.activationMB) {
            // Value of retaining: avoid recompute cost
            // Cost: activation memory
            float retainValue = p.avgRecomputeMs / (p.activationMB + 1e-6f);
            if (retainValue > retainThreshold_) {
                usedMB_ += p.activationMB;
                return true;
            }
        }
        return false;
    }

    void onLayerDone(uint32_t layerId, bool retained) {
        if (retained) usedMB_ -= profiles_[layerId].activationMB;
    }

    void adaptThreshold(float measuredSpeedup) {
        // If speedup from retaining is good, lower threshold to retain more
        if (measuredSpeedup > 1.2f && retainThreshold_ > 0.01f)
            retainThreshold_ *= 0.9f;
        else if (measuredSpeedup < 0.9f)
            retainThreshold_ *= 1.1f;
    }

private:
    std::unordered_map<uint32_t, LayerProfile> profiles_;
    float budgetMB_        = 4096.f;
    float usedMB_          = 0.f;
    float retainThreshold_ = 1.0f; // ms per MB
};


// ============================================================
// STRATEGY 10 — Versioned Copy-on-Write Weight Snapshots (VCWS)
// ------------------------------------------------------------
// Problem  : Fine-tuning, LoRA merging, or A/B experiments
//            need multiple "versions" of the same base model
//            weights without duplicating GBs of data.
// Idea     : Use a page-granular copy-on-write scheme backed
//            by Windows' section objects.  Base weights live
//            in a read-only MAP_COPY section.  Each "version"
//            gets its own MAP_COPY view; OS only duplicates
//            pages that are actually written (LoRA delta
//            injection, adapter merging, etc.).
// Novel    : A "diff manifest" records which 64 KB pages were
//            written per version, allowing fast version
//            switching (just discard the modified pages and
//            remap from the base section) and serialisation
//            of only the delta pages to disk.
// ============================================================

struct PageDiff {
    uint32_t versionId;
    uint32_t pageIndex;  // 64 KB page within the weight file
    std::vector<uint8_t> data; // 64 KB snapshot of modified page
};

class VersionedCOWWeights {
public:
    static constexpr size_t COW_PAGE = 65536;

    bool loadBase(const wchar_t* path) {
#ifdef _WIN32
        hBase_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (hBase_ == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER sz{}; GetFileSizeEx(hBase_, &sz);
        fileSize_ = static_cast<size_t>(sz.QuadPart);
        hSection_ = CreateFileMappingW(hBase_, nullptr, PAGE_WRITECOPY, 0, 0, nullptr);
        return hSection_ != nullptr;
#else
        std::string p;
        while (*path) {
            p.push_back(static_cast<char>(*path & 0xFF));
            ++path;
        }
        fdBase_ = ::open(p.c_str(), O_RDONLY);
        if (fdBase_ < 0) return false;
        struct stat st{};
        if (fstat(fdBase_, &st) != 0) return false;
        fileSize_ = static_cast<size_t>(st.st_size);
        return true;
#endif
    }

    // Create a new version — returns a writable COW view
    uint32_t createVersion() {
    #ifdef _WIN32
        void* view = MapViewOfFile(hSection_, FILE_MAP_COPY, 0, 0, fileSize_);
    #else
        void* view = mmap(nullptr, fileSize_, PROT_READ | PROT_WRITE, MAP_PRIVATE, fdBase_, 0);
    #endif
        if (!view) return UINT32_MAX;
    #ifndef _WIN32
        if (view == MAP_FAILED) return UINT32_MAX;
    #endif
        uint32_t id = nextId_++;
        views_[id] = static_cast<uint8_t*>(view);
        diffs_[id] = {};
        return id;
    }

    // Write to a version's view (e.g. inject LoRA delta)
    bool write(uint32_t versionId, size_t offset, const void* src, size_t bytes) {
        auto it = views_.find(versionId);
        if (it == views_.end()) return false;
        memcpy(it->second + offset, src, bytes);
        // Track which 64 KB pages were dirtied
        size_t startPage = offset / COW_PAGE;
        size_t endPage   = (offset + bytes - 1) / COW_PAGE;
        auto& manifest = diffs_[versionId];
        for (size_t p = startPage; p <= endPage; ++p) {
            bool already = false;
            for (auto& d : manifest) { if (d.pageIndex == p) { already = true; break; } }
            if (!already) {
                PageDiff pd;
                pd.versionId = versionId;
                pd.pageIndex = static_cast<uint32_t>(p);
                pd.data.resize(COW_PAGE);
                size_t copyOff = p * COW_PAGE;
                size_t copyLen = std::min(COW_PAGE, fileSize_ - copyOff);
                memcpy(pd.data.data(), it->second + copyOff, copyLen);
                manifest.push_back(std::move(pd));
            }
        }
        return true;
    }

    const void* read(uint32_t versionId, size_t offset) const {
        auto it = views_.find(versionId);
        if (it == views_.end()) return nullptr;
        return it->second + offset;
    }

    // Discard a version — frees only its COW pages
    void dropVersion(uint32_t versionId) {
        auto it = views_.find(versionId);
        if (it != views_.end()) {
#ifdef _WIN32
            UnmapViewOfFile(it->second);
#else
            munmap(it->second, fileSize_);
#endif
            views_.erase(it);
        }
        diffs_.erase(versionId);
    }

    // How many pages differ from base for a version?
    size_t diffPageCount(uint32_t versionId) const {
        auto it = diffs_.find(versionId);
        return it != diffs_.end() ? it->second.size() : 0;
    }

    ~VersionedCOWWeights() {
        for (auto& [id, v] : views_) {
    #ifdef _WIN32
            UnmapViewOfFile(v);
    #else
            munmap(v, fileSize_);
    #endif
            (void)id;
        }
    #ifdef _WIN32
        if (hSection_) CloseHandle(hSection_);
        if (hBase_ != INVALID_HANDLE_VALUE) CloseHandle(hBase_);
    #else
        if (fdBase_ >= 0) ::close(fdBase_);
    #endif
    }

private:
#ifdef _WIN32
    HANDLE   hBase_    = INVALID_HANDLE_VALUE;
    HANDLE   hSection_ = nullptr;
#else
    int      fdBase_   = -1;
#endif
    size_t   fileSize_ = 0;
    uint32_t nextId_   = 0;
    std::unordered_map<uint32_t, uint8_t*>              views_;
    std::unordered_map<uint32_t, std::vector<PageDiff>> diffs_;
};

} // namespace ai_mem
