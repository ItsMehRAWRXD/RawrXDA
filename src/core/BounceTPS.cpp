// ============================================================================
// BounceTPS.cpp — Production Implementation
// Bounce Tensor Ping-Pong TPS Acceleration Engine
// ============================================================================
#include "BounceTPS.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <cstring>

// ─── Internal Structures ────────────────────────────────────────────────────

struct BounceTensor {
    uint32_t id;
    uint64_t offset;
    uint64_t size;
    int state;           // BOUNCE_PE_*
    uint32_t heat;        // Access frequency counter
    void* mappedPtr;      // Non-null if HOT
    HANDLE hMap;         // Mapping handle if HOT
};

struct BouncePool {
    std::vector<uint32_t> slots;  // Tensor IDs in this pool
    size_t maxSize;
    std::atomic<size_t> currentSize{0};
};

struct BounceContextInternal {
    uint32_t flags;
    int maxHot;
    int maxCold;
    int targetTPS;
    int bounceRateMs;
    int heatDecay;
    int prefetchDepth;
    
    HANDLE hFile;
    uint64_t fileSize;
    std::vector<BounceTensor> tensors;
    
    BouncePool hotPool;
    BouncePool coldPool;
    
    std::atomic<uint64_t> tokenCount{0};
    std::atomic<uint64_t> tickCount{0};
    std::atomic<uint64_t> lastTickTokens{0};
    std::atomic<int> currentTPS{0};  // Scaled by 100
    
    std::mutex mutex;
    bool valid;
    
    std::chrono::steady_clock::time_point startTime;
};

// ─── Helpers ───────────────────────────────────────────────────────────────

static inline BounceContextInternal* ToCtx(BounceContext ctx) {
    return static_cast<BounceContextInternal*>(ctx);
}

static void PromoteToHot(BounceContextInternal* ctx, BounceTensor& tensor) {
    if (tensor.state == BOUNCE_PE_HOT || tensor.state == BOUNCE_PE_BOUNCING) return;
    
    tensor.state = BOUNCE_PE_BOUNCING;
    
    // Create file mapping for this tensor region
    LARGE_INTEGER mapOffset;
    mapOffset.QuadPart = static_cast<LONGLONG>(tensor.offset);
    
    SIZE_T mapSize = static_cast<SIZE_T>(tensor.size);
    if (mapSize == 0) {
        tensor.state = BOUNCE_PE_COLD;
        return;
    }
    
    tensor.hMap = CreateFileMapping(ctx->hFile, nullptr, PAGE_READONLY,
                                     mapOffset.HighPart, mapOffset.LowPart,
                                     nullptr);
    if (tensor.hMap) {
        tensor.mappedPtr = MapViewOfFile(tensor.hMap, FILE_MAP_READ,
                                           0, 0, mapSize);
        if (tensor.mappedPtr) {
            tensor.state = BOUNCE_PE_HOT;
            ctx->hotPool.currentSize.fetch_add(tensor.size);
            if (tensor.state == BOUNCE_PE_COLD) {
                ctx->coldPool.currentSize.fetch_sub(tensor.size);
            }
        } else {
            CloseHandle(tensor.hMap);
            tensor.hMap = nullptr;
            tensor.state = BOUNCE_PE_COLD;
        }
    } else {
        tensor.state = BOUNCE_PE_COLD;
    }
}

static void DemoteToCold(BounceContextInternal* ctx, BounceTensor& tensor) {
    if (tensor.state != BOUNCE_PE_HOT) return;
    
    if (tensor.mappedPtr) {
        UnmapViewOfFile(tensor.mappedPtr);
        tensor.mappedPtr = nullptr;
    }
    if (tensor.hMap) {
        CloseHandle(tensor.hMap);
        tensor.hMap = nullptr;
    }
    
    ctx->hotPool.currentSize.fetch_sub(tensor.size);
    ctx->coldPool.currentSize.fetch_add(tensor.size);
    tensor.state = BOUNCE_PE_COLD;
}

static void RebalancePools(BounceContextInternal* ctx) {
    std::lock_guard<std::mutex> lock(ctx->mutex);
    
    // Sort tensors by heat (descending)
    std::vector<std::pair<uint32_t, uint32_t>> heatList;
    heatList.reserve(ctx->tensors.size());
    for (const auto& t : ctx->tensors) {
        heatList.emplace_back(t.id, t.heat);
    }
    std::sort(heatList.begin(), heatList.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    size_t hotBytes = 0;
    size_t hotLimit = static_cast<size_t>(ctx->maxHot) * 1024 * 1024; // MB to bytes
    
    for (const auto& [id, heat] : heatList) {
        if (id >= ctx->tensors.size()) continue;
        auto& tensor = ctx->tensors[id];
        
        if (hotBytes + tensor.size <= hotLimit) {
            if (tensor.state == BOUNCE_PE_COLD) {
                PromoteToHot(ctx, tensor);
            }
            hotBytes += tensor.size;
        } else {
            if (tensor.state == BOUNCE_PE_HOT) {
                DemoteToCold(ctx, tensor);
            }
        }
    }
}

// ─── API Implementation ────────────────────────────────────────────────────

extern "C" {

BounceContext Bounce_Init(uint32_t flags) {
    auto* ctx = new (std::nothrow) BounceContextInternal();
    if (!ctx) return nullptr;
    
    ctx->flags = flags;
    ctx->maxHot = BOUNCE_DEFAULT_MAX_HOT;
    ctx->maxCold = BOUNCE_DEFAULT_MAX_COLD;
    ctx->targetTPS = BOUNCE_DEFAULT_TARGET_TPS;
    ctx->bounceRateMs = BOUNCE_DEFAULT_BOUNCE_RATE_MS;
    ctx->heatDecay = BOUNCE_DEFAULT_HEAT_DECAY;
    ctx->prefetchDepth = BOUNCE_DEFAULT_PREFETCH_DEPTH;
    ctx->hFile = INVALID_HANDLE_VALUE;
    ctx->fileSize = 0;
    ctx->valid = true;
    ctx->startTime = std::chrono::steady_clock::now();
    
    return ctx;
}

void Bounce_Destroy(BounceContext ctx) {
    auto* c = ToCtx(ctx);
    if (!c) return;
    
    std::lock_guard<std::mutex> lock(c->mutex);
    
    // Demote all tensors to release mappings
    for (auto& tensor : c->tensors) {
        if (tensor.state == BOUNCE_PE_HOT) {
            DemoteToCold(c, tensor);
        }
    }
    
    if (c->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(c->hFile);
    }
    
    c->valid = false;
    delete c;
}

int Bounce_AttachModel(BounceContext ctx, HANDLE hFile, uint64_t fileSize,
                       uint32_t tensorCount, const uint64_t* tensorOffsets,
                       const uint64_t* tensorSizes) {
    auto* c = ToCtx(ctx);
    if (!c || !c->valid || hFile == INVALID_HANDLE_VALUE || tensorCount == 0 ||
        !tensorOffsets || !tensorSizes) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(c->mutex);
    
    if (c->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(c->hFile);
    }
    
    // Duplicate handle for our own use
    HANDLE dupHandle;
    if (!DuplicateHandle(GetCurrentProcess(), hFile, GetCurrentProcess(),
                         &dupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        return 0;
    }
    
    c->hFile = dupHandle;
    c->fileSize = fileSize;
    c->tensors.clear();
    c->tensors.reserve(tensorCount);
    
    for (uint32_t i = 0; i < tensorCount; ++i) {
        BounceTensor t{};
        t.id = i;
        t.offset = tensorOffsets[i];
        t.size = tensorSizes[i];
        t.state = BOUNCE_PE_COLD;
        t.heat = 0;
        t.mappedPtr = nullptr;
        t.hMap = nullptr;
        c->tensors.push_back(t);
        c->coldPool.currentSize.fetch_add(t.size);
    }
    
    return 1;
}

int Bounce_Tick(BounceContext ctx) {
    auto* c = ToCtx(ctx);
    if (!c || !c->valid) return 0;
    
    c->tickCount.fetch_add(1);
    
    // Calculate TPS
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - c->startTime).count();
    
    if (elapsed > 0) {
        uint64_t totalTokens = c->tokenCount.load();
        int tps = static_cast<int>((totalTokens * 100) / elapsed); // Scaled by 100
        c->currentTPS.store(tps);
    }
    
    // Decay heat
    {
        std::lock_guard<std::mutex> lock(c->mutex);
        for (auto& tensor : c->tensors) {
            if (tensor.heat > 0) {
                tensor.heat -= static_cast<uint32_t>(c->heatDecay);
            }
        }
    }
    
    // Auto-tune: rebalance if TPS is below target
    if (c->currentTPS.load() < c->targetTPS * 100) {
        RebalancePools(c);
    }
    
    return static_cast<int>(c->tensors.size());
}

void* Bounce_GetTensor(BounceContext ctx, uint32_t tensorId) {
    auto* c = ToCtx(ctx);
    if (!c || !c->valid) return nullptr;
    
    std::lock_guard<std::mutex> lock(c->mutex);
    if (tensorId >= c->tensors.size()) return nullptr;
    
    auto& tensor = c->tensors[tensorId];
    tensor.heat += 10;  // Access boosts heat
    
    if (tensor.state == BOUNCE_PE_HOT && tensor.mappedPtr) {
        return tensor.mappedPtr;
    }
    
    // Promote on access
    PromoteToHot(c, tensor);
    return tensor.mappedPtr;
}

void Bounce_NotifyTokenGen(BounceContext ctx) {
    auto* c = ToCtx(ctx);
    if (!c) return;
    c->tokenCount.fetch_add(1);
}

float Bounce_GetTPS(BounceContext ctx) {
    auto* c = ToCtx(ctx);
    if (!c) return 0.0f;
    return static_cast<float>(c->currentTPS.load()) / 100.0f;
}

void Bounce_SetTargetTPS(BounceContext ctx, int target) {
    auto* c = ToCtx(ctx);
    if (!c || target <= 0) return;
    c->targetTPS = target;
}

void Bounce_SetPoolRatio(BounceContext ctx, int hotPercent, int coldPercent) {
    auto* c = ToCtx(ctx);
    if (!c || hotPercent < 0 || coldPercent < 0 || hotPercent + coldPercent > 100) return;
    
    // Convert percentage to MB (heuristic: assume 1GB total)
    c->maxHot = (hotPercent * 1024) / 100;
    c->maxCold = (coldPercent * 1024) / 100;
}

} // extern "C"
