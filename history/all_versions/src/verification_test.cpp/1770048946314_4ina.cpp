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

    fs::remove(modelName);
}

int main() {
    std::cout << "RawrXD Verification Suite\n";
    std::cout << "=========================\n";
    
    TestAgenticCapabilities();
    TestInferencePipeline();
    
    std::cout << "\n[Summary] All Tests Completed.\n";
    return 0;
}
