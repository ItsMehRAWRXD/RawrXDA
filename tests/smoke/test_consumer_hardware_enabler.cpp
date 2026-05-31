// ============================================================================
// File: tests/smoke/test_consumer_hardware_enabler.cpp
// ============================================================================

#include "memory/consumer_hardware_enabler.hpp"
#include <iostream>
#include <vector>
#include <cassert>

using namespace rawrxd::memory;

bool test_hardware_profile() {
    std::cout << "Testing hardware profiles... ";
    
    auto rtx4090 = HardwareProfile::RTX4090();
    assert(rtx4090.vram_bytes == 24ULL * 1024 * 1024 * 1024);
    assert(rtx4090.has_tensor_cores);
    
    auto rtx3080 = HardwareProfile::RTX3080();
    assert(rtx3080.vram_bytes == 10ULL * 1024 * 1024 * 1024);
    assert(!rtx3080.has_fp8_support);
    
    auto rtx4070 = HardwareProfile::RTX4070();
    assert(rtx4070.vram_bytes == 12ULL * 1024 * 1024 * 1024);
    
    std::cout << "PASS\n";
    return true;
}

bool test_model_profile() {
    std::cout << "Testing model profiles... ";
    
    auto qwen = ModelProfile::Qwen235B();
    assert(qwen.num_parameters > 200000000000ULL);
    assert(qwen.num_layers == 80);
    assert(qwen.fp16_weight_bytes > 400ULL * 1024 * 1024 * 1024);
    
    auto llama = ModelProfile::Llama405B();
    assert(llama.num_parameters > 400000000000ULL);
    
    auto mixtral = ModelProfile::Mixtral8x7B();
    assert(mixtral.is_moe);
    assert(mixtral.num_experts == 8);
    
    std::cout << "PASS\n";
    return true;
}

bool test_layer_pruner() {
    std::cout << "Testing layer pruner... ";
    
    LayerPruner pruner(LayerPruner::PruningStrategy::ADAPTIVE);
    
    auto model = ModelProfile::Qwen235B();
    auto hardware = HardwareProfile::RTX3080();  // 10 GB
    
    // Prune
    auto result = pruner.prune(model, hardware, 0.05f);
    
    assert(!result.pruned_layers.empty());
    assert(!result.remaining_layers.empty());
    assert(result.memory_saved > 0);
    
    // Get optimal layer count
    int optimal = pruner.getOptimalLayerCount(model, hardware);
    assert(optimal > 0);
    assert(optimal < static_cast<int>(model.num_layers));
    
    std::cout << "PASS (pruned " << result.pruned_layers.size() << " layers)\n";
    return true;
}

bool test_early_exit_controller() {
    std::cout << "Testing early exit controller... ";
    
    EarlyExitController controller(0.9f);
    
    // Add exit points
    controller.addExitPoint(20, 0.85f);
    controller.addExitPoint(40, 0.90f);
    controller.addExitPoint(60, 0.95f);
    
    // Create test hidden state (high confidence)
    std::vector<float> hidden_state(4096);
    for (size_t i = 0; i < 100; ++i) {
        hidden_state[i] = 2.0f;  // Peak at first 100
    }
    for (size_t i = 100; i < hidden_state.size(); ++i) {
        hidden_state[i] = -2.0f;  // Low for rest
    }
    
    std::vector<float> attention(4096);
    for (size_t i = 0; i < attention.size(); ++i) {
        attention[i] = (i < 100) ? 1.0f : 0.01f;
    }
    
    // Test at layer 20
    auto decision = controller.shouldExit(20, hidden_state.data(), 
                                        hidden_state.size(), attention);
    
    assert(decision.exit_layer == 20);
    
    // Get statistics
    float exit_rate = controller.getExitRate();
    
    std::cout << "PASS\n";
    return true;
}

bool test_speculative_execution() {
    std::cout << "Testing speculative execution... ";
    
    SpeculativeExecutionEngine engine(1000000000, 4);
    
    // Mock draft generation
    auto draft_generate = [](const std::vector<int>& ctx, int n) {
        std::vector<int> draft;
        for (int i = 0; i < n; ++i) {
            draft.push_back(i + 100);
        }
        return draft;
    };
    
    // Mock main forward
    auto main_forward = [](const std::vector<int>& ctx) {
        std::vector<float> logits(1000, 0.0f);
        logits[100] = 2.0f;
        logits[101] = 1.5f;
        logits[102] = 1.0f;
        return std::make_pair(logits, 100);
    };
    
    std::vector<int> context = {1, 2, 3, 4, 5};
    auto result = engine.execute(context, draft_generate, main_forward);
    
    assert(!result.draft_tokens.empty());
    assert(result.speedup_factor >= 1.0f);
    
    // Get statistics
    float acceptance_rate = engine.getAcceptanceRate();
    
    std::cout << "PASS (acceptance: " << (acceptance_rate * 100) << "%)\n";
    return true;
}

bool test_execution_planning() {
    std::cout << "Testing execution planning... ";
    
    ConsumerHardwareEnabler enabler;
    
    auto hardware = HardwareProfile::RTX3080();
    assert(enabler.initialize(hardware));
    
    auto model = ModelProfile::Qwen235B();
    
    auto plan = enabler.planExecution(model, hardware, 0.95f, 10.0f);
    
    assert(!plan.gpu_layers.empty());
    assert(plan.estimated_vram_usage > 0);
    assert(plan.estimated_tps > 0);
    
    std::cout << "PASS\n";
    return true;
}

bool test_enable_model() {
    std::cout << "Testing model enabling... ";
    
    ConsumerHardwareEnabler enabler;
    
    auto hardware = HardwareProfile::RTX4090();
    assert(enabler.initialize(hardware));
    
    auto model = ModelProfile::Qwen235B();
    auto result = enabler.enableModel(model);
    
    std::cout << "Result: " << result.message << "\n";
    std::cout << "  GPU layers: " << result.plan.gpu_layers.size() << "\n";
    std::cout << "  CPU layers: " << result.plan.cpu_layers.size() << "\n";
    std::cout << "  VRAM usage: " << (result.memory_required / 1024.0 / 1024.0 / 1024.0) << " GB\n";
    
    assert(result.success || !result.plan.gpu_layers.empty());
    
    std::cout << "PASS\n";
    return true;
}

bool test_realistic_qwen235b_rtx3080() {
    std::cout << "\n=== Realistic: Qwen-235B on RTX 3080 (10GB) ===\n";
    
    ConsumerHardwareEnabler enabler;
    
    auto hardware = HardwareProfile::RTX3080();
    assert(enabler.initialize(hardware));
    
    std::cout << "Hardware: RTX 3080\n";
    std::cout << "  VRAM: " << (hardware.vram_bytes / 1024.0 / 1024.0 / 1024.0) << " GB\n";
    std::cout << "  RAM: " << (hardware.ram_bytes / 1024.0 / 1024.0 / 1024.0) << " GB\n";
    
    auto model = ModelProfile::Qwen235B();
    auto result = enabler.enableModel(model);
    
    std::cout << "\nModel: Qwen-235B\n";
    std::cout << "  Parameters: " << model.num_parameters << "\n";
    std::cout << "  Layers: " << model.num_layers << "\n";
    std::cout << "  Original size: " << (model.fp16_weight_bytes / 1024.0 / 1024.0 / 1024.0) << " GB\n";
    
    if (result.success) {
        std::cout << "\n✓ ENABLED\n";
        std::cout << "  GPU layers: " << result.plan.gpu_layers.size() << "\n";
        std::cout << "  CPU layers: " << result.plan.cpu_layers.size() << "\n";
        std::cout << "  Disk layers: " << result.plan.disk_layers.size() << "\n";
        std::cout << "  Estimated VRAM: " << (result.plan.estimated_vram_usage / 1024.0 / 1024.0 / 1024.0) << " GB\n";
        std::cout << "  Estimated TPS: " << result.estimated_tps << "\n";
        std::cout << "  Estimated quality: " << (result.estimated_quality * 100) << "%\n";
        
        // Check optimization techniques
        std::cout << "\n  Optimizations:\n";
        std::cout << "    - Speculative execution: " << (result.plan.use_speculative ? "ON" : "OFF") << "\n";
        std::cout << "    - Recomputation: " << (result.plan.use_recomputation ? "ON" : "OFF") << "\n";
        std::cout << "    - KV offloading: " << (result.plan.kv_offload_enabled ? "ON" : "OFF") << "\n";
        
        // Precision breakdown
        int fp16_layers = 0, int8_layers = 0, int4_layers = 0;
        for (int bits : result.plan.layer_bits) {
            if (bits >= 16) fp16_layers++;
            else if (bits >= 8) int8_layers++;
            else int4_layers++;
        }
        
        std::cout << "\n  Precision breakdown:\n";
        std::cout << "    - FP16: " << fp16_layers << " layers\n";
        std::cout << "    - INT8: " << int8_layers << " layers\n";
        std::cout << "    - INT4: " << int4_layers << " layers\n";
        
    } else {
        std::cout << "\n✗ NOT FEASIBLE\n";
        std::cout << "  Reason: " << result.message << "\n";
    }
    
    std::cout << "PASS\n";
    return true;
}

bool test_realistic_mixtral_rtx4070() {
    std::cout << "\n=== Realistic: Mixtral-8x7B on RTX 4070 (12GB) ===\n";
    
    ConsumerHardwareEnabler enabler;
    
    auto hardware = HardwareProfile::RTX4070();
    assert(enabler.initialize(hardware));
    
    std::cout << "Hardware: RTX 4070\n";
    std::cout << "  VRAM: " << (hardware.vram_bytes / 1024.0 / 1024.0 / 1024.0) << " GB\n";
    
    auto model = ModelProfile::Mixtral8x7B();
    auto result = enabler.enableModel(model);
    
    std::cout << "\nModel: Mixtral-8x7B (MoE)\n";
    std::cout << "  Parameters: " << model.num_parameters << "\n";
    std::cout << "  Experts: " << model.num_experts << " (top-" << model.experts_per_token << ")\n";
    
    if (result.success) {
        std::cout << "\n✓ ENABLED\n";
        std::cout << "  Estimated TPS: " << result.estimated_tps << "\n";
        std::cout << "  Estimated quality: " << (result.estimated_quality * 100) << "%\n";
    } else {
        std::cout << "\n✗ " << result.message << "\n";
    }
    
    std::cout << "PASS\n";
    return true;
}

int main() {
    std::cout << "\n=== Consumer Hardware Model Enabler Smoke Tests ===\n\n";
    
    int passed = 0;
    int total = 0;
    
    auto run_test = [&](const char* name, bool (*test)()) {
        total++;
        try {
            if (test()) {
                passed++;
            }
        } catch (const std::exception& e) {
            std::cout << "FAIL (exception: " << e.what() << ")\n";
        }
    };
    
    run_test("Hardware Profile", test_hardware_profile);
    run_test("Model Profile", test_model_profile);
    run_test("Layer Pruner", test_layer_pruner);
    run_test("Early Exit Controller", test_early_exit_controller);
    run_test("Speculative Execution", test_speculative_execution);
    run_test("Execution Planning", test_execution_planning);
    run_test("Enable Model", test_enable_model);
    
    // Realistic scenarios
    test_realistic_qwen235b_rtx3080();
    test_realistic_mixtral_rtx4070();
    
    std::cout << "\n=== Results: " << passed << "/" << total << " tests passed ===\n\n";
    
    return (passed == total) ? 0 : 1;
}
