// bench_deflate_std.cpp — C++20 deflate benchmark (pure std::vector, no Qt).
// Benchmarks brutal MASM gzip compression using std::vector and std::chrono.

#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);
extern "C" void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len);

#if defined(_M_AMD64) || defined(__x86_64__)
extern "C" void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len) {
    (void)src;
    (void)len;
    (void)out_len;
    return nullptr;
}
#elif defined(_M_ARM64) || defined(__aarch64__)
extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
    (void)src;
    (void)len;
    (void)out_len;
    return nullptr;
}
#endif

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    const size_t len = 1048576; // 1 MB
    std::vector<uint8_t> src(len);
    std::mt19937 rng(42u);
    std::uniform_int_distribution<int> dist(0, 255);
    std::generate(src.begin(), src.end(), [&]() { return static_cast<uint8_t>(dist(rng)); });

    using Clock = std::chrono::high_resolution_clock;
    using Ms = std::chrono::duration<double, std::milli>;

#if defined(_M_AMD64) || defined(__x86_64__)
    {
        auto t0 = Clock::now();
        size_t out_len = 0;
        void* compressed = deflate_brutal_masm(src.data(), len, &out_len);
        auto t1 = Clock::now();
        double masm_ms = std::chrono::duration_cast<Ms>(t1 - t0).count();
        std::cout << "Brutal MASM: " << masm_ms << " ms (out " << out_len << " bytes)\n";
        std::free(compressed);
    }
#else
    std::cout << "Brutal MASM: N/A (not x64)\n";
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
    {
        auto t0 = Clock::now();
        size_t out_len = 0;
        void* compressed = deflate_brutal_neon(src.data(), len, &out_len);
        auto t1 = Clock::now();
        double neon_ms = std::chrono::duration_cast<Ms>(t1 - t0).count();
        std::cout << "Brutal NEON: " << neon_ms << " ms (out " << out_len << " bytes)\n";
        std::free(compressed);
    }
#else
    std::cout << "Brutal NEON: N/A (not ARM64)\n";
#endif

    return 0;
}
