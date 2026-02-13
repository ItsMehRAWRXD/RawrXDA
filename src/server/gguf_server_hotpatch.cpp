// gguf_server_hotpatch.cpp — Server-Layer Hotpatching (Layer 3) Implementation
// Modify inference request/response at runtime.
// Injection Points: PreRequest, PostRequest, PreResponse, PostResponse, StreamChunk
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "gguf_server_hotpatch.hpp"
#include <cstring>
#include <mutex>
#include <atomic>

// ---------------------------------------------------------------------------
// Server Injection Points
// ---------------------------------------------------------------------------
enum class ServerInjectionPoint : uint8_t {
    PreRequest    = 0,
    PostRequest   = 1,
    PreResponse   = 2,
    PostResponse  = 3,
    StreamChunk   = 4,
};

// ---------------------------------------------------------------------------
// ServerHotpatchStats
// ---------------------------------------------------------------------------
struct ServerHotpatchStats {
    std::atomic<uint64_t> requestsProcessed{0};
    std::atomic<uint64_t> responsesProcessed{0};
    std::atomic<uint64_t> patchesTriggered{0};
    std::atomic<uint64_t> patchesFailed{0};
    std::atomic<uint64_t> chunksIntercepted{0};
};

// ---------------------------------------------------------------------------
// External ASM entry points (request_patch.asm)
// ---------------------------------------------------------------------------
extern "C" {
    int asm_intercept_request(void* reqBuffer, size_t reqLen);
    int asm_intercept_response(void* respBuffer, size_t respLen);
}

// ---------------------------------------------------------------------------
// GGUFServerHotpatch implementation
// ---------------------------------------------------------------------------
static std::mutex           g_serverMutex;
static ServerHotpatchStats  g_serverStats;

GGUFServerHotpatch& GGUFServerHotpatch::instance() {
    static GGUFServerHotpatch inst;
    return inst;
}

void GGUFServerHotpatch::add_patch(const ServerHotpatch& patch) {
    std::lock_guard<std::mutex> lock(g_serverMutex);
    m_patches.push_back(patch);
}

bool GGUFServerHotpatch::apply_patches(Request* req, Response* res) {
    if (!req || !res) return false;

    std::lock_guard<std::mutex> lock(g_serverMutex);
    bool allSuccess = true;

    g_serverStats.requestsProcessed.fetch_add(1, std::memory_order_relaxed);

    for (auto& patch : m_patches) {
        if (!patch.transform) continue;

        bool result = patch.transform(req, res);
        patch.hit_count++;
        g_serverStats.patchesTriggered.fetch_add(1, std::memory_order_relaxed);

        if (!result) {
            g_serverStats.patchesFailed.fetch_add(1, std::memory_order_relaxed);
            allSuccess = false;
        }
    }

    g_serverStats.responsesProcessed.fetch_add(1, std::memory_order_relaxed);
    return allSuccess;
}

// ---------------------------------------------------------------------------
// Extended API — Injection-point-specific patch application
// ---------------------------------------------------------------------------
static std::vector<ServerHotpatch> g_preRequestPatches;
static std::vector<ServerHotpatch> g_postRequestPatches;
static std::vector<ServerHotpatch> g_preResponsePatches;
static std::vector<ServerHotpatch> g_postResponsePatches;
static std::vector<ServerHotpatch> g_streamChunkPatches;

static std::vector<ServerHotpatch>& get_patch_list(ServerInjectionPoint point) {
    switch (point) {
        case ServerInjectionPoint::PreRequest:    return g_preRequestPatches;
        case ServerInjectionPoint::PostRequest:   return g_postRequestPatches;
        case ServerInjectionPoint::PreResponse:   return g_preResponsePatches;
        case ServerInjectionPoint::PostResponse:  return g_postResponsePatches;
        case ServerInjectionPoint::StreamChunk:   return g_streamChunkPatches;
        default:                                  return g_preRequestPatches;
    }
}

// Add a patch at a specific injection point
PatchResult server_add_patch_at(ServerInjectionPoint point, const ServerHotpatch& patch) {
    if (!patch.name || !patch.transform) {
        return PatchResult::error("Null patch name or transform function", 1);
    }
    std::lock_guard<std::mutex> lock(g_serverMutex);
    get_patch_list(point).push_back(patch);
    return PatchResult::ok("Server patch added at injection point");
}

// Remove a patch by name from a specific injection point
PatchResult server_remove_patch_at(ServerInjectionPoint point, const char* name) {
    if (!name) return PatchResult::error("Null name", 1);
    std::lock_guard<std::mutex> lock(g_serverMutex);
    auto& list = get_patch_list(point);
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (std::strcmp(it->name, name) == 0) {
            list.erase(it);
            return PatchResult::ok("Server patch removed");
        }
    }
    return PatchResult::error("Patch not found at injection point", 2);
}

// Apply all patches at a given injection point
bool server_apply_patches_at(ServerInjectionPoint point, Request* req, Response* res) {
    std::lock_guard<std::mutex> lock(g_serverMutex);
    auto& list = get_patch_list(point);
    bool allOk = true;
    for (auto& p : list) {
        if (!p.transform) continue;
        bool result = p.transform(req, res);
        p.hit_count++;
        g_serverStats.patchesTriggered.fetch_add(1, std::memory_order_relaxed);
        if (!result) {
            g_serverStats.patchesFailed.fetch_add(1, std::memory_order_relaxed);
            allOk = false;
        }
    }
    return allOk;
}

// Get server patch statistics
const ServerHotpatchStats& get_server_hotpatch_stats() {
    return g_serverStats;
}

void reset_server_hotpatch_stats() {
    g_serverStats.requestsProcessed.store(0, std::memory_order_relaxed);
    g_serverStats.responsesProcessed.store(0, std::memory_order_relaxed);
    g_serverStats.patchesTriggered.store(0, std::memory_order_relaxed);
    g_serverStats.patchesFailed.store(0, std::memory_order_relaxed);
    g_serverStats.chunksIntercepted.store(0, std::memory_order_relaxed);
}
