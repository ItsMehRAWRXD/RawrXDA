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

// Keep in sync with kernels/q8_0_avx2.cc layout
struct BlockQ8_0 {
    uint16_t d;      // half scale
    int8_t qs[32];
};

int main() {
    using namespace std::chrono;

    const int seqLen = 4096;
    const int headDim = 64; // multiple of 32

    std::vector<float>         Q (seqLen * headDim);
    std::vector<BlockQ8_0>     Kq8(seqLen * (headDim / 32));
    std::vector<float>         V (seqLen * headDim);
    std::vector<float>         O (seqLen * headDim);
    std::vector<float>         state(256, 0.0f);
    std::vector<float>         pOut (256, 0.0f);

    // Initialize buffers with deterministic pseudo-random data
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::uniform_int_distribution<int> idist(-127, 127);
    for (auto &q : Q) q = dist(rng);
    for (auto &v : V) v = dist(rng);
    for (size_t i = 0; i < Kq8.size(); ++i) {
        // half(1.0f) = 0x3C00
        Kq8[i].d = 0x3C00u;
        for (int j = 0; j < 32; ++j) Kq8[i].qs[j] = (int8_t)idist(rng);
    }

    auto t0 = high_resolution_clock::now();
    flash_attn_avx2(Q.data(), Kq8.data(), V.data(), O.data(), seqLen, headDim);
    auto t1 = high_resolution_clock::now();
    double ms_intrin = duration<double, std::milli>(t1 - t0).count();

    t0 = high_resolution_clock::now();
    flash_attn_puppeteer_avx2(Q.data(), Kq8.data(), V.data(), O.data(),
                              seqLen, headDim, /*Q8_0=*/2,
                              state.data(), pOut.data());
    t1 = high_resolution_clock::now();
    double ms_asm = duration<double, std::milli>(t1 - t0).count();

    double speedup = (ms_asm > 0.0) ? (ms_intrin / ms_asm) : 0.0;
    std::printf("Intrinsics: %.2f ms  Puppeteer-ASM: %.2f ms  Speedup: %.2fx\n",
           ms_intrin, ms_asm, speedup);
    if (speedup >= 1.2) std::puts("\xE2\x9C\x85 Puppeteer-ASM bonus: >= 1.2× over intrinsics");

    return 0;
}
