#include <QtCore/QCoreApplication>
#include <QtCore/QByteArray>
#include <QtCore/QElapsedTimer>
#include <iostream>
#include <cstdlib>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);
extern "C" void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len);

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const size_t len = 1048576; // 1 MB
    QByteArray src;
    src.resize(static_cast<int>(len));
    for (int i = 0; i < src.size(); ++i) src[i] = static_cast<char>(std::rand());

    QElapsedTimer timer;

    // Qt qCompress
    timer.start();
    QByteArray compressed_qt = qCompress(src);
    qint64 qt_time = timer.elapsed();
    std::cout << "Qt qCompress: " << qt_time << " ms" << std::endl;

    // Brutal MASM (only if available on this build)
    timer.restart();
    size_t out_len_masm = 0;
    void* compressed_masm = nullptr;
#if defined(_M_X64) || defined(__x86_64__)
    compressed_masm = deflate_brutal_masm(src.constData(), len, &out_len_masm);
#endif
    qint64 masm_time = timer.elapsed();
    if (compressed_masm) {
        std::cout << "Brutal MASM: " << masm_time << " ms" << std::endl;
        std::free(compressed_masm);
    } else {
        std::cout << "Brutal MASM: N/A (not built on this arch)" << std::endl;
    }

    // Brutal NEON (only if available on this build)
    timer.restart();
    size_t out_len_neon = 0;
    void* compressed_neon = nullptr;
#if defined(__aarch64__) || defined(_M_ARM64)
    compressed_neon = deflate_brutal_neon(src.constData(), len, &out_len_neon);
#endif
    qint64 neon_time = timer.elapsed();
    if (compressed_neon) {
        std::cout << "Brutal NEON: " << neon_time << " ms" << std::endl;
        std::free(compressed_neon);
    } else {
        std::cout << "Brutal NEON: N/A (not built on this arch)" << std::endl;
    }

    return 0;
}
// bench_deflate_qt.cpp — Real Qt qCompress vs brutal stored-block comparison
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
    const size_t len = 1048576; // 1 MB
    std::vector<unsigned char> src(len);
    std::mt19937 rng(42);
    for (size_t i = 0; i < len; ++i) src[i] = static_cast<unsigned char>(rng());

    printf("Benchmarking 1 MB random data...\n\n");

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
        printf("  Size: %zu → %zu bytes (%.2fx ratio)\n\n", len, qt_out_len, (double)len / qt_out_len);
    }
#else
    printf("Qt qCompress: NOT AVAILABLE (HAVE_QT_CORE not defined)\n\n");
#endif

    // Brutal MASM stored-blocks
    size_t out_len_masm = 0;
    auto t0 = clk::now();
    void* out_masm = deflate_brutal_masm(src.data(), len, &out_len_masm);
    auto t1 = clk::now();
    double ms_masm = std::chrono::duration<double, std::milli>(t1 - t0).count();

    printf("Brutal MASM (stored blocks):\n");
    printf("  Time: %.2f ms\n", ms_masm);
    printf("  Size: %zu → %zu bytes (%.2fx ratio)\n\n", len, out_len_masm, (double)len / out_len_masm);

    if (out_masm) std::free(out_masm);

    if (ms_qt >= 0.0) {
        double speedup = ms_qt / ms_masm;
        printf("===========================================\n");
        printf("Speedup vs Qt: %.2fx\n", speedup);
        printf("===========================================\n\n");
        
        if (speedup >= 1.2) {
            printf("✓ SUCCESS: Speedup >= 1.2x\n");
        } else {
            printf("⚠ WARNING: Speedup < 1.2x target\n");
        }
    } else {
        printf("Cannot calculate speedup (Qt not available)\n");
    }

    return 0;
}
