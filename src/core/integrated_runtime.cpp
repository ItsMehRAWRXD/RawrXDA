// ============================================================================
// integrated_runtime.cpp — Unified Transcendence lifecycle entry
// ============================================================================

#include "integrated_runtime.hpp"
#include "transcendence_coordinator.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace IntegratedRuntime {

namespace {

void debugLine(const char* line)
{
#ifdef _WIN32
    if (line && line[0])
        OutputDebugStringA(line);
#else
    (void)line;
#endif
}

} // namespace

static bool s_bootInvoked = false;

void boot()
{
    const char* skip = std::getenv("RAWRXD_SKIP_INTEGRATED_RUNTIME");
    if (skip && skip[0] == '1')
    {
        debugLine("[IntegratedRuntime] skipped (RAWRXD_SKIP_INTEGRATED_RUNTIME=1)\n");
        return;
    }

    PatchResult r = rawrxd::TranscendenceCoordinator::instance().initializeAll();
    s_bootInvoked = true;

    if (r.success)
    {
        debugLine(("[IntegratedRuntime] " + r.detail + "\n").c_str());
    }
    else
    {
        char buf[640];
        std::snprintf(buf, sizeof(buf),
                      "[IntegratedRuntime] coordinator: %s (code=%d) — non-fatal, continuing\n",
                      r.detail.c_str(), r.errorCode);
        debugLine(buf);
    }
}

void shutdown()
{
    if (!s_bootInvoked)
        return;

    PatchResult r = rawrxd::TranscendenceCoordinator::instance().shutdownAll();
    s_bootInvoked = false;

    if (r.success)
        debugLine(("[IntegratedRuntime] shutdown: " + r.detail + "\n").c_str());
    else
    {
        char buf[512];
        std::snprintf(buf, sizeof(buf), "[IntegratedRuntime] shutdown note: %s (code=%d)\n",
                      r.detail.c_str(), r.errorCode);
        debugLine(buf);
    }
}

} // namespace IntegratedRuntime
} // namespace RawrXD
