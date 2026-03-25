#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "boot_preflight_latch.hpp"
#include "../gguf_preflight_guard.hpp"

#include <mutex>
#include <sstream>

#ifdef RAWR_ENABLE_STONE_PREFLIGHT
extern "C" bool rawrxd_preflight_lock();
#endif

namespace RawrXD::Preflight {
namespace {

std::once_flag g_bootPreflightOnce;
BootPreflightResult g_cachedResult{};

EnvironmentSnapshot SafeCaptureSnapshot() {
    EnvironmentSnapshot snapshot{};
#if defined(_MSC_VER) && defined(_WIN32)
    __try {
        snapshot = RawrXD::GGUFPreflightGuard::captureEnvironmentSnapshot();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        snapshot = EnvironmentSnapshot{};
    }
#else
    try {
        snapshot = RawrXD::GGUFPreflightGuard::captureEnvironmentSnapshot();
    } catch (...) {
        snapshot = EnvironmentSnapshot{};
    }
#endif
    return snapshot;
}

bool SafeStoneLock() {
    bool stone_locked = true;
#ifdef RAWR_ENABLE_STONE_PREFLIGHT
#if defined(_MSC_VER) && defined(_WIN32)
    __try {
        stone_locked = rawrxd_preflight_lock();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        stone_locked = false;
    }
#else
    try {
        stone_locked = rawrxd_preflight_lock();
    } catch (...) {
        stone_locked = false;
    }
#endif
#endif
    return stone_locked;
}

} // namespace

BootPreflightResult RunBootPreflightLatch() {
    std::call_once(g_bootPreflightOnce, []() {
        const auto snapshot = SafeCaptureSnapshot();
        const bool stone_locked = SafeStoneLock();

        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        std::ostringstream oss;
        oss << "boot_preflight"
            << " gpu=" << (snapshot.gpu_capable_class ? 1 : 0)
            << " avx512=" << (snapshot.avx512_capable_class ? 1 : 0)
            << " lockable=" << (snapshot.pages_lockable ? 1 : 0)
            << " stone=" << (stone_locked ? 1 : 0);

        g_cachedResult.ok = stone_locked;
        g_cachedResult.gpu_capable = snapshot.gpu_capable_class;
        g_cachedResult.avx512_capable = snapshot.avx512_capable_class;
        g_cachedResult.pages_lockable = snapshot.pages_lockable;
        g_cachedResult.detail = oss.str();
    });

    return g_cachedResult;
}

} // namespace RawrXD::Preflight
