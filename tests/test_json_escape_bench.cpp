#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "agentic/json_pulse.h"

int main() {
    using clock = std::chrono::steady_clock;
    using namespace rawrxd::agentic::jsonpulse;

    JsonPulse pulse;

    const std::string snippet =
        "{\"path\":\"d:/rawrxd/src/agentic/agentic_executor.cpp\",\"text\":\"line1\\nline2\\t\\\"quoted\\\"\\\\slash\"}";

    constexpr int smallIters = 200000;
    std::vector<char> outSmall(snippet.size() * 8, '\0');

    std::size_t smallBytes = 0;
    const auto t0 = clock::now();
    for (int i = 0; i < smallIters; ++i) {
        smallBytes += pulse.Encode(snippet.data(), snippet.size(), outSmall.data(), outSmall.size());
    }
    const auto t1 = clock::now();

    const double smallTotalNs = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    const double smallNsPerIter = smallTotalNs / static_cast<double>(smallIters);
    const double smallUsPerIter = smallNsPerIter / 1000.0;

    // 1 MiB payload path to estimate full-context serialization throughput.
    constexpr std::size_t oneMiB = 1024 * 1024;
    std::string big(oneMiB, 'A');
    for (std::size_t i = 0; i < big.size(); i += 97) {
        big[i] = '"';
    }
    for (std::size_t i = 53; i < big.size(); i += 211) {
        big[i] = '\\';
    }
    for (std::size_t i = 19; i < big.size(); i += 307) {
        big[i] = '\n';
    }

    std::vector<char> outBig(big.size() * 8, '\0');
    constexpr int bigIters = 12;

    std::size_t bigBytes = 0;
    const auto t2 = clock::now();
    for (int i = 0; i < bigIters; ++i) {
        bigBytes += pulse.Encode(big.data(), big.size(), outBig.data(), outBig.size());
    }
    const auto t3 = clock::now();

    const double bigTotalNs = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count());
    const double bigNsPerIter = bigTotalNs / static_cast<double>(bigIters);
    const double bigUsPerIter = bigNsPerIter / 1000.0;
    const double bigMsPerIter = bigUsPerIter / 1000.0;
    const double bigGbps = (static_cast<double>(oneMiB) / (bigNsPerIter * 1e-9)) / 1e9;

    // High-density dirty-path workload: quotes/backslashes/newlines dominate.
    std::string worst(oneMiB, '"');
    for (std::size_t i = 1; i < worst.size(); i += 2) {
        worst[i] = '\\';
    }
    for (std::size_t i = 3; i < worst.size(); i += 7) {
        worst[i] = '\n';
    }

    std::vector<char> outWorst(worst.size() * 8, '\0');
    constexpr int worstIters = 12;

    std::size_t worstBytes = 0;
    const auto t4 = clock::now();
    for (int i = 0; i < worstIters; ++i) {
        worstBytes += pulse.Encode(worst.data(), worst.size(), outWorst.data(), outWorst.size());
    }
    const auto t5 = clock::now();

    const double worstTotalNs = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t5 - t4).count());
    const double worstNsPerIter = worstTotalNs / static_cast<double>(worstIters);
    const double worstUsPerIter = worstNsPerIter / 1000.0;
    const double worstMsPerIter = worstUsPerIter / 1000.0;
    const double worstGbps = (static_cast<double>(oneMiB) / (worstNsPerIter * 1e-9)) / 1e9;

    std::ofstream metrics("d:/rxdn/tests/json_bench_latest.txt", std::ios::trunc);

    auto emit = [&](const std::string& key, const std::string& value) {
        std::cout << key << "=" << value << "\n";
        if (metrics.is_open()) {
            metrics << key << "=" << value << "\n";
        }
    };

    std::cout << std::fixed << std::setprecision(3);
    if (metrics.is_open()) {
        metrics << std::fixed << std::setprecision(3);
    }

    emit("json_pulse_has_vbmi2", (pulse.hasVbmi2() ? "true" : "false"));
    emit("small_input_bytes", std::to_string(snippet.size()));
    emit("small_iterations", std::to_string(smallIters));
    emit("small_total_ns", std::to_string(static_cast<std::uint64_t>(smallTotalNs)));
    emit("small_ns_per_iteration", std::to_string(smallNsPerIter));
    emit("small_us_per_iteration", std::to_string(smallUsPerIter));
    emit("small_output_bytes_accum", std::to_string(smallBytes));
    emit("big_input_bytes", std::to_string(oneMiB));
    emit("big_iterations", std::to_string(bigIters));
    emit("big_total_ns", std::to_string(static_cast<std::uint64_t>(bigTotalNs)));
    emit("big_us_per_iteration", std::to_string(bigUsPerIter));
    emit("big_ms_per_iteration", std::to_string(bigMsPerIter));
    emit("big_gbps", std::to_string(bigGbps));
    emit("big_output_bytes_accum", std::to_string(bigBytes));
    emit("worst_input_bytes", std::to_string(oneMiB));
    emit("worst_iterations", std::to_string(worstIters));
    emit("worst_total_ns", std::to_string(static_cast<std::uint64_t>(worstTotalNs)));
    emit("worst_us_per_iteration", std::to_string(worstUsPerIter));
    emit("worst_ms_per_iteration", std::to_string(worstMsPerIter));
    emit("worst_gbps", std::to_string(worstGbps));
    emit("worst_output_bytes_accum", std::to_string(worstBytes));

    if (metrics.is_open()) {
        metrics.flush();
    }
    std::cout.flush();

    return 0;
}
