// copilot_integration_example.cpp - Example usage of Copilot-like pipeline
// This demonstrates how to integrate the pipeline into your IDE.
// 
// The pipeline provides:
//   - Automatic kernel selection (Q4_K/Q5_K/Q6_K) based on latency budget
//   - Speculative decoding for faster perceived response
//   - KV cache reuse for repeated contexts
//   - Streaming ghost text with TAB accept / ESC reject
//   - All 15 TPS/latency optimizations
//
// Total implementation: ~1,443 LOC (well under 50k limit)

#include "copilot_pipeline.h"
#include "vulkan_compute.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace RawrXD;

// Example: IDE integration
void ExampleIDEIntegration() {
    // 1. Initialize Vulkan compute
    VulkanCompute vulkan;
    if (!vulkan.Initialize()) {
        std::cerr << "Failed to initialize Vulkan\n";
        return;
    }
    
    // 2. Create Copilot pipeline
    auto pipeline = CreateCopilotPipeline(&vulkan);
    
    // 3. Load model (Ollama integration)
    if (!pipeline->LoadModel("codestral:22b")) {
        std::cerr << "Failed to load model\n";
        return;
    }
    
    // 4. Configure for autocomplete
    IDECompletionBridge::Config ide_config;
    ide_config.max_context_lines = 400;
    ide_config.max_completion_tokens = 100;
    ide_config.enable_speculative = true;
    ide_config.enable_adaptive_quant = true;
    ide_config.debounce_ms = std::chrono::milliseconds(150);
    pipeline->SetIDEConfig(ide_config);
    
    // 5. Request completion
    CompletionRequest request;
    request.type = IDEContextType::CODE_COMPLETION;
    request.file_path = "main.cpp";
    request.file_content = R"(
#include <iostream>
#include <vector>

// Function to process data
void processData(const std::vector<int>& data) {
    for (int x : data) {
        std::cout << x << " ";
    }
}

int main() {
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    // CURSOR HERE
    return 0;
}
)";
    request.cursor_line = 14;
    request.cursor_column = 8;
    request.max_tokens = 50;
    request.timeout = std::chrono::milliseconds(200);
    
    // 6. Stream completion
    pipeline->RequestCompletion(request, [](const CompletionResult& result) {
        if (result.accepted) {
            std::cout << "Accepted completion:\n" << result.text << "\n";
            std::cout << "Latency: " << result.latency.count() << " us\n";
            std::cout << "Kernel: " << result.kernel_used << "\n";
        }
    });
    
    // 7. Wait for completion
    while (pipeline->IsGenerating()) {
        // Show ghost text
        GhostText ghost = pipeline->GetGhostText();
        if (ghost.visible) {
            std::cout << "\r[Ghost] " << ghost.text << std::flush;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 8. Get statistics
    auto inference_stats = pipeline->GetInferenceStats();
    auto completion_stats = pipeline->GetCompletionStats();
    
    std::cout << "\n=== Statistics ===\n";
    std::cout << "First token latency: " << inference_stats.first_token_latency.count() << " us\n";
    std::cout << "Avg token latency: " << inference_stats.avg_token_latency.count() << " us\n";
    std::cout << "Tokens generated: " << inference_stats.tokens_generated << "\n";
    std::cout << "Kernel switches: " << inference_stats.kernel_switches << "\n";
    std::cout << "Completions requested: " << completion_stats.completions_requested << "\n";
    std::cout << "Completions accepted: " << completion_stats.completions_accepted << "\n";
    std::cout << "Q4_K uses: " << completion_stats.kernel_q4k_count << "\n";
    std::cout << "Q5_K uses: " << completion_stats.kernel_q5k_count << "\n";
    std::cout << "Q6_K uses: " << completion_stats.kernel_q6k_count << "\n";
}

// Example: Manual kernel selection
void ExampleManualKernelSelection() {
    VulkanCompute vulkan;
    vulkan.Initialize();
    
    auto pipeline = CreateCopilotPipeline(&vulkan);
    pipeline->LoadModel("codestral:22b");
    
    // Force Q4_K for fastest autocomplete
    pipeline->SetKernelMode(1);  // Q4KQ81U32
    
    // Or use Q6_K for highest quality
    // pipeline->SetKernelMode(4);  // Q6KU32
}

// Example: Real-time typing with debounce
void ExampleRealTimeTyping() {
    VulkanCompute vulkan;
    vulkan.Initialize();
    
    auto pipeline = CreateCopilotPipeline(&vulkan);
    pipeline->LoadModel("codestral:22b");
    
    // Configure for real-time typing
    IDECompletionBridge::Config config;
    config.debounce_ms = std::chrono::milliseconds(150);  // Wait 150ms after typing stops
    config.max_context_lines = 200;  // Smaller context for faster response
    config.max_completion_tokens = 30;  // Shorter completions
    pipeline->SetIDEConfig(config);
    
    // Simulate typing
    std::string code = "int main() {\n    std::cout << \"Hello";
    
    CompletionRequest request;
    request.type = IDEContextType::CODE_COMPLETION;
    request.file_content = code;
    request.cursor_line = 1;
    request.cursor_column = static_cast<int>(code.length() - code.find_last_of('\n') - 1);
    request.max_tokens = 30;
    
    // Each keystroke cancels previous request (debounce handles this)
    pipeline->RequestCompletion(request, [](const CompletionResult& result) {
        if (result.accepted) {
            std::cout << "Completion: " << result.text << "\n";
        }
    });
}

// Example: Speculative decoding
void ExampleSpeculativeDecoding() {
    VulkanCompute vulkan;
    vulkan.Initialize();
    
    auto pipeline = CreateCopilotPipeline(&vulkan);
    pipeline->LoadModel("qwen3.5-40b-q4");
    
    // Enable speculative decoding
    IDECompletionBridge::Config config;
    config.enable_speculative = true;  // Q4_K draft + Q6_K verify
    pipeline->SetIDEConfig(config);
    
    // Request will automatically use speculative decode
    CompletionRequest request;
    request.type = IDEContextType::FUNCTION_GENERATION;
    request.file_content = "// Generate a sorting function\n";
    request.cursor_line = 1;
    request.cursor_column = 0;
    request.max_tokens = 200;
    
    pipeline->RequestCompletion(request, [](const CompletionResult& result) {
        std::cout << "Generated function:\n" << result.text << "\n";
    });
}

// Example: Adaptive quantization
void ExampleAdaptiveQuantization() {
    VulkanCompute vulkan;
    vulkan.Initialize();
    
    auto pipeline = CreateCopilotPipeline(&vulkan);
    pipeline->LoadModel("codestral:22b");
    
    // Enable adaptive quant switching
    IDECompletionBridge::Config config;
    config.enable_adaptive_quant = true;  // Switch kernels based on confidence
    pipeline->SetIDEConfig(config);
    
    // Pipeline will automatically:
    //   - Start with Q4_K for fast first token
    //   - Switch to Q5_K if confidence drops
    //   - Use Q6_K for final refinement
    
    CompletionRequest request;
    request.type = IDEContextType::CODE_COMPLETION;
    request.file_content = "def fibonacci(n):\n";
    request.cursor_line = 1;
    request.cursor_column = 0;
    request.max_tokens = 100;
    
    pipeline->RequestCompletion(request, [](const CompletionResult& result) {
        std::cout << "Completion: " << result.text << "\n";
        std::cout << "Kernel used: " << result.kernel_used << "\n";
    });
}

int main() {
    std::cout << "=== Copilot Pipeline Demo ===\n\n";
    
    std::cout << "1. IDE Integration Example\n";
    ExampleIDEIntegration();
    
    std::cout << "\n2. Manual Kernel Selection\n";
    ExampleManualKernelSelection();
    
    std::cout << "\n3. Real-Time Typing\n";
    ExampleRealTimeTyping();
    
    std::cout << "\n4. Speculative Decoding\n";
    ExampleSpeculativeDecoding();
    
    std::cout << "\n5. Adaptive Quantization\n";
    ExampleAdaptiveQuantization();
    
    return 0;
}