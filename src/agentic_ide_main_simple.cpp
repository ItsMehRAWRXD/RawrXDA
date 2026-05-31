// RawrXD Agentic IDE - Minimal Main
// Entry point for GUI application
// VSU Effects: Uses Adobe RGBa color space for professional color accuracy

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <fstream>
#include "logger.h"
#include "RawrXD_Window.h"
#include "RawrXD_Foundation.h"
#include "universal_model_router.h"
#include "include/RawrXD_ColorSpace.h"
#include "streaming/AdaptiveTensorCodec.hpp"
#include "streaming/atc_benchmark.hpp"
#include "quantization/braided_quantizer_fingerprint.hpp"
#include <iostream>

using namespace RawrXD;
using namespace RawrXD::ColorSpace;

// Helper to convert AdobeRGBa to COLORREF for Win32 GDI
inline COLORREF AdobeRGBaToCOLORREF(const AdobeRGBa& color) {
    auto srgb = color.TosRGB();
    return RGB(static_cast<int>(srgb.r * 255), 
               static_cast<int>(srgb.g * 255), 
               static_cast<int>(srgb.b * 255));
}

class IDEWindow : public Window {
    UniversalModelRouter router;
    rawrxd::AdaptiveTensorCodec atc; // Add the Adaptive Tensor Codec
    String status = "Ready. Press F5 to run Titan+Assembly inference.";
    String outputText;
    
public:
    IDEWindow() {
        // Initialize logic - assume model path is auto-detected or configured
        // router.initializeLocalEngine(""); 

        // Attempt to open a model. Replace with your actual model path.
        // This should be a large GGUF model to test the streaming.
        if (atc.OpenModel(L"d:\\codestral22b.gguf")) {
            status = "ATC Model Loaded. Ready for streaming inference.";
        } else {
            status = "Error: Could not load model with ATC.";
        }
    }
    
    void paintEvent(PAINTSTRUCT& ps) override {
        HDC hdc = ps.hdc;
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        // VSU Acrylic background
        HBRUSH bgBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(VSU::Acrylic::DarkBase));
        FillRect(hdc, &rect, bgBrush);
        DeleteObject(bgBrush);
        
        SetBkMode(hdc, TRANSPARENT);
        
        // Draw Status with VSU text color
        SetTextColor(hdc, AdobeRGBaToCOLORREF(VSU::Accents::Blue));
        RECT rParam = {10, 10, rect.right, 40};
        DrawTextW(hdc, status.c_str(), -1, &rParam, DT_LEFT);
        
        // Draw Output with VSU text color
        SetTextColor(hdc, AdobeRGBaToCOLORREF(AdobeRGBa(0.86f, 0.86f, 0.86f, 1.00f)));
        RECT rOut = {10, 50, rect.right, rect.bottom};
        DrawTextW(hdc, outputText.c_str(), -1, &rOut, DT_LEFT | DT_WORDBREAK);
    }
    
    void keyPressEvent(int key, int mods) override {
        if (key == VK_F5) {
            status = "Running Inference via ATC Streaming...";
            update(); // Repaint
             
            // This is a conceptual test. A real implementation would involve
            // a compute loop that uses the ATC to get tensor data tile by tile.
            
            // 1. Prefetch the first tensor we'll need (e.g., "token_embd.weight")
            atc.PrefetchTensor("token_embd.weight");

            // 2. Get a pointer to the low-fidelity ("360p") tile data.
            //    Accessing this pointer will be fast because of the prefetch.
            const void* tile_data = atc.GetTileData("token_embd.weight", 0, rawrxd::TensorLOD::L0_Q4);

            if (tile_data) {
                // 3. In a real engine, you would now pass this tile_data to an AVX-512 kernel.
                //    For this test, we'll just confirm we got a valid pointer.
                char buffer[256];
                sprintf_s(buffer, "Successfully got tile pointer: %p. Ready for compute.", tile_data);
                outputText = String(buffer);

                // 4. Once done with the tensor, we can signal to the OS it can be discarded.
                atc.DiscardTensor("token_embd.weight");
                status = "Streaming Inference Step Complete.";

            } else {
                outputText = "Failed to get tile data via ATC.";
                status = "Streaming Inference Error.";
            }
            
            update();
        }
        else if (key == VK_F6) { // New key for running the benchmark
            status = "Running ATC Benchmark...";
            update();

            ATCBenchmark benchmark;
            // The reference output should be the predictable result from our simulated inference
            std::string reference_output;
            for (int i = 0; i < 100; ++i) {
                reference_output += static_cast<char>(('T' + i) % 256);
            }

            ATCBenchmarkResult result = benchmark.run(L"d:\\codestral22b.gguf", "Test", reference_output);

            if (result.success) {
                char buffer[512];
                sprintf_s(buffer, "Benchmark Complete.\nTTFT: %.2f ms\nTPS: %.2f\nPeak Memory: %zu MB\nFingerprint Match: %s",
                    result.time_to_to_first_token_ms,
                    result.tokens_per_second,
                    result.peak_working_set_mb,
                    result.fingerprint_match ? "Yes" : "No");
                outputText = String(buffer);
            } else {
                outputText = "Benchmark Failed: " + String(result.error_message);
            }
            status = "Benchmark Finished.";
            update();
        }
        else if (key == VK_F7) { // New key for Braided Quantizer Fingerprint
            status = "Running Braided Quantizer Fingerprint...";
            update();

            BraidedQuantizerFingerprintResult result = BraidedQuantizerFingerprint::run_test();
            
            outputText = String(result.message);
            if (result.passed) {
                status = "Fingerprint Passed.";
            } else {
                status = "Fingerprint FAILED.";
            }
            update();
        }
        else if (key == VK_ESCAPE) {
            PostQuitMessage(0);
        }
    }
};

// For GUI apps, we need WinMain, not main
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Convert Windows command line to argc/argv for Qt removal compatibility
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    LocalFree(argvW);

    // Create minimal main window
    IDEWindow window;
    window.create(nullptr, "RawrXD Agentic IDE Titan Powered", WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    window.resize(1024, 768);

    // Message Loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}


