// bench_deflate_brutal_speed.cpp — Compare Qt qCompress vs brutal stored-block memcpy gzip
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <random>
#include <chrono>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);

#if defined(HAVE_QT_CORE)
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <QtCore/QDebug>
#include <QtCore/QSysInfo>
using namespace std::chrono;
#endif

using clk = std::chrono::high_resolution_clock;

int main() {
    const size_t len = 1ull << 20; // 1 MB
    std::vector<unsigned char> src(len);
    std::mt19937 rng(42);
    for (size_t i = 0; i < len; ++i) src[i] = static_cast<unsigned char>(rng());

    double ms_qt = -1.0;

#if defined(HAVE_QT_CORE)
    {
        QByteArray in(reinterpret_cast<const char*>(src.data()), static_cast<int>(src.size()));
        auto t0 = clk::now();
        QByteArray comp = qCompress(in, 9);
        auto t1 = clk::now();
        ms_qt = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
#endif

    size_t out_len = 0;
    auto t0 = clk::now();
    void* out = deflate_brutal_masm(src.data(), src.size(), &out_len);
    auto t1 = clk::now();
    double ms_asm = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (out) std::free(out);

    if (ms_qt >= 0.0) {
        double speedup = ms_qt / ms_asm;
        std::printf("1 MB random: Qt %.2f ms, Brutal %.2f ms, speedup %.2fx\n", ms_qt, ms_asm, speedup);
    } else {
        std::printf("1 MB random: Brutal %.2f ms (Qt not found, speedup vs Qt not measured)\n", ms_asm);
    }
    return 0;
}
