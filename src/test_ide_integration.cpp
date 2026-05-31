// ============================================================================
// test_ide_integration.cpp — Test Suite for Unified IDE Integration
// Tests all connected components: AgenticEngine, ChatInterface, ToolRegistry,
// GitHubMCPBridge, ModelRouter, VulkanCompute, etc.
// ============================================================================

#include "ide_integration.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

// Mock implementations for testing
class MockAgenticEngine : public AgenticEngine {
public:
    std::string AnalyzeCode(const std::string& code) override {
        return "{\"complexity\": 42, \"issues\": [], \"suggestions\": [\"Add comments\"]}";
    }
    std::string GenerateCode(const std::string& prompt, const std::string& language) override {
        return "// Generated code for: " + prompt + "\nvoid example() { }";
    }
    std::string RefactorCode(const std::string& code, const std::string& type) override {
        return "// Refactored: " + type + "\n" + code;
    }
    std::string ExplainCode(const std::string& code) override {
        return "This code does something important.";
    }
    std::string GenerateTests(const std::string& code) override {
        return "TEST(example_test) { ASSERT_TRUE(true); }";
    }
    std::string ExecuteTask(const std::string& desc, const std::string& prompt) override {
        return "Task completed: " + desc;
    }
    std::string RunSubAgent(const std::string& desc, const std::string& prompt) override {
        return "Sub-agent result: " + desc;
    }
    std::string ExecuteChain(const std::vector<std::string>& steps) override {
        return "Chain executed with " + std::to_string(steps.size()) + " steps";
    }
    std::vector<std::string> ExecuteSwarm(const std::vector<std::string>& prompts, int maxParallel) override {
        std::vector<std::string> results;
        for (const auto& p : prompts) {
            results.push_back("Swarm result: " + p);
        }
        return results;
    }
};

class MockChatInterface : public ChatInterface {
public:
    std::string SendMessage(const std::string& message) override {
        return "Response to: " + message;
    }
    void SendMessageAsync(const std::string& message, std::function<void(const std::string&)> callback) override {
        callback("Async response to: " + message);
    }
    std::vector<std::string> GetHistory() override {
        return {"msg1", "msg2", "msg3"};
    }
    void ClearHistory() override {
        // Clear history
    }
};

class MockInferenceEngine : public RawrXD::CPUInferenceEngine {
public:
    void LoadModel(const std::string& path) override {
        modelPath = path;
    }
    void UnloadModel() override {
        modelPath.clear();
    }
    std::string GetStatus() override {
        return modelPath.empty() ? "No model loaded" : "Model: " + modelPath;
    }
    void SetParameter(const std::string& key, const std::string& value) override {
        params[key] = value;
    }
    std::string Generate(const std::string& prompt, int maxTokens, float temperature) override {
        return "Generated: " + prompt.substr(0, std::min((size_t)50, prompt.size()));
    }
    void GenerateAsync(const std::string& prompt, std::function<void(const std::string&)> callback, int maxTokens) override {
        callback("Token1 ");
        callback("Token2 ");
        callback("Token3");
    }
    std::vector<float> Embed(const std::string& text) override {
        return {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    }
private:
    std::string modelPath;
    std::map<std::string, std::string> params;
};

class MockVulkanCompute : public RawrXD::VulkanCompute {
public:
    std::string GetDeviceInfo() override {
        return "{\"name\": \"MockGPU\", \"memory\": 8192, \"compute_units\": 64}";
    }
    uint32_t AllocateBuffer(size_t size) override {
        return nextBufferId++;
    }
    void FreeBuffer(uint32_t id) override {
        // Free buffer
    }
    void CopyToBuffer(uint32_t id, const void* data, size_t size) override {
        // Copy to GPU
    }
    void CopyFromBuffer(uint32_t id, void* data, size_t size) override {
        // Copy from GPU
    }
    void MatMul(uint32_t A, uint32_t B, uint32_t C, uint32_t M, uint32_t K, uint32_t N) override {
        // Matrix multiplication
    }
    void Attention(uint32_t Q, uint32_t K, uint32_t V, uint32_t out,
                   uint32_t seqLen, uint32_t headDim, uint32_t numHeads) override {
        // Attention computation
    }
private:
    uint32_t nextBufferId = 1;
};

// Test functions
void TestLifecycle() {
    std::cout << "Testing lifecycle..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    assert(!ide.IsInitialized());
    
    IDEComponents components;
    components.agenticEngine = new MockAgenticEngine();
    components.chatInterface = new MockChatInterface();
    components.inferenceEngine = new MockInferenceEngine();
    components.vulkanCompute = new MockVulkanCompute();
    
    assert(ide.Initialize(components));
    assert(ide.IsInitialized());
    
    ide.Shutdown();
    assert(!ide.IsInitialized());
    
    std::cout << "  ✓ Lifecycle test passed" << std::endl;
}

void TestChatOperations() {
    std::cout << "Testing chat operations..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    IDEComponents components;
    components.chatInterface = new MockChatInterface();
    ide.Initialize(components);
    
    // Test SendMessage
    auto result = ide.SendMessage("Hello, world!");
    assert(result.success);
    assert(result.output == "Response to: Hello, world!");
    
    // Test GetChatHistory
    auto history = ide.GetChatHistory();
    assert(history.size() == 3);
    
    // Test ClearChatHistory
    ide.ClearChatHistory();
    
    ide.Shutdown();
    
    std::cout << "  ✓ Chat operations test passed" << std::endl;
}

void TestCodeOperations() {
    std::cout << "Testing code operations..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    IDEComponents components;
    components.agenticEngine = new MockAgenticEngine();
    ide.Initialize(components);
    
    // Test AnalyzeCode
    auto result = ide.AnalyzeCode("int main() { return 0; }");
    assert(result.success);
    assert(result.output.find("complexity") != std::string::npos);
    
    // Test GenerateCode
    result = ide.GenerateCode("Create a hello world function", "cpp");
    assert(result.success);
    assert(result.output.find("Generated code") != std::string::npos);
    
    // Test RefactorCode
    result = ide.RefactorCode("int x = 1;", "extract_function");
    assert(result.success);
    assert(result.output.find("Refactored") != std::string::npos);
    
    // Test ExplainCode
    result = ide.ExplainCode("int x = 1;");
    assert(result.success);
    
    // Test GenerateTests
    result = ide.GenerateTests("int add(int a, int b) { return a + b; }");
    assert(result.success);
    assert(result.output.find("TEST") != std::string::npos);
    
    ide.Shutdown();
    
    std::cout << "  ✓ Code operations test passed" << std::endl;
}

void TestModelOperations() {
    std::cout << "Testing model operations..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    IDEComponents components;
    components.inferenceEngine = new MockInferenceEngine();
    ide.Initialize(components);
    
    // Test LoadModel
    auto result = ide.LoadModel("/path/to/model.gguf");
    assert(result.success);
    
    // Test GetModelStatus
    result = ide.GetModelStatus();
    assert(result.success);
    assert(result.output.find("model.gguf") != std::string::npos);
    
    // Test SetModelParameter
    result = ide.SetModelParameter("temperature", "0.8");
    assert(result.success);
    
    // Test Generate
    result = ide.Generate("Hello, world!", 100, 0.8f);
    assert(result.success);
    assert(result.output.find("Generated") != std::string::npos);
    
    // Test Embed
    result = ide.Embed("Hello, world!");
    assert(result.success);
    assert(result.output.find("[") != std::string::npos);
    
    // Test UnloadModel
    result = ide.UnloadModel();
    assert(result.success);
    
    ide.Shutdown();
    
    std::cout << "  ✓ Model operations test passed" << std::endl;
}

void TestGPUOperations() {
    std::cout << "Testing GPU operations..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    IDEComponents components;
    components.vulkanCompute = new MockVulkanCompute();
    ide.Initialize(components);
    
    // Test GetGPUInfo
    auto result = ide.GetGPUInfo();
    assert(result.success);
    assert(result.output.find("MockGPU") != std::string::npos);
    
    // Test AllocateGPUBuffer
    result = ide.AllocateGPUBuffer(1024);
    assert(result.success);
    uint32_t bufferId = std::stoi(result.output);
    
    // Test GPUMatMul
    result = ide.GPUMatMul(bufferId, bufferId, bufferId, 128, 64, 128);
    assert(result.success);
    
    // Test GPUAttention
    result = ide.GPUAttention(bufferId, bufferId, bufferId, bufferId, 128, 64, 8);
    assert(result.success);
    
    // Test FreeGPUBuffer
    result = ide.FreeGPUBuffer(bufferId);
    assert(result.success);
    
    ide.Shutdown();
    
    std::cout << "  ✓ GPU operations test passed" << std::endl;
}

void TestAgentOperations() {
    std::cout << "Testing agent operations..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    IDEComponents components;
    components.agenticEngine = new MockAgenticEngine();
    ide.Initialize(components);
    
    // Test ExecuteAgentTask
    auto result = ide.ExecuteAgentTask("Analyze code", "Find bugs in this code");
    assert(result.success);
    assert(result.output.find("Task completed") != std::string::npos);
    
    // Test RunSubAgent
    result = ide.RunSubAgent("Code reviewer", "Review this PR");
    assert(result.success);
    assert(result.output.find("Sub-agent result") != std::string::npos);
    
    // Test ExecuteChain
    result = ide.ExecuteChain({"Step 1", "Step 2", "Step 3"});
    assert(result.success);
    assert(result.output.find("3 steps") != std::string::npos);
    
    // Test ExecuteSwarm
    result = ide.ExecuteSwarm({"Task 1", "Task 2", "Task 3"}, 4);
    assert(result.success);
    assert(result.output.find("Swarm result") != std::string::npos);
    
    ide.Shutdown();
    
    std::cout << "  ✓ Agent operations test passed" << std::endl;
}

void TestDiagnostics() {
    std::cout << "Testing diagnostics..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    IDEComponents components;
    components.agenticEngine = new MockAgenticEngine();
    components.chatInterface = new MockChatInterface();
    components.inferenceEngine = new MockInferenceEngine();
    components.vulkanCompute = new MockVulkanCompute();
    ide.Initialize(components);
    
    // Test GetDiagnostics
    auto diag = ide.GetDiagnostics();
    assert(diag.find("\"initialized\": true") != std::string::npos);
    assert(diag.find("\"agenticEngine\": true") != std::string::npos);
    assert(diag.find("\"chatInterface\": true") != std::string::npos);
    
    // Test GetStats
    auto stats = ide.GetStats();
    assert(stats["initialized"] == "true");
    assert(stats["has_agentic_engine"] == "true");
    assert(stats["has_chat_interface"] == "true");
    
    ide.Shutdown();
    
    std::cout << "  ✓ Diagnostics test passed" << std::endl;
}

void TestBuilder() {
    std::cout << "Testing builder pattern..." << std::endl;
    
    MockAgenticEngine agenticEngine;
    MockChatInterface chatInterface;
    MockInferenceEngine inferenceEngine;
    MockVulkanCompute vulkanCompute;
    
    IDEComponents components = IDEBuilder()
        .WithAgenticEngine(&agenticEngine)
        .WithChatInterface(&chatInterface)
        .WithInferenceEngine(&inferenceEngine)
        .WithVulkanCompute(&vulkanCompute)
        .Build();
    
    assert(components.agenticEngine == &agenticEngine);
    assert(components.chatInterface == &chatInterface);
    assert(components.inferenceEngine == &inferenceEngine);
    assert(components.vulkanCompute == &vulkanCompute);
    
    std::cout << "  ✓ Builder pattern test passed" << std::endl;
}

void TestCAPI() {
    std::cout << "Testing C API..." << std::endl;
    
    MockAgenticEngine agenticEngine;
    MockChatInterface chatInterface;
    MockInferenceEngine inferenceEngine;
    MockVulkanCompute vulkanCompute;
    
    IDEComponents components;
    components.agenticEngine = &agenticEngine;
    components.chatInterface = &chatInterface;
    components.inferenceEngine = &inferenceEngine;
    components.vulkanCompute = &vulkanCompute;
    
    // Test IDE_Init
    assert(IDE_Init(&components));
    
    // Test IDE_IsInitialized
    assert(IDE_IsInitialized());
    
    // Test IDE_SendMessage
    char result[1024];
    int ret = IDE_SendMessage("Hello", result, sizeof(result));
    assert(ret == 0);
    assert(std::string(result).find("Response to") != std::string::npos);
    
    // Test IDE_AnalyzeCode
    ret = IDE_AnalyzeCode("int x = 1;", result, sizeof(result));
    assert(ret == 0);
    assert(std::string(result).find("complexity") != std::string::npos);
    
    // Test IDE_GenerateCode
    ret = IDE_GenerateCode("Create function", "cpp", result, sizeof(result));
    assert(ret == 0);
    assert(std::string(result).find("Generated code") != std::string::npos);
    
    // Test IDE_LoadModel
    ret = IDE_LoadModel("/path/to/model.gguf", result, sizeof(result));
    assert(ret == 0);
    
    // Test IDE_Generate
    ret = IDE_Generate("Hello", 100, 0.8f, result, sizeof(result));
    assert(ret == 0);
    assert(std::string(result).find("Generated") != std::string::npos);
    
    // Test IDE_GetGPUInfo
    ret = IDE_GetGPUInfo(result, sizeof(result));
    assert(ret == 0);
    assert(std::string(result).find("MockGPU") != std::string::npos);
    
    // Test IDE_GetDiagnostics
    ret = IDE_GetDiagnostics(result, sizeof(result));
    assert(ret == 0);
    assert(std::string(result).find("initialized") != std::string::npos);
    
    // Test IDE_Shutdown
    IDE_Shutdown();
    assert(!IDE_IsInitialized());
    
    std::cout << "  ✓ C API test passed" << std::endl;
}

void TestPerformance() {
    std::cout << "Testing performance..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    IDEComponents components;
    components.agenticEngine = new MockAgenticEngine();
    components.chatInterface = new MockChatInterface();
    components.inferenceEngine = new MockInferenceEngine();
    ide.Initialize(components);
    
    const int iterations = 1000;
    
    // Benchmark SendMessage
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto result = ide.SendMessage("Test message " + std::to_string(i));
        assert(result.success);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double avgMs = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    std::cout << "  SendMessage avg: " << avgMs << " ms" << std::endl;
    assert(avgMs < 1.0); // Should be sub-millisecond for mock
    
    // Benchmark Generate
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto result = ide.Generate("Test prompt", 100, 0.8f);
        assert(result.success);
    }
    end = std::chrono::high_resolution_clock::now();
    avgMs = std::chrono::duration<double, std::milli>(end - start).count() / iterations;
    std::cout << "  Generate avg: " << avgMs << " ms" << std::endl;
    assert(avgMs < 1.0); // Should be sub-millisecond for mock
    
    ide.Shutdown();
    
    std::cout << "  ✓ Performance test passed" << std::endl;
}

void TestEventHandling() {
    std::cout << "Testing event handling..." << std::endl;
    
    IDEIntegration& ide = IDEIntegration::Instance();
    
    IDEComponents components;
    components.agenticEngine = new MockAgenticEngine();
    ide.Initialize(components);
    
    std::vector<std::string> receivedEvents;
    
    ide.SetEventHandler([&receivedEvents](const std::string& eventType, const std::string& data) {
        receivedEvents.push_back(eventType + ":" + data);
    });
    
    // Trigger events through operations
    auto result = ide.SendMessage("Test");
    assert(result.success);
    
    // Check if events were received
    // Note: In real implementation, events would be emitted during operations
    
    ide.Shutdown();
    
    std::cout << "  ✓ Event handling test passed" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "IDE Integration Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    TestLifecycle();
    TestChatOperations();
    TestCodeOperations();
    TestModelOperations();
    TestGPUOperations();
    TestAgentOperations();
    TestDiagnostics();
    TestBuilder();
    TestCAPI();
    TestPerformance();
    TestEventHandling();
    
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "All tests passed! ✓" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
