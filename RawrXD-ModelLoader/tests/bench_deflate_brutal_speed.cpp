// bench_deflate_brutal_speed.cpp — Brutal stored-block gzip benchmark (pure C++20, no Qt)
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <chrono>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);

using clk = std::chrono::high_resolution_clock;

int main() {
    const size_t len = 1ull << 20; // 1 MB
    std::vector<unsigned char> src(len);
    std::mt19937 rng(42);
    for (size_t i = 0; i < len; ++i) src[i] = static_cast<unsigned char>(rng());

    size_t out_len = 0;
    auto t0 = clk::now();
    void* out = deflate_brutal_masm(src.data(), src.size(), &out_len);
    auto t1 = clk::now();
    double ms_asm = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (out) std::free(out);

    std::printf("1 MB random: Brutal MASM %.2f ms, output %zu bytes\n", ms_asm, out_len);
    return 0;
}
