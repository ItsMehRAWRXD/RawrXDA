#if !defined(_MSC_VER)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "copilot_gap_closer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD {

namespace {

struct VecNode {
    std::vector<float> embedding;
    bool deleted{false};
};

struct ComposerOp {
    std::string path;
    int32_t opType{0};
    std::vector<uint8_t> content;
};

struct ComposerTx {
    std::vector<ComposerOp> ops;
};

struct CrdtDoc {
    uint8_t uuid[16]{};
    int32_t peerId{0};
    int64_t lamport{0};
    std::string text;
};

std::mutex g_vecMutex;
std::vector<VecNode> g_vecNodesOwned;
std::atomic<int64_t> g_composerTxCount{0};
std::mutex g_composerMutex;
std::mutex g_crdtMutex;
std::mutex g_gitMutex;
std::string g_gitBranch = "unknown";
std::string g_gitCommit = "unknown";

inline uint64_t ticksNow() {
    return static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

inline void perfRecord(GapCloserPerfCounter& perf, uint64_t startTicks) {
    const uint64_t elapsed = ticksNow() - startTicks;
    ++perf.calls;
    perf.lastCycles = elapsed;
    perf.totalCycles += elapsed;
}

}  // namespace

extern "C" void* g_VecDbNodes = nullptr;
extern "C" int32_t g_VecDbNumNodes = 0;
extern "C" int32_t g_VecDbEntryPoint = -1;
extern "C" int32_t g_VecDbMaxLevel = 0;
extern "C" GapCloserPerfCounter g_VecDbPerf = {};

extern "C" int32_t g_ComposerState = COMPOSER_STATE_IDLE;
extern "C" GapCloserPerfCounter g_ComposerPerf = {};

extern "C" void* g_CrdtLocalDoc = nullptr;
extern "C" int32_t g_CrdtPeerCount = 0;
extern "C" GapCloserPerfCounter g_CrdtPerf = {};

extern "C" int32_t VecDb_Init() {
    const uint64_t t0 = ticksNow();
    std::lock_guard<std::mutex> lock(g_vecMutex);
    g_vecNodesOwned.clear();
    g_VecDbNumNodes = 0;
    g_VecDbEntryPoint = -1;
    g_VecDbMaxLevel = 0;
    g_VecDbNodes = &g_vecNodesOwned;
    perfRecord(g_VecDbPerf, t0);
    return 0;
}

extern "C" int32_t VecDb_Insert(const float* vector, void*) {
    const uint64_t t0 = ticksNow();
    if (!vector) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(g_vecMutex);
    VecNode node;
    node.embedding.assign(vector, vector + VECDB_DIMENSIONS);
    g_vecNodesOwned.push_back(std::move(node));

    g_VecDbNumNodes = static_cast<int32_t>(g_vecNodesOwned.size());
    if (g_VecDbEntryPoint < 0) {
        g_VecDbEntryPoint = 0;
    }
    g_VecDbMaxLevel = std::min<int32_t>(VECDB_MAX_LEVEL, 1 + (g_VecDbNumNodes / 10000));
    perfRecord(g_VecDbPerf, t0);
    return g_VecDbNumNodes - 1;
}

extern "C" float VecDb_L2Distance_AVX2(const float* a, const float* b) {
    if (!a || !b) {
        return 0.0f;
    }
    double sum = 0.0;
    for (int i = 0; i < VECDB_DIMENSIONS; ++i) {
        const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        sum += d * d;
    }
    return static_cast<float>(sum);
}

extern "C" int32_t VecDb_Search(const float* query, int32_t* results, int32_t k) {
    const uint64_t t0 = ticksNow();
    if (!query || !results || k <= 0) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(g_vecMutex);
    std::vector<std::pair<float, int32_t>> scored;
    scored.reserve(g_vecNodesOwned.size());
    for (int32_t i = 0; i < static_cast<int32_t>(g_vecNodesOwned.size()); ++i) {
        if (g_vecNodesOwned[static_cast<size_t>(i)].deleted) {
            continue;
        }
        const float dist = VecDb_L2Distance_AVX2(query, g_vecNodesOwned[static_cast<size_t>(i)].embedding.data());
        scored.emplace_back(dist, i);
    }
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    const int32_t n = std::min<int32_t>(k, static_cast<int32_t>(scored.size()));
    for (int32_t i = 0; i < n; ++i) {
        results[i] = scored[static_cast<size_t>(i)].second;
    }
    perfRecord(g_VecDbPerf, t0);
    return n;
}

extern "C" int32_t VecDb_Delete(int32_t nodeId) {
    const uint64_t t0 = ticksNow();
    std::lock_guard<std::mutex> lock(g_vecMutex);
    if (nodeId < 0 || nodeId >= static_cast<int32_t>(g_vecNodesOwned.size())) {
        return -1;
    }
    g_vecNodesOwned[static_cast<size_t>(nodeId)].deleted = true;
    perfRecord(g_VecDbPerf, t0);
    return 0;
}

extern "C" int32_t VecDb_GetNodeCount() {
    std::lock_guard<std::mutex> lock(g_vecMutex);
    int32_t alive = 0;
    for (const auto& n : g_vecNodesOwned) {
        if (!n.deleted) {
            ++alive;
        }
    }
    return alive;
}

extern "C" void* Composer_BeginTransaction() {
    const uint64_t t0 = ticksNow();
    std::lock_guard<std::mutex> lock(g_composerMutex);
    if (g_ComposerState == COMPOSER_STATE_PENDING || g_ComposerState == COMPOSER_STATE_APPLYING) {
        return nullptr;
    }
    g_ComposerState = COMPOSER_STATE_PENDING;
    auto* tx = new ComposerTx();
    perfRecord(g_ComposerPerf, t0);
    return tx;
}

extern "C" int32_t Composer_AddFileOp(void* tx, const char* path, int32_t opType,
                                      const void* content, uint64_t contentLen) {
    const uint64_t t0 = ticksNow();
    if (!tx || !path) {
        return 0;
    }
    auto* ctx = static_cast<ComposerTx*>(tx);
    ComposerOp op;
    op.path = path;
    op.opType = opType;
    if (content && contentLen > 0) {
        const auto* bytes = static_cast<const uint8_t*>(content);
        op.content.assign(bytes, bytes + contentLen);
    }
    ctx->ops.push_back(std::move(op));
    perfRecord(g_ComposerPerf, t0);
    return 1;
}

extern "C" int32_t Composer_Commit(void* tx) {
    const uint64_t t0 = ticksNow();
    if (!tx) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_composerMutex);
    g_ComposerState = COMPOSER_STATE_APPLYING;

    auto* ctx = static_cast<ComposerTx*>(tx);
    const bool ok = (ctx->ops.size() <= static_cast<size_t>(COMPOSER_MAX_OPS));
    g_ComposerState = ok ? COMPOSER_STATE_COMMITTED : COMPOSER_STATE_ROLLBACK;
    if (ok) {
        g_composerTxCount.fetch_add(1, std::memory_order_relaxed);
    }
    delete ctx;

    perfRecord(g_ComposerPerf, t0);
    return ok ? 0 : -2;
}

extern "C" int32_t Composer_GetState() {
    return g_ComposerState;
}

extern "C" int64_t Composer_GetTxCount() {
    return g_composerTxCount.load(std::memory_order_relaxed);
}

extern "C" void* Crdt_InitDocument(const void* uuid, int32_t peerId) {
    const uint64_t t0 = ticksNow();
    auto* doc = new CrdtDoc();
    if (uuid) {
        std::memcpy(doc->uuid, uuid, sizeof(doc->uuid));
    }
    doc->peerId = peerId;
    doc->lamport = 1;

    {
        std::lock_guard<std::mutex> lock(g_crdtMutex);
        if (g_CrdtLocalDoc) {
            delete static_cast<CrdtDoc*>(g_CrdtLocalDoc);
        }
        g_CrdtLocalDoc = doc;
        g_CrdtPeerCount = 1;
    }

    perfRecord(g_CrdtPerf, t0);
    return doc;
}

extern "C" int64_t Crdt_InsertText(void* doc, uint64_t position,
                                   const void* text, int32_t length) {
    const uint64_t t0 = ticksNow();
    if (!doc || !text || length <= 0) {
        return -1;
    }
    auto* d = static_cast<CrdtDoc*>(doc);
    const size_t pos = static_cast<size_t>(std::min<uint64_t>(position, d->text.size()));
    d->text.insert(pos, static_cast<const char*>(text), static_cast<size_t>(length));
    ++d->lamport;
    perfRecord(g_CrdtPerf, t0);
    return d->lamport;
}

extern "C" int64_t Crdt_DeleteText(void* doc, uint64_t position, int32_t length) {
    const uint64_t t0 = ticksNow();
    if (!doc || length <= 0) {
        return -1;
    }
    auto* d = static_cast<CrdtDoc*>(doc);
    if (position >= d->text.size()) {
        return d->lamport;
    }
    const size_t pos = static_cast<size_t>(position);
    const size_t len = std::min<size_t>(static_cast<size_t>(length), d->text.size() - pos);
    d->text.erase(pos, len);
    ++d->lamport;
    perfRecord(g_CrdtPerf, t0);
    return d->lamport;
}

extern "C" int32_t Crdt_MergeRemoteOp(void* doc, const void*) {
    if (!doc) {
        return -1;
    }
    auto* d = static_cast<CrdtDoc*>(doc);
    ++d->lamport;
    return 0;
}

extern "C" int64_t Crdt_GetDocLength(void* doc) {
    if (!doc) {
        return 0;
    }
    return static_cast<int64_t>(static_cast<CrdtDoc*>(doc)->text.size());
}

extern "C" int64_t Crdt_GetLamport(void* doc) {
    if (!doc) {
        return 0;
    }
    return static_cast<CrdtDoc*>(doc)->lamport;
}

extern "C" void* Git_ExtractContext(const char* repoPath, const char* currentFile,
                                    int32_t lineNumber) {
    std::lock_guard<std::mutex> lock(g_gitMutex);
    std::string ctx = "repo=";
    ctx += (repoPath ? repoPath : "");
    ctx += "; file=";
    ctx += (currentFile ? currentFile : "");
    ctx += "; line=" + std::to_string(lineNumber);
    ctx += "; branch=" + g_gitBranch;
    ctx += "; commit=" + g_gitCommit;

    HGLOBAL mem = GlobalAlloc(GMEM_FIXED, ctx.size() + 1);
    if (!mem) {
        return nullptr;
    }
    std::memcpy(mem, ctx.c_str(), ctx.size() + 1);
    return mem;
}

extern "C" void Git_SetBranch(const char* branch) {
    std::lock_guard<std::mutex> lock(g_gitMutex);
    g_gitBranch = branch ? branch : "";
}

extern "C" void Git_SetCommitHash(const char* hash) {
    std::lock_guard<std::mutex> lock(g_gitMutex);
    g_gitCommit = hash ? hash : "";
}

extern "C" void GapCloser_GetPerfCounters(GapCloserPerfCounter* out3) {
    if (!out3) {
        return;
    }
    out3[0] = g_VecDbPerf;
    out3[1] = g_ComposerPerf;
    out3[2] = g_CrdtPerf;
}

}  // namespace RawrXD

#endif  // !defined(_MSC_VER)
