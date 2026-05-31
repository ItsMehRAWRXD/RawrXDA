// Minimal stubs to satisfy runtime_patcher dependencies for SwarmSmokeTest.
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <cstdlib>
#include <cstring>

extern "C" {
void* __iat_hook_base[128] = {};
uint64_t __iat_hook_count = 128;
uint64_t masquerade_context[16] = {};

static std::atomic<uint64_t> g_installCalls{0};
static std::atomic<uint64_t> g_getCalls{0};
static std::atomic<uint64_t> g_invalidSlotCalls{0};
static std::atomic<uint64_t> g_activeHooks{0};
static std::atomic<uint64_t> g_lastInstallSlot{0};
static std::atomic<uint64_t> g_lastGetSlot{0};
static std::atomic<uint64_t> g_overrideWindowRejects{0};
static std::atomic<uint64_t> g_windowOverrideSets{0};
static std::atomic<uint64_t> g_globalsInitCalls{0};
static std::atomic<uint64_t> g_globalsInitFail{0};
static std::atomic<uint64_t> g_lastResolvedWindow{128};

uint64_t ResolveActiveWindow() {
    const char* env = std::getenv("RAWRXD_SWARM_IAT_WINDOW");
    if (!env || env[0] == '\0') {
        return __iat_hook_count;
    }

    char* end = nullptr;
    const uint64_t parsed = std::strtoull(env, &end, 10);
    if (end == env) {
        return __iat_hook_count;
    }

    if (parsed == 0) {
        return 1;
    }

    const uint64_t resolved = parsed < __iat_hook_count ? parsed : __iat_hook_count;
    g_lastResolvedWindow.store(resolved, std::memory_order_relaxed);
    return resolved;
}

void* InstallIATHook(uint64_t slot, void* fn) {
    g_installCalls.fetch_add(1, std::memory_order_relaxed);
    g_lastInstallSlot.store(slot, std::memory_order_relaxed);
    const uint64_t activeWindow = ResolveActiveWindow();
    if (slot < activeWindow) {
        void* prev = __iat_hook_base[slot];
        __iat_hook_base[slot] = fn;
        if (prev == nullptr && fn != nullptr) {
            g_activeHooks.fetch_add(1, std::memory_order_relaxed);
        } else if (prev != nullptr && fn == nullptr) {
            const uint64_t active = g_activeHooks.load(std::memory_order_relaxed);
            if (active > 0) {
                g_activeHooks.fetch_sub(1, std::memory_order_relaxed);
            }
        }

        // Keep a tiny status snapshot for smoke assertions.
        masquerade_context[0] = slot;
        masquerade_context[1] = reinterpret_cast<uint64_t>(fn);
        masquerade_context[2] = reinterpret_cast<uint64_t>(prev);
        masquerade_context[3] = g_activeHooks.load(std::memory_order_relaxed);
        masquerade_context[6] = activeWindow;
        return prev;
    }
    g_invalidSlotCalls.fetch_add(1, std::memory_order_relaxed);
    if (slot < __iat_hook_count) {
        g_overrideWindowRejects.fetch_add(1, std::memory_order_relaxed);
    }
    return nullptr;
}

void* GetIATHook(uint64_t slot) {
    g_getCalls.fetch_add(1, std::memory_order_relaxed);
    g_lastGetSlot.store(slot, std::memory_order_relaxed);
    const uint64_t activeWindow = ResolveActiveWindow();
    if (slot < activeWindow) {
        return __iat_hook_base[slot];
    }
    g_invalidSlotCalls.fetch_add(1, std::memory_order_relaxed);
    if (slot < __iat_hook_count) {
        g_overrideWindowRejects.fetch_add(1, std::memory_order_relaxed);
    }
    return nullptr;
}

uint64_t GetMasqueradeStats() {
    // [63:56]=window rejects, [55:48]=invalid slots, [47:40]=active hooks,
    // [39:32]=gets, [31:24]=installs, [23:12]=last get slot (12 bits), [11:0]=last install slot.
    const uint64_t rejects = g_overrideWindowRejects.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t invalid = g_invalidSlotCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t active = g_activeHooks.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t gets = g_getCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t installs = g_installCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t lastGet = g_lastGetSlot.load(std::memory_order_relaxed) & 0xFFFu;
    const uint64_t lastInstall = g_lastInstallSlot.load(std::memory_order_relaxed) & 0xFFFu;
    return (rejects << 56) | (invalid << 48) | (active << 40) | (gets << 32) | (installs << 24) |
           (lastGet << 12) | lastInstall;
}

uint64_t GetMasqueradeStatsExtended() {
    // [63:56]=globals_init_fail, [55:48]=globals_init_calls, [47:40]=window_override_sets,
    // [39:32]=window_rejects, [31:24]=invalid, [23:16]=active_hooks,
    // [15:8]=resolved_window(low8), [7:0]=hook_count(low8)
    const uint64_t globalsFail = g_globalsInitFail.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t globalsCalls = g_globalsInitCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t overrideSets = g_windowOverrideSets.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t rejects = g_overrideWindowRejects.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t invalid = g_invalidSlotCalls.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t active = g_activeHooks.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t resolved = g_lastResolvedWindow.load(std::memory_order_relaxed) & 0xFFu;
    const uint64_t hookCount = __iat_hook_count & 0xFFu;
    return (globalsFail << 56) | (globalsCalls << 48) | (overrideSets << 40) | (rejects << 32) |
           (invalid << 24) | (active << 16) | (resolved << 8) | hookCount;
}

uint64_t SetMasqueradeWindow(uint64_t window) {
    if (window == 0) {
        window = 1;
    }
    if (window > __iat_hook_count) {
        window = __iat_hook_count;
    }

#if defined(_WIN32)
    char buf[32] = {0};
    std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(window));
    SetEnvironmentVariableA("RAWRXD_SWARM_IAT_WINDOW", buf);
#endif
    g_lastResolvedWindow.store(window, std::memory_order_relaxed);
    g_windowOverrideSets.fetch_add(1, std::memory_order_relaxed);
    return window;
}

uint64_t ResetMasqueradeStats() {
    g_installCalls.store(0, std::memory_order_relaxed);
    g_getCalls.store(0, std::memory_order_relaxed);
    g_invalidSlotCalls.store(0, std::memory_order_relaxed);
    g_activeHooks.store(0, std::memory_order_relaxed);
    g_lastInstallSlot.store(0, std::memory_order_relaxed);
    g_lastGetSlot.store(0, std::memory_order_relaxed);
    g_overrideWindowRejects.store(0, std::memory_order_relaxed);
    g_windowOverrideSets.store(0, std::memory_order_relaxed);
    g_globalsInitCalls.store(0, std::memory_order_relaxed);
    g_globalsInitFail.store(0, std::memory_order_relaxed);
    g_lastResolvedWindow.store(__iat_hook_count, std::memory_order_relaxed);

    for (uint64_t i = 0; i < __iat_hook_count; ++i) {
        __iat_hook_base[i] = nullptr;
    }
    std::memset(masquerade_context, 0, sizeof(masquerade_context));
    return GetMasqueradeStats();
}

void* g_hHeap = nullptr;
void* g_hInstance = nullptr;
int RawrXD_GlobalsInit(void* heap, void* instance) {
    g_globalsInitCalls.fetch_add(1, std::memory_order_relaxed);
    g_hHeap = heap;
    g_hInstance = instance;
    masquerade_context[4] = reinterpret_cast<uint64_t>(heap);
    masquerade_context[5] = reinterpret_cast<uint64_t>(instance);
    // Success when both are non-null; otherwise report a soft init failure.
    const int ok = (heap != nullptr && instance != nullptr) ? 1 : 0;
    if (!ok) {
        g_globalsInitFail.fetch_add(1, std::memory_order_relaxed);
    }
    return ok;
}
}
