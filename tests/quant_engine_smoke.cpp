#include "../src/qtapp/quant_utils.hpp"
#include <random>
#include <iostream>
#include <vector>
#include <cmath>

static QByteArray make_raw_floats(const std::vector<float>& v) {
    QByteArray b;
    b.resize(v.size() * 4);
    std::memcpy(b.data(), v.data(), v.size() * 4);
    return b;
}

int main() {
    // Generate random floats
    std::mt19937 rng(2025);
    std::uniform_real_distribution<float> dist(-3.0f, 3.0f);
    const int n = 97; // odd length
    std::vector<float> vals(n);
    for (int i = 0; i < n; ++i) vals[i] = dist(rng);

    QByteArray raw = make_raw_floats(vals);

    // Q5
    QByteArray q5 = apply_quant(raw, "Q5_0");
    auto u5 = unpack_generic_bits(q5, 5);
    if ((int)u5.size() != n) { std::cerr << "Q5 unpack size mismatch\n"; return 1; }
    float maxErr5 = 0.f;
    for (int i = 0; i < n; ++i) maxErr5 = std::max(maxErr5, fabs(u5[i] - vals[i]));
    std::cout << "Q5 maxErr=" << maxErr5 << "\n";

    // Q6
    QByteArray q6 = apply_quant(raw, "Q6_K");
    auto u6 = unpack_generic_bits(q6, 6);
    if ((int)u6.size() != n) { std::cerr << "Q6 unpack size mismatch\n"; return 1; }
    float maxErr6 = 0.f;
    for (int i = 0; i < n; ++i) maxErr6 = std::max(maxErr6, fabs(u6[i] - vals[i]));
    std::cout << "Q6 maxErr=" << maxErr6 << "\n";

    // F16
    QByteArray f16 = apply_quant(raw, "F16");
    auto u16 = unpack_f16(f16);
    if ((int)u16.size() != n) { std::cerr << "F16 unpack size mismatch\n"; return 1; }
    float maxErr16 = 0.f;
    for (int i = 0; i < n; ++i) maxErr16 = std::max(maxErr16, fabs(u16[i] - vals[i]));
    std::cout << "F16 maxErr=" << maxErr16 << "\n";

    // Check error bounds: heuristics based on scale
    if (maxErr5 > 1.5f) {
        std::cerr << "Q5 error too high" << std::endl;
        return 2;
    }
    if (maxErr6 > 1.0f) {
        std::cerr << "Q6 error too high" << std::endl;
        return 3;
    }
    if (maxErr16 > 1e-3f * 3.0f) { // product dependant on test range
        std::cerr << "F16 error too high" << std::endl;
        return 4;
    }

    std::cout << "quant_engine_smoke: PASS" << std::endl;
    return 0;
}
