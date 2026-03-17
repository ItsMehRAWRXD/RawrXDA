// tests/benchmark_coldburst.cpp
#include <iostream>
#include <chrono>
#include "direct_io_ring.h"

extern "C" void BurstExecute();

int main(int argc, char** argv) {
    const char* model_path = (argc > 1) ? argv[1] : "test_ioring_loader.exe";
    std::cout << "--- RAWRXD v1.1.x Quantum Burst Benchmark ---" << std::endl;
    
    DirectIOContext* ctx = nullptr;
    if (!DirectIO_Init(&ctx, model_path)) {
        std::cerr << "Failed to initialize DirectIO" << std::endl;
        return 1;
    }

    // Allocate 8GB for burst zones
    g_zoneBuffer = _aligned_malloc(8LL * 1024 * 1024 * 1024, 4096);
    if (!g_zoneBuffer) {
        std::cerr << "Failed to allocate burst buffer" << std::endl;
        return 1;
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Execute vectorized burst
    std::cout << "Executing 4-lane vectorized burst..." << std::endl;
    BurstExecute();

    // Poll until all prefetches complete
    while (ctx->pending > 0) {
        DirectIO_Poll(ctx);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    std::cout << "✓ Quantum Burst Cold-Load: " << dur << " us" << std::endl;
    std::cout << "✓ Average per tensor (4 lanes): " << (double)dur / GetBurstCount() << " us" << std::endl;

    DirectIO_Shutdown(ctx);
    _aligned_free(g_zoneBuffer);
    
    return 0;
}
