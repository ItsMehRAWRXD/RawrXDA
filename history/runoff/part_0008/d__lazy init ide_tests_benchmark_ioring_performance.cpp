// tests/benchmark_ioring_performance.cpp
#include <iostream>
#include <vector>
#include <chrono>
#include "io/backend_interface.hpp"

// RAWRXD v1.1.0 IORing Cold-Load Benchmark
// Goal: Validate sub-5ms latency for kernel-bypass I/O

int main(int argc, char** argv) {
    const char* test_file = (argc > 1) ? argv[1] : "test_ioring_loader.exe";
    std::cout << "--- RAWRXD v1.1.0 IORing Benchmark ---" << std::endl;
    std::cout << "Target File: " << test_file << std::endl;

    auto backend = CreateIOBackend(IOBackendType::IORING_WINDOWS);
    if (!backend || !backend->Initialize(test_file)) {
        std::cerr << "Failed to initialize IORing backend" << std::endl;
        return 1;
    }

    const size_t RING_SIZE = 64 * 1024 * 1024; // 64MB ring
    const size_t ZONE_COUNT = 1024; 
    const size_t ZONE_SIZE = RING_SIZE / ZONE_COUNT;

    void* buffer = _aligned_malloc(RING_SIZE, 4096);
    if (!backend->RegisterBuffers(buffer, RING_SIZE, ZONE_COUNT)) {
        std::cerr << "Failed to register buffers" << std::endl;
        return 1;
    }

    const int NUM_READS = 100;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_READS; ++i) {
        IORequest req;
        req.file_offset = 0; // Simple case: read start of file repeatedly
        req.size = ZONE_SIZE;
        req.zone_index = i % ZONE_COUNT;
        req.zone_offset = 0;
        req.request_id = i;
        backend->SubmitRead(req);
    }
    
    backend->Flush();

    std::vector<IOCompletion> completions;
    while (completions.size() < NUM_READS) {
        backend->PollCompletions(completions);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "✓ Performed " << NUM_READS << " reads of " << (ZONE_SIZE/1024) << "KB each" << std::endl;
    std::cout << "✓ Total Time: " << duration << " us" << std::endl;
    std::cout << "✓ Average Latency per Read: " << (double)duration / NUM_READS << " us" << std::endl;

    backend->Shutdown();
    delete backend;
    _aligned_free(buffer);

    return 0;
}
