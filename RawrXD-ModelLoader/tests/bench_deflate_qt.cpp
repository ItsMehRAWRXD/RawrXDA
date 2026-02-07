// bench_deflate_qt.cpp
#include <QCoreApplication>
#include <QJsonDocument>
#include <QByteArray>
#include <QElapsedTimer>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len);
extern "C" void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len);

// Stubs for cross-compilation compatibility
#if defined(_M_AMD64) || defined(__x86_64__)
    // On x64, stub NEON
    extern "C" void* deflate_brutal_neon(const void* src, size_t len, size_t* out_len) {
        (void)src; (void)len; (void)out_len;
        return nullptr;
    }
#elif defined(_M_ARM64) || defined(__aarch64__)
    // On ARM64, stub MASM
    extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
        (void)src; (void)len; (void)out_len;
        return nullptr;
    }
#endif

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const size_t len = 1048576; // 1 MB
    QByteArray src(len, Qt::Uninitialized);
    std::srand(42);
    std::generate(src.begin(), src.end(), []() { return static_cast<char>(std::rand()); });

    QElapsedTimer timer;

    // Qt qCompress
    timer.start();
    QByteArray compressed_qt = qCompress(src);
    qint64 qt_time = timer.elapsed();
    std::cout << "Qt qCompress: " << qt_time << " ms" << std::endl;

    // Brutal MASM
    #if defined(_M_AMD64) || defined(__x86_64__)
    timer.restart();
    size_t out_len_masm = 0;
    void* compressed_masm = deflate_brutal_masm(src.constData(), len, &out_len_masm);
    qint64 masm_time = timer.elapsed();
    std::cout << "Brutal MASM: " << masm_time << " ms" << std::endl;
    free(compressed_masm);
    #else
    std::cout << "Brutal MASM: N/A" << std::endl;
    #endif

    // Brutal NEON
    #if defined(_M_ARM64) || defined(__aarch64__)
    timer.restart();
    size_t out_len_neon = 0;
    void* compressed_neon = deflate_brutal_neon(src.constData(), len, &out_len_neon);
    qint64 neon_time = timer.elapsed();
    std::cout << "Brutal NEON: " << neon_time << " ms" << std::endl;
    free(compressed_neon);
    #else
    // Simulate output for the user's expected format if they really want to see it, 
    // but technically we can't run it. 
    // I'll just print N/A to be honest.
    std::cout << "Brutal NEON: N/A" << std::endl;
    #endif

    return 0;
}
