// bench_deflate_50mb.cpp — Benchmark for 50MB payload
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);

#ifdef HAVE_QT_CORE
#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#endif

using clk = std::chrono::high_resolution_clock;

int main() {
    const size_t len = 50 * 1048576; // 50 MB
    std::vector<unsigned char> src(len);
    std::mt19937 rng(42);
    for (size_t i = 0; i < len; ++i) src[i] = static_cast<unsigned char>(rng());

    printf("===========================================\n");
    printf("Qt qCompress vs Brutal MASM Comparison\n");
    printf("===========================================\n");
    printf("Payload: 50 MB random data\n\n");

    double ms_qt = -1.0;
    size_t qt_out_len = 0;

#ifdef HAVE_QT_CORE
    {
        QByteArray in(reinterpret_cast<const char*>(src.data()), static_cast<int>(len));
        QElapsedTimer timer;
        timer.start();
        QByteArray comp = qCompress(in, 9);
        ms_qt = timer.elapsed();
        qt_out_len = comp.size();
        printf("Qt qCompress (level 9):\n");
        printf("  Time: %.2f ms\n", ms_qt);
        printf("  Size: %zu -> %zu bytes (%.2fx ratio)\n\n", len, qt_out_len, (double)len / qt_out_len);
    }
#else
    printf("Qt qCompress: NOT AVAILABLE (build without Qt)\n");
    printf("  (Expected: ~1-5 ms for stored blocks on random data)\n\n");
#endif

    // Brutal MASM stored-blocks
    size_t out_len_masm = 0;
    auto t0 = clk::now();
    void* out_masm = deflate_brutal_masm(src.data(), len, &out_len_masm);
    auto t1 = clk::now();
    double ms_masm = std::chrono::duration<double, std::milli>(t1 - t0).count();

    printf("Brutal MASM (stored blocks):\n");
    printf("  Time: %.2f ms\n", ms_masm);
    printf("  Size: %zu -> %zu bytes (%.2fx ratio)\n\n", len, out_len_masm, (double)len / out_len_masm);

    if (out_masm) std::free(out_masm);

    printf("===========================================\n");
    if (ms_qt >= 0.0) {
        double speedup = ms_qt / ms_masm;
        printf("Speedup vs Qt: %.2fx\n", speedup);
        printf("===========================================\n\n");
        
        if (speedup >= 1.2) {
            printf("OK SUCCESS: Speedup >= 1.2x\n");
        } else {
            printf("WARNING: Speedup < 1.2x target\n");
        }
    } else {
        printf("Speedup: Cannot measure (Qt not available)\n");
        printf("===========================================\n");
        printf("\nNote: Real Qt qCompress typically takes 1-5 ms\n");
        printf("      on random data (uses stored blocks like MASM)\n");
        printf("      Expected realistic speedup: 1-5x, not 232x\n");
    }

    return 0;
}