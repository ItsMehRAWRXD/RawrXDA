#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <string>
#include "memory_oracle.h"
#include "hardware_telemetry_bridge.h"
#include "win32_hardware_provider.h"
#include "dxgi_hardware_provider.h"
#include "memory_morph_controller.h"
#include "memory_diagnostic_engine.h"

using namespace RawrXD::Memory;

int main(int argc, char** argv) {
    bool live = false;
    bool diag = false;
    for(int i=1; i<argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--live") live = true;
        if(arg == "--diag") diag = true;
    }

    if (diag) {
        std::cout << "[HTMMC] Initializing Memory Diagnostics (GoldMemory Mode)..." << std::endl;
        MemoryDiagnosticEngine engine;
        size_t testSize = 256LL * 1024 * 1024; // 256MB test buffer
        void* buffer = malloc(testSize);
        if (buffer) {
            auto report = engine.runTest(DiagnosticMode::NORMAL, buffer, testSize);
            std::cout << "  - Status: " << (report.passed ? "PASSED" : "FAILED") << std::endl;
            std::cout << "  - Throughput: " << report.throughputMBs << " MB/s" << std::endl;
            std::cout << "  - Errors: " << report.errorsFound << std::endl;
            free(buffer);
        }
        return 0;
    }

    MemoryOracle oracle;
    HardwareTelemetryBridge bridge;

    if(live) {
        std::cout << "[HTMMC] Initializing Hardware Providers..." << std::endl;
        bridge.addProvider(std::make_shared<Win32HardwareProvider>());
        bridge.addProvider(std::make_shared<DxgiHardwareProvider>());
        std::cout << "[HTMMC] Live Telemetry Enabled (PDH + DXGI)" << std::endl;
    }

    MemoryMorphController morph(oracle, bridge);

    std::cout << "[HTMMC] Starting 10s Morph Cycle..." << std::endl;
    auto start = std::chrono::steady_clock::now();
    while(std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
        morph.update();
        auto metrics = oracle.getMetricSnapshot();
        
        std::printf("\rPressure: %.2f | VRAM: %llu MB | RAM: %llu MB | W: [R:%d%% C:%d%% T:%d%%]          ", 
            metrics.pressure, 
            metrics.vram_used / (1024*1024), 
            metrics.ram_used / (1024*1024), 
            (int)(metrics.weights[0]*100), 
            (int)(metrics.weights[1]*100), 
            (int)(metrics.weights[2]*100));
        
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "\n[HTMMC] Benchmark Complete." << std::endl;
    return 0;
}
