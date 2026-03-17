#include <immintrin.h>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <cmath>

extern "C" float q4_0_dot_16_scalar(const uint8_t* w4_16, float scale, const float* x16);
extern "C" float q4_0_dot_16_avx2(const uint8_t* w4_16, float scale, const float* x16);

int main() {
    const int N = 64; // 64 x 64 block test
    const float scale = 0.125f;

    std::vector<uint8_t> q4((N * N) / 2); // 2 values per byte
    std::vector<float> x(N);

    // Deterministic data: x[i] = i*0.25; q4 nibbles cycle 0..15
    for (int i = 0; i < N; ++i) x[i] = 0.25f * i;
    for (int i = 0; i < (N * N) / 2; ++i) {
        uint8_t lo = (i * 3) & 0x0F;
        uint8_t hi = ((i * 5) + 1) & 0x0F;
        q4[i] = (hi << 4) | lo;
    }

    // Compute y = W * x, where W is 64x64 Q4_0; test a few rows using 16-element micro-kernel
    auto dot_row = [&](int row) {
        float sum = 0.0f;
        for (int col = 0; col < N; col += 16) {
            const uint8_t* wblk = &q4[(row * N + col) / 2]; // 16 vals -> 8 bytes
            sum += q4_0_dot_16_avx2(wblk, scale, &x[col]);
        }
        return sum;
    };

    auto dot_row_scalar = [&](int row) {
        float sum = 0.0f;
        for (int col = 0; col < N; col += 16) {
            const uint8_t* wblk = &q4[(row * N + col) / 2];
            sum += q4_0_dot_16_scalar(wblk, scale, &x[col]);
        }
        return sum;
    };

    bool ok = true;
    for (int r = 0; r < 4; ++r) {
        float a = dot_row(r);
        float b = dot_row_scalar(r);
        if (std::fabs(a - b) > 1e-4f) ok = false;
        std::printf("row %d: avx2=%.6f scalar=%.6f\n", r, a, b);
    }

    std::puts(ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
