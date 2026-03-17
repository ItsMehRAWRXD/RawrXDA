// main_native.cpp
#include <windows.h>
#include <cstdio>
#include "native/gguf_native_loader.hpp"
#include "telemetry/async_logger.hpp"
#include "telemetry/metrics_server.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // Initialize native logging
    RawrXD::AsyncLogger::instance().start();
    RAWR_LOG_INFO("RawrXD Native v14.2.0 starting...");
    
    // Initialize heap
    HANDLE hHeap = GetProcessHeap();
    if (!hHeap) {
        RAWR_LOG_ERROR("Failed to get process heap");
        return 1;
    }
    
    // Test GGUF loading
    RawrXD::Native::NativeGGUFLoader loader;
    if (loader.load(L"models/test.gguf")) {
        RAWR_LOG_INFO("Loaded GGUF with %zu tensors", loader.tensorCount());
        
        // List tensors
        for (size_t i = 0; i < loader.tensorCount(); ++i) {
            const auto& t = loader.getTensor(i);
            RAWR_LOG_INFO("  [%zu] %s (%zu dims, %zu bytes)", 
                         i, t.name.view().data(), t.n_dims, t.size_bytes);
        }
    } else {
        RAWR_LOG_WARN("No model loaded, running in demo mode");
    }
    
    // Start metrics server on :9090
    RawrXD::MetricsServer metrics;
    metrics.start(9090);
    
    RAWR_LOG_INFO("RawrXD Native ready. Press Ctrl+C to exit.");
    
    // Main loop
    while (true) {
        Sleep(1000);
    }
    
    return 0;
}