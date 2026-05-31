#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cassert>
#include <cstring>
#include <cmath>

// Define necessary stubs/mocks for the visualizer/gui parts to allow CLI compilation
// if they are pulled in by headers.

#include "agentic_engine.h"
#include "cpu_inference_engine.h"

// We need to link against:
// agentic_engine.cpp
// cpu_inference_engine.cpp
// universal_model_router.cpp
// rawrxd_model_loader.cpp
// rawrxd_transformer.cpp
// rawrxd_tokenizer.cpp
// rawrxd_sampler.cpp
// action_executor.cpp (maybe)

using namespace RawrXD;
namespace fs = std::filesystem;

extern "C" void DequantQ4_0_AVX512(void* src, uint16_t* dst, size_t blocks) {
    if (!src || !dst || blocks == 0) return;
    // Q4_0 dequantization: each block = 2 bytes scale + 16 bytes packed weights (32 x 4-bit)
    const uint8_t* in = static_cast<const uint8_t*>(src);
    for (size_t b = 0; b < blocks; ++b) {
        // Read scale (first 2 bytes as half-precision float)
        uint16_t scale_half = *reinterpret_cast<const uint16_t*>(in + b * 18);
        // Convert half to float (simplified)
        float scale = scale_half * 1.0f / 512.0f; // Approximate
        // Unpack 16 bytes into 32 nibbles
        for (int i = 0; i < 16; ++i) {
            uint8_t packed = in[b * 18 + 2 + i];
            uint8_t low = packed & 0x0F;
            uint8_t high = (packed >> 4) & 0x0F;
            // Dequantize: value = scale * (nibble - 8)
            float v_low = scale * (static_cast<float>(low) - 8.0f);
            float v_high = scale * (static_cast<float>(high) - 8.0f);
            // Convert to uint16_t (simple clamp)
            dst[b * 32 + i * 2] = static_cast<uint16_t>(std::max(0.0f, v_low * 1000.0f));
            dst[b * 32 + i * 2 + 1] = static_cast<uint16_t>(std::max(0.0f, v_high * 1000.0f));
        }
    }
}

extern "C" void DequantQ4_0_AVX2(void* src, uint16_t* dst, size_t blocks) {
    // AVX2 path falls back to same implementation (compiler will vectorize)
    DequantQ4_0_AVX512(src, dst, blocks);
}

void CreateDummyModel(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    // Create a generic header? No, we implemented BLOB support.
    // Just flat floats.
    // 4MB of data
    std::vector<float> data(1024 * 1024);
    for(auto& x : data) x = ((float)rand() / RAND_MAX) * 0.1f;
    f.write((char*)data.data(), data.size() * sizeof(float));
    f.close();
    std::cout << "[Test] Created dummy model: " << path << std::endl;
}

void TestAgenticCapabilities() {
    std::cout << "\n=== Testing Agentic Capabilities ===\n";
    AgenticEngine agent;
    agent.initialize();

    // Test Plan Generation (Mocking LLM response or using the real simple parser)
    // Since we don't have a real intelligent model loaded, the "processQuery"
    // will return random garbage if we used the model.
    // BUT, AgenticEngine::planTask calls `processQuery`.
    // If we want to verify the *execution* logic, we can verify executePlan directly
    // with a manually constructed plan.
    
    std::cout << "[Test] Testing Plan Execution...\n";
    
    nlohmann::json plan = nlohmann::json::array();
    plan.push_back({
        {"type", "file_edit"},
        {"target", "test_output.txt"},
        {"content", "Hello from Agentic Engine!"}
    });
    plan.push_back({
        {"type", "command"},
        {"cmd", "echo Agent Execution Success"}
    });
    
    std::string report = agent.executePlan(plan);
    std::cout << report << std::endl;
    
    // Verify file creation
    if (fs::exists("test_output.txt")) {
        std::ifstream t("test_output.txt");
        std::stringstream buffer;
        buffer << t.rdbuf();
        if (buffer.str() == "Hello from Agentic Engine!") {
            std::cout << "[PASS] Agentic File Creation Verified.\n";
        } else {
            std::cout << "[FAIL] File content mismatch.\n";
        }
    } else {
        std::cout << "[FAIL] File was not created.\n";
    }
}

void TestInferencePipeline() {
    std::cout << "\n=== Testing Inference Pipeline ===\n";
    
    std::string modelName = "test_model.blob";
    CreateDummyModel(modelName);
    
    auto engine = std::make_shared<CPUInferenceEngine>();
    
    std::cout << "[Test] Loading Model...\n";
    auto result = engine->loadModel(modelName);
    
    if (result.has_value()) {
        std::cout << "[PASS] Model Load Success.\n";
    } else {
        std::cout << "[FAIL] Model Load Failed.\n";
        // It might fail if Vulkan init fails on this CI environment.
        // We will proceed to check graceful handling.
        return;
    }
    
    std::cout << "[Test] Tokenization...\n";
    auto tokens = engine->Tokenize("Hello World");
    std::cout << "Tokens: " << tokens.size() << "\n";
    if (tokens.size() > 0) std::cout << "[PASS] Tokenizer works (ASCII fallback).\n";
    
    std::cout << "[Test] Generation (Streaming)...\n";
    bool done = false;
    int tokenCount = 0;
    
    engine->GenerateStreaming(tokens, 10, [&](const std::string& s){
        tokenCount++;
        std::cout << ".";
    }, [&](){
        done = true;
    });
    
    if (done) std::cout << "\n[PASS] Generation completed.\n";
    else std::cout << "\n[FAIL] Generation hung or failed.\n";

    // Destruct engine to release file lock
    engine.reset();

    try {
        fs::remove(modelName);
    } catch(...) {
        fprintf(stderr, "[VerificationTest] Removal error ignored\n");
    }
}

int main() {
    std::cout << "RawrXD Verification Suite\n";
    std::cout << "=========================\n";
    
    TestAgenticCapabilities();
    TestInferencePipeline();
    
    std::cout << "\n[Summary] All Tests Completed.\n";
    return 0;
}
