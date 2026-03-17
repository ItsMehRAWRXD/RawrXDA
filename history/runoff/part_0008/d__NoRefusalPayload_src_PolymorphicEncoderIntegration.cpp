/**
 * @file PolymorphicEncoderIntegration.cpp
 * @brief Demonstrates integration of RawrXD Polymorphic Encoder with NoRefusal Payload Supervisor
 */

#include "PayloadSupervisor.hpp"
#include "RawrXD_PolymorphicEncoder.hpp"
#include "x64_codegen.hpp"
#include <windows.h>
#include <iostream>
#include <vector>
#include <chrono>

using namespace std;
using namespace RawrXD;

/**
 * @brief Demonstrate basic encoder operations
 */
void DemoBasicEncoding() {
    cout << "\n[*] DEMO 1: Basic Encoding Operations\n";
    cout << "=====================================\n\n";

    // Sample plaintext
    const char* plaintext = "Hello, RawrXD Polymorphic Encoder!";
    size_t size = strlen(plaintext) + 1;

    // Create output buffer
    vector<uint8_t> ciphertext(size);

    // Initialize encoder
    PolymorphicEncoder encoder;
    encoder.SetAlgorithm(ENC_XOR_CHAIN);
    encoder.SetKey("TestKey123", 10);

    // Encode
    int64_t result = encoder.Encode(plaintext, ciphertext.data(), size);
    cout << "[+] Encoded " << size << " bytes (result: " << result << ")\n";

    // Print hex dump
    cout << "[+] Ciphertext (hex): ";
    for (size_t i = 0; i < size; ++i) {
        printf("%02X ", ciphertext[i]);
        if ((i + 1) % 16 == 0) cout << "\n                      ";
    }
    cout << "\n\n";

    // Decode (symmetric)
    vector<uint8_t> recovered(size);
    result = encoder.Decode(ciphertext.data(), recovered.data(), size);
    cout << "[+] Decoded (result: " << result << ")\n";
    cout << "[+] Recovered text: " << (char*)recovered.data() << "\n";

    // Verify
    if (memcmp(plaintext, recovered.data(), size) == 0) {
        cout << "[✓] Encoding/Decoding verified!\n";
    } else {
        cout << "[✗] Verification failed!\n";
    }
}

/**
 * @brief Demonstrate polymorphic encoding
 */
void DemoPolymorphicEncoding() {
    cout << "\n[*] DEMO 2: Polymorphic Encoding\n";
    cout << "=================================\n\n";

    // Generate multiple keys for demonstration
    vector<uint8_t> key1(32), key2(32);
    for (int i = 0; i < 32; ++i) {
        key1[i] = static_cast<uint8_t>(i ^ 0xAA);
        key2[i] = static_cast<uint8_t>(i ^ 0x55);
    }

    // Test payload
    vector<uint8_t> payload(256);
    for (size_t i = 0; i < payload.size(); ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Encode with first key
    PolymorphicEncoder encoder1;
    encoder1.SetAlgorithm(ENC_ROLLING_XOR);
    encoder1.SetKey(key1.data(), key1.size());

    vector<uint8_t> encoded1(payload.size());
    encoder1.Encode(payload.data(), encoded1.data(), payload.size());
    cout << "[+] Encoded with ROLLING_XOR\n";

    // Encode with second key (shows polymorphic variation)
    PolymorphicEncoder encoder2;
    encoder2.SetAlgorithm(ENC_RC4_STREAM);
    encoder2.SetKey(key2.data(), key2.size());

    vector<uint8_t> encoded2(payload.size());
    encoder2.Encode(payload.data(), encoded2.data(), payload.size());
    cout << "[+] Encoded with RC4_STREAM\n";

    // Compare results (should be different)
    bool identical = (encoded1 == encoded2);
    cout << "[+] Results identical: " << (identical ? "YES" : "NO (expected)\n");

    // Show first 16 bytes of each
    cout << "\n[+] ROLLING_XOR output (first 16 bytes): ";
    for (int i = 0; i < 16; ++i) printf("%02X ", encoded1[i]);
    cout << "\n";

    cout << "[+] RC4_STREAM output (first 16 bytes):  ";
    for (int i = 0; i < 16; ++i) printf("%02X ", encoded2[i]);
    cout << "\n";
}

/**
 * @brief Demonstrate code generation and PE creation
 */
void DemoCodeGeneration() {
    cout << "\n[*] DEMO 3: x64 Code Generation\n";
    cout << "================================\n\n";

    x64CodeGenerator gen;

    // Generate simple exit code: mov rax, 60; xor rdi, rdi; syscall
    gen.Mov_R64_Imm64(0, 60);           // rax = 60 (sys_exit)
    gen.Xor_R64_R64(7, 7);              // rdi = 0 (status)
    gen.Syscall();
    gen.Ret();

    cout << "[+] Generated " << gen.GetSize() << " bytes of x64 machine code\n";
    cout << "[+] Machine code (hex): ";
    const uint8_t* code = gen.GetBuffer();
    for (size_t i = 0; i < gen.GetSize(); ++i) {
        printf("%02X ", code[i]);
    }
    cout << "\n\n";

    // Generate PE executable
    vector<uint8_t> code_vec(code, code + gen.GetSize());
    vector<uint8_t> pe = PEGenerator::GeneratePE(code_vec);

    cout << "[+] Generated PE executable: " << pe.size() << " bytes\n";

    // Verify PE signature
    if (pe.size() > 0x3C + 4) {
        uint32_t pe_offset = *(uint32_t*)(pe.data() + 0x3C);
        if (pe_offset < pe.size() && pe.size() > pe_offset + 4) {
            if (*(uint32_t*)(pe.data() + pe_offset) == 0x4550) {
                cout << "[✓] Valid PE signature detected\n";
            }
        }
    }

    // Save to file
    const char* output_file = "output\\generated_stub.exe";
    CreateDirectoryA("output", nullptr);
    
    FILE* f = fopen(output_file, "wb");
    if (f) {
        fwrite(pe.data(), 1, pe.size(), f);
        fclose(f);
        cout << "[✓] PE saved to: " << output_file << "\n";
    }
}

/**
 * @brief Demonstrate base64 encoding
 */
void DemoBase64Encoding() {
    cout << "\n[*] DEMO 4: Base64 Encoding\n";
    cout << "============================\n\n";

    const char* plaintext = "RawrXD Encoder Engine v5.0";
    vector<uint8_t> payload(plaintext, plaintext + strlen(plaintext));

    // Base64 output buffer (worst case: input_size * 4/3 + padding)
    vector<char> b64_output(payload.size() * 2);

    PolymorphicEncoder encoder;
    int64_t result = Rawshell_Base64EncodeBinary(
        payload.data(),
        b64_output.data(),
        payload.size()
    );

    cout << "[+] Encoded '" << plaintext << "'\n";
    cout << "[+] Result (base64): " << b64_output.data() << "\n";
}

/**
 * @brief Performance benchmark
 */
void DemoBenchmark() {
    cout << "\n[*] DEMO 5: Performance Benchmark\n";
    cout << "==================================\n\n";

    vector<size_t> sizes = {1024, 10240, 102400, 1048576};  // 1KB to 1MB

    PolymorphicEncoder encoder;
    encoder.SetAlgorithm(ENC_XOR_CHAIN);
    encoder.GenerateKey(32);

    for (size_t size : sizes) {
        vector<uint8_t> input(size);
        vector<uint8_t> output(size);

        // Fill with random data
        for (size_t i = 0; i < size; ++i) {
            input[i] = static_cast<uint8_t>(rand() & 0xFF);
        }

        // Benchmark encoding
        auto start = chrono::high_resolution_clock::now();
        encoder.Encode(input.data(), output.data(), size);
        auto end = chrono::high_resolution_clock::now();

        double elapsed_ms = chrono::duration<double, milli>(end - start).count();
        double throughput_mb = (size / (1024 * 1024)) / (elapsed_ms / 1000.0);

        printf("[+] Size: %7zu bytes | Time: %8.3f ms | Throughput: %8.2f MB/s\n",
               size, elapsed_ms, throughput_mb);
    }
}

/**
 * @brief Main integration demonstration
 */
int main() {
    cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    cout << "║  RawrXD Comprehensive Polymorphic Encoder v5.0                ║\n";
    cout << "║  Integration with NoRefusal Payload Supervisor                ║\n";
    cout << "╚═══════════════════════════════════════════════════════════════╝\n";

    try {
        DemoBasicEncoding();
        DemoPolymorphicEncoding();
        DemoCodeGeneration();
        DemoBase64Encoding();
        DemoBenchmark();

        cout << "\n[✓] All demonstrations completed successfully!\n\n";
    }
    catch (const exception& ex) {
        cerr << "[✗] Exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
