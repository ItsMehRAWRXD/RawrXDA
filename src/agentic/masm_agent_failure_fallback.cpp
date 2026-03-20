#include <atomic>
#include <cstdint>

namespace {
std::atomic<uint32_t> g_masmAgentFailureChecks{0};
}

extern "C" void masm_agent_failure_detect_simd(void) {
    g_masmAgentFailureChecks.fetch_add(1, std::memory_order_relaxed);
}
