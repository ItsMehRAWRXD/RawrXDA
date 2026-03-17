#include "masm_kernels.h"
#include <iostream>
#include <vector>
#include <cmath>

int main() {
    auto info = masm::backend_info();
    std::cout << "MASM backend info: compiled_with_avx2=" << info.compiled_with_avx2
              << " cpu_avx2=" << info.cpu_avx2
              << " os_xsave_enabled=" << info.os_xsave_enabled << "\n";

    if (!info.usable()) {
        std::cout << "MASM backend not usable in this environment; skipping test\n";
        return 0; // skipped
    }

    size_t len = 64;
    std::vector<float> a(len), b(len);
    for (size_t i = 0; i < len; ++i) {
        a[i] = float(i) * 0.71f + 0.1f;
        b[i] = float(i) * -0.33f + 0.2f;
    }

    // Compute reference
    float ref = 0.0f;
    for (size_t i = 0; i < len; ++i) ref += a[i] * b[i];

    float got = 0.0f;
    try {
        got = dot_avx2(a.data(), b.data(), len);
    } catch (const std::exception& e) {
        std::cerr << "dot_avx2 threw exception: " << e.what() << "\n";
        return 3;
    }

    if (std::fabs(ref - got) > 1e-3f) {
        std::cerr << "dot_avx2 mismatch: ref=" << ref << " got=" << got << "\n";
        return 2;
    }

    std::cout << "dot_avx2 OK: " << got << "\n";
    return 0;
}
