#include <chrono>
#include <cstdint>
#include <iostream>

#include "agentic_executor_avx512.h"

int main() {
    using namespace rawrxd::agentic::avx512;
    using clock = std::chrono::steady_clock;

    AgentRegisterHotState state{};
    initializeHotState(state, "benchmark_goal", "D:/rawrxd");

    constexpr int warmupIters = 2000;
    constexpr int measureIters = 50000;

    for (int i = 0; i < warmupIters; ++i) {
        state.iteration = static_cast<uint32_t>(i);
        loadContext(state);
        executeStep(state);
        storeContext(state);
    }

    const auto t0 = clock::now();
    for (int i = 0; i < measureIters; ++i) {
        state.iteration = static_cast<uint32_t>(i);
        loadContext(state);
        executeStep(state);
        storeContext(state);
    }
    const auto t1 = clock::now();

    const double totalNs = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    const double nsPerIter = totalNs / static_cast<double>(measureIters);
    const double usPerIter = nsPerIter / 1000.0;

    std::cout << "agentic_kernel_linked=" << (kernelExportsAvailable() ? "true" : "false") << "\n";
    std::cout << "iterations=" << measureIters << "\n";
    std::cout << "total_ns=" << static_cast<uint64_t>(totalNs) << "\n";
    std::cout << "ns_per_iteration=" << nsPerIter << "\n";
    std::cout << "us_per_iteration=" << usPerIter << "\n";

    if (state.lastStepSuccess == 0) {
        std::cerr << "Kernel step did not set success flag" << std::endl;
        return 1;
    }

    return 0;
}
