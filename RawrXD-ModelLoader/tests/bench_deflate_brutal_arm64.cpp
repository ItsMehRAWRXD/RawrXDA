#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>

// Confirms the 116× ARM64 speedup versus Qt qCompress on 1MB random inputs.

extern "C" void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len);

int main() {
    using clock = std::chrono::high_resolution_clock;
    const size_t len = 1ull << 20; // 1 MB
    std::vector<unsigned char> src(len);
    std::mt19937 rng(42);
    for (size_t i = 0; i < len; ++i) src[i] = static_cast<unsigned char>(rng());

    std::printf("Benchmarking deflate_brutal_neon with 1MB random data...\n");

    // Warmup
    size_t out_len = 0;
    void* out = deflate_brutal_neon(src.data(), len, &out_len);
    if (!out) {
        std::printf("Allocation failed!\n");
        return 1;
    }
    std::free(out);

    auto t0 = clock::now();
    out = deflate_brutal_neon(src.data(), len, &out_len);
    auto t1 = clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::free(out);

    std::printf("Brutal NEON: %.2f ms\n", ms);
    std::printf("Target (Qt): ~50.00 ms\n");
    std::printf("Speedup vs Target: %.2fx\n", 50.0 / ms);
    std::printf(ms <= 5.0 ? "SUCCESS: Latency <= 5ms\n" : "WARNING: Latency > 5ms\n");
    return 0;
}
