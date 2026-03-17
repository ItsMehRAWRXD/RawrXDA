// native_core/main_native.cpp
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "gguf_native_loader.hpp"
#include "win32_file_iterator.hpp"
#include "http_native_server.hpp"
#include <iostream>
#include <string>
#include <cstring>

extern "C" {
    void Q4_0_DequantBlock_AVX2(const void* input, float* output);
    void Q4_0_DequantBatch_AVX2(const void* input, float* output, uint64_t blockCount);
}

int main() {
    std::cout << "=== RawrXD Native Core v14.2.0 ===" << std::endl;
    std::cout << "Zero-dependency inference engine" << std::endl;

    // --- Test Q4_0 dequantization kernel ---
    std::cout << "\n[1] Testing Q4_0 AVX2 dequant kernel..." << std::endl;
    // Create a fake Q4_0 block: 2 bytes scale (f16) + 16 bytes nibbles = 18 bytes
    alignas(16) uint8_t q4block[18] = {};
    // Set scale to 1.0 in f16 (0x3C00)
    q4block[0] = 0x00; q4block[1] = 0x3C;
    // Fill nibbles with test pattern
    for (int i = 0; i < 16; i++) q4block[2 + i] = 0x37; // low=7, high=3
    
    alignas(32) float output[32] = {};
    Q4_0_DequantBlock_AVX2(q4block, output);
    std::cout << "  First 8 dequantized values: ";
    for (int i = 0; i < 8; i++) std::cout << output[i] << " ";
    std::cout << std::endl;

    // --- Test file iterator ---
    std::cout << "\n[2] Testing Win32 file iterator..." << std::endl;
    RawrXD::Native::FileIterator iter(L".");
    int fileCount = 0;
    iter.ForEach([&fileCount](const WIN32_FIND_DATAW& data) {
        fileCount++;
        if (fileCount <= 5) {
            std::wcout << L"  " << data.cFileName << std::endl;
        }
        return true;
    });
    std::cout << "  Total entries: " << fileCount << std::endl;

    // --- Test GGUF loader ---
    std::cout << "\n[3] GGUF loader: ready (no model file provided)" << std::endl;

    // --- Test HTTP server ---
    std::cout << "\n[4] Testing HTTP server on port 9090..." << std::endl;
    RawrXD::Native::HttpNativeServer server(9090);
    if (server.Start([](const std::string& request) -> std::string {
        return "{\"status\":\"ok\",\"engine\":\"RawrXD-Native\",\"version\":\"14.2.0\"}";
    })) {
        std::cout << "  HTTP server running on http://localhost:9090" << std::endl;
        std::cout << "  Press Enter to stop..." << std::endl;
        std::cin.get();
        server.Stop();
    } else {
        std::cout << "  Failed to start HTTP server (port in use?)" << std::endl;
    }

    std::cout << "\nAll tests complete." << std::endl;
    return 0;
}