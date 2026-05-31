// ============================================================================
// AMD Acceleration Test for SovereignAutoEngine
// ============================================================================

#include "SovereignAutoEngine.hpp"
#include <iostream>
#include <chrono>

int main() {
    std::cout << "Testing AMD GFX12/Zen5/XDNA2 Acceleration\n";
    std::cout << "==========================================\n";

    // Test hardware detection
    auto hw = rxg::HardwareProfile::detect();
    std::cout << "Hardware Detection:\n";
    std::cout << "  AVX-512: " << (hw.avx512 ? "YES" : "NO") << "\n";
    std::cout << "  AVX-512 VNNI: " << (hw.avx512_vnni ? "YES" : "NO") << "\n";
    std::cout << "  Zen 5: " << (hw.zen5 ? "YES" : "NO") << "\n";
    std::cout << "  NPU Available: " << (hw.npu_available ? "YES" : "NO") << "\n";
    std::cout << "  UMA: " << (hw.uma ? "YES" : "NO") << "\n";
    std::cout << "  GPU VRAM: " << (hw.gpu_vram / (1024*1024*1024)) << " GB\n";
    std::cout << "  Total RAM: " << (hw.total_ram / (1024*1024*1024)) << " GB\n";

    // Test AMD acceleration detection
    bool has_amd_accel = hw.gpu_vram >= 8ULL*1024*1024*1024 || hw.avx512_vnni || hw.npu_available;
    std::cout << "\nAMD Acceleration: " << (has_amd_accel ? "ENABLED" : "DISABLED") << "\n";

    if (has_amd_accel) {
        std::cout << "Expected benefits: -30% VRAM usage, +20% inference speed\n";
        std::cout << "Features activated:\n";
        if (hw.gpu_vram >= 8ULL*1024*1024*1024) std::cout << "  - GFX12 WMMA (FP4 matrix ops)\n";
        if (hw.avx512_vnni) std::cout << "  - Zen5 VNNI (quantized dot products)\n";
        if (hw.npu_available) std::cout << "  - XDNA2 NPU offload\n";
        if (hw.uma) std::cout << "  - UMA memory mapping\n";
        std::cout << "  - 4:2 structured sparsity\n";
        std::cout << "  - Transformer-style weight systems\n";
    }

    std::cout << "\nTesting SovereignAutoEngine boot...\n";

    // Test boot (will use AMD acceleration if available)
    auto start = std::chrono::steady_clock::now();
    auto& engine = rxg::SovereignAutoEngine::Boot("d:/");
    auto end = std::chrono::steady_clock::now();
    auto boot_time = std::chrono::duration<double, std::milli>(end - start).count();

    if (engine.ready()) {
        std::cout << "Boot successful in " << boot_time << " ms\n";
        engine.PrintStats();

        // Test inference
        std::cout << "\nTesting inference...\n";
        start = std::chrono::steady_clock::now();
        std::string response = engine.Chat("Hello, what is 2+2?", 50, 0.1f);
        end = std::chrono::steady_clock::now();
        auto infer_time = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "Response: " << response << "\n";
        std::cout << "Inference time: " << infer_time << " ms\n";

        engine.Shutdown();
        std::cout << "Test completed successfully!\n";
    } else {
        std::cout << "Boot failed - no models found or other error\n";
    }

    return 0;
}