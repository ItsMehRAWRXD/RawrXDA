#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>

extern "C" void flash_attn_avx2(
    const float* Q, const void* K, const float* V,
    float* O, int seqLen, int headDim);

extern "C" void flash_attn_puppeteer_avx2(
    const float* Q, const void* K, const float* V,
    float* O, int seqLen, int headDim, int quantType,
    const float* puppeteerState, float* puppeteerOut);

struct BlockQ8_0 { uint16_t d; int8_t qs[32]; };

int main() {
    using namespace std::chrono;

    const int seqLen = 16384;
    const int headDim = 256; // multiple of 32

    std::vector<float>         Q (seqLen * headDim);
    std::vector<BlockQ8_0>     Kq8(seqLen * (headDim / 32));
    std::vector<float>         V (seqLen * headDim);
    std::vector<float>         O (seqLen * headDim);
    std::vector<float>         state(256, 0.0f);
    std::vector<float>         pOut (256, 0.0f);

    std::mt19937 rng(2025);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::uniform_int_distribution<int> idist(-127, 127);
    for (auto &q : Q) q = dist(rng);
    for (auto &v : V) v = dist(rng);
    for (size_t i = 0; i < Kq8.size(); ++i) {
        Kq8[i].d = 0x3C00u; // half(1.0)
        for (int j = 0; j < 32; ++j) Kq8[i].qs[j] = (int8_t)idist(rng);
    }

    // Warmup
    flash_attn_avx2(Q.data(), Kq8.data(), V.data(), O.data(), seqLen, headDim);
    flash_attn_puppeteer_avx2(Q.data(), Kq8.data(), V.data(), O.data(), seqLen, headDim,
                              2, state.data(), pOut.data());

    auto bench = [&](auto fn)->double{
        const int iters = 3; // ultimate scale, keep iterations modest
        double total = 0.0;
        for (int i = 0; i < iters; ++i) {
            auto t0 = high_resolution_clock::now();
            fn();
            auto t1 = high_resolution_clock::now();
            total += duration<double, std::milli>(t1 - t0).count();
        }
        return total / iters;
    };

    double ms_intrin = bench([&]{
        flash_attn_avx2(Q.data(), Kq8.data(), V.data(), O.data(), seqLen, headDim);
    });
    double ms_asm = bench([&]{
        flash_attn_puppeteer_avx2(Q.data(), Kq8.data(), V.data(), O.data(), seqLen, headDim,
                                  2, state.data(), pOut.data());
    });

    double speedup = (ms_asm > 0.0) ? (ms_intrin / ms_asm) : 0.0;
    std::printf("[Ultimate 16Kx256] Intrinsics: %.2f ms  Puppeteer-ASM: %.2f ms  Speedup: %.2fx\n",
           ms_intrin, ms_asm, speedup);
    if (ms_intrin >= 1.0)
        std::puts("Runtime OK: >= 1 ms window");
    if (speedup >= 1.2)
        std::puts("✅ Puppeteer-ASM bonus: >= 1.2× over intrinsics");

    return 0;
}
