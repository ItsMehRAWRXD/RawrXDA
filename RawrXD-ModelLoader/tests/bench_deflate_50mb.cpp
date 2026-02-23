// bench_deflate_50mb.cpp — Benchmark for 50MB payload (pure C++20, no Qt)
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);

using clk = std::chrono::high_resolution_clock;

int main() {
    const size_t len = 50 * 1048576; // 50 MB
    std::vector<unsigned char> src(len);
    std::mt19937 rng(42);
    for (size_t i = 0; i < len; ++i) src[i] = static_cast<unsigned char>(rng());

    printf("===========================================\n");
    printf("Brutal MASM Stored-Block Gzip Benchmark\n");
    printf("===========================================\n");
    printf("Payload: 50 MB random data\n\n");

    size_t out_len_masm = 0;
    auto t0 = clk::now();
    void* out_masm = deflate_brutal_masm(src.data(), len, &out_len_masm);
    auto t1 = clk::now();
    double ms_masm = std::chrono::duration<double, std::milli>(t1 - t0).count();

    printf("Brutal MASM (stored blocks):\n");
    printf("  Time: %.2f ms\n", ms_masm);
    printf("  Size: %zu -> %zu bytes (%.2fx ratio)\n\n", len, out_len_masm, (double)len / (out_len_masm ? out_len_masm : 1));

    if (out_masm) std::free(out_masm);

    printf("===========================================\n");
    printf("Qt-free build: no qCompress comparison.\n");
    printf("===========================================\n");
    return 0;
}
