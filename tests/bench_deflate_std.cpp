/**
 * bench_deflate_std.cpp — C++20 deflate benchmark (pure std::vector, no Qt).
 * Benchmarks brutal MASM/NEON compress.
 */
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

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
    std::srand(42);
    std::generate(src.begin(), src.end(), []() { return static_cast<uint8_t>(std::rand() & 0xFF); });

    using Clock = std::chrono::high_resolution_clock;
    using Ms = std::chrono::duration<double, std::milli>;

#if defined(_M_AMD64) || defined(__x86_64__)
    {
        size_t out_len_masm = 0;
        auto t0 = Clock::now();
        void* compressed_masm = deflate_brutal_masm(src.data(), len, &out_len_masm);
        auto t1 = Clock::now();
        double masm_ms = std::chrono::duration_cast<Ms>(t1 - t0).count();
        std::cout << "Brutal MASM: " << masm_ms << " ms (out " << out_len_masm << " bytes)\n";
        std::free(compressed_masm);
    }
#else
    std::cout << "Brutal MASM: N/A\n";
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
    {
        size_t out_len_neon = 0;
        auto t0 = Clock::now();
        void* compressed_neon = deflate_brutal_neon(src.data(), len, &out_len_neon);
        auto t1 = Clock::now();
        double neon_ms = std::chrono::duration_cast<Ms>(t1 - t0).count();
        std::cout << "Brutal NEON: " << neon_ms << " ms (out " << out_len_neon << " bytes)\n";
        std::free(compressed_neon);
    }
#else
    std::cout << "Brutal NEON: N/A\n";
#endif

    return 0;
}
