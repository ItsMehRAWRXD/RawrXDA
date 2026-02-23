/**
 * Additional components unit tests — C++20 only (no Qt).
 * HardwareBackendSelector, Profiler, ObservabilityDashboard.
 */
#include <cstdio>
#include <vector>

#if __has_include("hardware_backend_selector.h")
#include "hardware_backend_selector.h"
#else
#include "../../include/hardware_backend_selector.h"
#endif

#define TEST_VERIFY(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL: %s\n", #cond); ++g_fail; } } while(0)

static int g_fail = 0;

int main() {
    std::fprintf(stdout, "Additional components unit tests (C++20, no Qt)\n");
    HardwareBackendSelector selector;
    selector.initialize();
    std::vector<HardwareBackendSelector::BackendInfo> backends = selector.detectBackends();
    std::fprintf(stdout, "  Backends detected: %zu\n", backends.size());
    TEST_VERIFY(backends.size() >= 1);
    std::fprintf(stdout, "Done: %d failures\n", g_fail);
    return g_fail ? 1 : 0;
}
