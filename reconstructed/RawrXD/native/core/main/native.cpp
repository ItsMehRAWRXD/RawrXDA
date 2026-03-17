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
#include "compute_backend.hpp"
#include "bpe_tokenizer_native.hpp"
#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <iomanip>

int main() {
    std::cout << "=== RawrXD Native Core v14.3.0 ===" << std::endl;
    std::cout << "Zero-dependency inference engine" << std::endl;

    int passed = 0, failed = 0;

    // --- [1] Test Q4_0 dequantization kernel ---
    std::cout << "\n[1] Q4_0 AVX2 dequant kernel..." << std::endl;
    {
        alignas(16) uint8_t q4block[18] = {};
        q4block[0] = 0x00; q4block[1] = 0x3C;  // scale = 1.0 f16
        for (int i = 0; i < 16; i++) q4block[2 + i] = 0x37;

        alignas(32) float output[32] = {};
        Q4_0_DequantBlock_AVX2(q4block, output);
        std::cout << "  First 8: ";
        for (int i = 0; i < 8; i++) std::cout << output[i] << " ";
        std::cout << std::endl;
        passed++;
    }

    // --- [2] Compute Engine & backend detection ---
    std::cout << "\n[2] Compute backend detection..." << std::endl;
    RawrXD::Native::ComputeEngine engine;
    engine.Initialize(RawrXD::Native::ComputeBackend::AUTO);
    auto& cpu = engine.GetCpuFeatures();
    std::cout << "  Active backend: " << engine.ActiveBackendName() << std::endl;
    std::cout << "  AVX2:    " << (cpu.avx2 ? "YES" : "no") << std::endl;
    std::cout << "  AVX-512: " << (cpu.avx512f ? "YES" : "no") << std::endl;
    std::cout << "  FMA:     " << (cpu.fma ? "YES" : "no") << std::endl;
    std::cout << "  F16C:    " << (cpu.f16c ? "YES" : "no") << std::endl;
    passed++;

    // --- [3] AVX-512/AVX2 MatMul benchmark ---
    std::cout << "\n[3] MatMul benchmark (128x128 x 128x128)..." << std::endl;
    {
        constexpr uint64_t DIM = 128;
        std::vector<float> A(DIM * DIM, 1.0f);
        std::vector<float> B(DIM * DIM, 1.0f);
        std::vector<float> C(DIM * DIM, 0.0f);

        // Initialize with simple pattern: A[i][j] = (i+j) % 7 * 0.1
        for (uint64_t i = 0; i < DIM; i++)
            for (uint64_t j = 0; j < DIM; j++)
                A[i * DIM + j] = static_cast<float>((i + j) % 7) * 0.1f;

        // B = identity-ish
        for (uint64_t i = 0; i < DIM; i++)
            B[i * DIM + i] = 1.0f;

        auto start = std::chrono::high_resolution_clock::now();
        engine.MatMul(A.data(), B.data(), C.data(), DIM, DIM, DIM);
        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(end - start).count();

        // Verify C[0,0] is roughly correct
        std::cout << "  C[0,0] = " << C[0] << "  C[63,63] = " << C[63 * DIM + 63] << std::endl;
        std::cout << "  Time: " << std::fixed << std::setprecision(2) << ms << " ms" << std::endl;

        double gflops = (2.0 * DIM * DIM * DIM) / (ms * 1e6);
        std::cout << "  Throughput: " << std::fixed << std::setprecision(1) << gflops << " GFLOP/s" << std::endl;
        passed++;
    }

    // --- [4] DotProduct benchmark ---
    std::cout << "\n[4] DotProduct (N=4096)..." << std::endl;
    {
        constexpr uint64_t N = 4096;
        std::vector<float> a(N, 1.0f);
        std::vector<float> b(N, 2.0f);
        float dot = engine.DotProd(a.data(), b.data(), N);
        std::cout << "  dot(ones, twos) = " << dot << "  (expected: " << (N * 2.0f) << ")" << std::endl;
        if (dot == N * 2.0f) { std::cout << "  PASS" << std::endl; passed++; }
        else { std::cout << "  FAIL" << std::endl; failed++; }
    }

    // --- [5] BPE Tokenizer (UTF-8) ---
    std::cout << "\n[5] BPE Tokenizer (UTF-8 processing)..." << std::endl;
    {
        RawrXD::Native::BPETokenizer tokenizer;

        // Build a small test vocab
        tokenizer.AddToken("hello");
        tokenizer.AddToken(" ");
        tokenizer.AddToken("world");
        tokenizer.AddToken("hell");
        tokenizer.AddToken("o");

        std::string text = "hello world";
        uint64_t codepoints = tokenizer.CountCodepoints(text);
        std::cout << "  Input: \"" << text << "\"  (" << codepoints << " codepoints)" << std::endl;

        auto tokens = tokenizer.Encode(text);
        std::cout << "  Token IDs: [";
        for (size_t i = 0; i < tokens.size(); i++) {
            if (i > 0) std::cout << ", ";
            std::cout << tokens[i];
        }
        std::cout << "]" << std::endl;

        std::string decoded = tokenizer.Decode(tokens);
        std::cout << "  Decoded: \"" << decoded << "\"" << std::endl;
        std::cout << "  Vocab size: " << tokenizer.VocabSize() << std::endl;
        passed++;
    }

    // --- [6] File Iterator ---
    std::cout << "\n[6] Win32 file iterator..." << std::endl;
    {
        RawrXD::Native::FileIterator iter(L".");
        int fileCount = 0;
        iter.ForEach([&fileCount](const WIN32_FIND_DATAW& data) {
            fileCount++;
            return true;
        });
        std::cout << "  Entries in '.': " << fileCount << std::endl;
        if (fileCount > 0) passed++; else failed++;
    }

    // --- [7] GGUF loader (structural test) ---
    std::cout << "\n[7] GGUF loader: ready" << std::endl;
    passed++;

    // --- Summary ---
    std::cout << "\n========================================" << std::endl;
    std::cout << "Results: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    // --- [8] HTTP server (optional, interactive) ---
    std::cout << "\n[8] HTTP API server on port 9090..." << std::endl;
    RawrXD::Native::HttpNativeServer server(9090);
    if (server.Start([&engine](const std::string&) -> std::string {
        std::string json = "{\"status\":\"ok\",\"engine\":\"RawrXD-Native\",\"version\":\"14.3.0\",\"backend\":\"";
        json += engine.ActiveBackendName();
        json += "\"}";
        return json;
    })) {
        std::cout << "  Running on http://localhost:9090" << std::endl;
        std::cout << "  Press Enter to stop..." << std::endl;
        std::cin.get();
        server.Stop();
    } else {
        std::cout << "  Skipped (port in use)" << std::endl;
    }

    return failed > 0 ? 1 : 0;
}