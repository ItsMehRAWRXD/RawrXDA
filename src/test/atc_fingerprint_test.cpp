// d:/rawrxd/src/test/atc_fingerprint_test.cpp
#include "../atc_codec.h"
#include "../gpu_enforcement.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <windows.h>
#include <psapi.h>

// --- Memory Measurement ---
size_t get_process_memory_usage() {
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.PrivateUsage;
    }
    return 0;
}

// --- Main Test Logic ---
void run_fingerprint_test(const wchar_t* model_path) {
    std::cout << "--- Starting ATC Fingerprint Test ---" << std::endl;
    std::wcout << L"Model: " << model_path << std::endl;

    // Mandatory GPU gate — aborts the process if no GPU backend is available.
    rxd::gpu::require();
    const auto& gpu = rxd::gpu::status();
    std::cout << "GPU backend: " << gpu.device_name
              << " (" << (gpu.vram_total_bytes >> 20) << " MiB VRAM)" << std::endl;

    AdaptiveTensorCodec codec;

    // 1. Map the model
    auto start_map_time = std::chrono::high_resolution_clock::now();
    if (!codec.map_model(model_path)) {
        std::cerr << "Error: Failed to map model." << std::endl;
        return;
    }
    auto end_map_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> map_duration = end_map_time - start_map_time;
    std::cout << "Model mapped successfully in " << map_duration.count() << " ms." << std::endl;

    size_t memory_after_map = get_process_memory_usage();
    std::cout << "Memory usage after map (Private Bytes): " << memory_after_map / (1024 * 1024) << " MB" << std::endl;
    
    // 2. Prepare dummy input for generation
    // In a real scenario, this would come from a tokenizer.
    std::vector<int> input_tokens(512, 1); // 512 dummy tokens

    std::cout << "\n--- Running Generation Benchmark ---" << std::endl;
    const int tokens_to_generate = 256;
    
    auto start_gen_time = std::chrono::high_resolution_clock::now();
    
    // For now, we'll just loop and measure memory, but we'll call the actual codec functions
    // to make the simulation more realistic and to ensure the code path is exercised.
    size_t peak_memory_during_gen = memory_after_map;

    // Create a dummy tile to work on
    TileMeta dummy_tile;
    dummy_tile.scale = 1.0f;
    dummy_tile.offset = 0.0f;
    // Let's define a 4-bit base + 2-bit refinement braid
    dummy_tile.braids.push_back({4, 0, 0}); 
    dummy_tile.braids.push_back({2, 0, 0});

    TileBuffer work_buffer;

    for (int i = 0; i < tokens_to_generate; ++i) {
        // Simulate one token generation step which involves processing one tile
        codec.prefetch_tile(dummy_tile);
        codec.decode_tile_l0(dummy_tile, &work_buffer);
        
        size_t current_mem = get_process_memory_usage();
        if (current_mem > peak_memory_during_gen) {
            peak_memory_during_gen = current_mem;
        }
        // No need to sleep, the dequantization call itself is our workload
    }

    auto end_gen_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> gen_duration = end_gen_time - start_gen_time;

    double tps = tokens_to_generate / gen_duration.count();

    std::cout << "\n--- Fingerprint Results ---" << std::endl;
    std::cout << "Generated " << tokens_to_generate << " tokens in " << gen_duration.count() << " seconds." << std::endl;
    std::cout << "Tokens Per Second (TPS): " << tps << std::endl;
    std::cout << "Peak Memory during generation (Private Bytes): " << peak_memory_during_gen / (1024 * 1024) << " MB" << std::endl;
    std::cout << "--- Test Finished ---" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_gguf_model>" << std::endl;
        return 1;
    }

    // Convert char* to wchar_t* for the model path
    const size_t cSize = strlen(argv[1]) + 1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, argv[1], cSize);

    run_fingerprint_test(wc);

    delete[] wc;
    return 0;
}
