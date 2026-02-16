// tests/benchmark_coldburst.cpp
#include <chrono>
#include "direct_io_ring.h"
#include "logging/logger.h"

extern "C" {
    void BurstExecute();
    bool Sovereign_InitiateBootstrap(const char* clusterId);
}

int main(int argc, char** argv) {
    Logger logger("BenchmarkColdburst");
    const char* model_path = (argc > 1) ? argv[1] : "test_ioring_loader.exe";
    logger.info("--- RAWRXD v1.1.x Quantum Burst Benchmark ---");

    // Phase: Sovereign Bootstrap
    logger.info("Initiating Ghost-C2 Handshake...");
    if (Sovereign_InitiateBootstrap("RAWRXD_ALPHA_PRIME")) {
        logger.info("Sovereign Identity Established.");
    } else {
        logger.warn("Handshake deferred (hardware entropy pending).");
    }
    
    DirectIOContext* ctx = nullptr;
    if (!DirectIO_Init(&ctx, model_path)) {
        logger.error("Failed to initialize DirectIO (File: {})", model_path);
        return 1;
    }
    logger.info("DirectIO Initialized");

    // Allocate 8GB for burst zones
    g_zoneBuffer = _aligned_malloc(8LL * 1024 * 1024 * 1024, 4096);
    if (!g_zoneBuffer) {
        logger.error("Failed to allocate burst buffer");
        return 1;
    }
    logger.info("Burst zones allocated (8GB)");

    auto t0 = std::chrono::high_resolution_clock::now();
    
    // Execute vectorized burst
    logger.info("Executing 4-lane vectorized burst...");
    BurstExecute();

    // Poll until all prefetches complete
    while (DirectIO_GetPendingCount(ctx) > 0) {
        DirectIO_Poll(ctx);
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    logger.info("Quantum Burst Cold-Load: {} us", dur);
    logger.info("Average per tensor (4 lanes): {} us", (double)dur / GetBurstCount());

    DirectIO_Shutdown(ctx);
    _aligned_free(g_zoneBuffer);
    
    return 0;
}
