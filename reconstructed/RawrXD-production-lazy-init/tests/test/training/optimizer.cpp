#include "../include/training_optimizer.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace TrainingOptimizer;

void test_hardware_detection() {
    std::cout << "Testing hardware detection...\n";
    
    auto profile = HardwareDetector::detectHardware();
    
    std::cout << HardwareDetector::getHardwareDescription(profile);
    
    // Verify profile has valid values
    assert(profile.cpuCores > 0);
    assert(profile.cpuThreads > 0);
    assert(profile.ramTotalBytes > 0);
    assert(profile.ramAvailableBytes > 0);
    
    std::cout << "✓ Hardware detection passed\n\n";
}

void test_training_profiler() {
    std::cout << "Testing training profiler...\n";
    
    TrainingProfiler profiler;
    
    // Simulate training operations
    for (int i = 0; i < 5; i++) {
        profiler.startOperation("matmul");
        // Simulate work
        volatile double x = 0;
        for (int j = 0; j < 1000000; j++) x += j * 0.1;
        profiler.endOperation("matmul");
        
        profiler.startOperation("activation");
        for (int j = 0; j < 500000; j++) x += j * 0.05;
        profiler.endOperation("activation");
    }
    
    profiler.recordEpochComplete(1000.0);
    
    auto bottlenecks = profiler.getBottlenecks(3);
    std::cout << "Top bottlenecks: " << bottlenecks.size() << "\n";
    
    assert(profiler.getTotalOperations() > 0);
    
    std::cout << "✓ Training profiler passed\n\n";
}

void test_simd_optimizer() {
    std::cout << "Testing SIMD optimizer...\n";
    
    auto profile = HardwareDetector::detectHardware();
    
    // Test matrix multiply
    const int M = 256, N = 256, K = 256;
    std::vector<float> A(M * K, 1.0f);
    std::vector<float> B(K * N, 1.0f);
    std::vector<float> C(M * N, 0.0f);
    
    SIMDOptimizer::matmul(A.data(), B.data(), C.data(), M, N, K, profile);
    
    // Verify result (all 1s * all 1s = K)
    for (int i = 0; i < std::min(10, M); i++) {
        for (int j = 0; j < std::min(10, N); j++) {
            assert(std::abs(C[i * N + j] - K) < 0.1f);
        }
    }
    
    // Test GELU activation
    std::vector<float> x(1000);
    for (int i = 0; i < 1000; i++) x[i] = (i - 500) * 0.01f;  // Range [-5, 5]
    
    SIMDOptimizer::gelu(x.data(), x.size(), profile);
    
    // GELU(0) should be close to 0
    assert(std::abs(x[500]) < 0.1f);
    
    std::cout << "✓ SIMD optimizer passed\n\n";
}

void test_mixed_precision_trainer() {
    std::cout << "Testing mixed precision trainer...\n";
    
    auto profile = HardwareDetector::detectHardware();
    MixedPrecisionTrainer trainer(PrecisionMode::FP16, profile);
    
    // Test FP32 to FP16 conversion
    float loss = 1.5f;
    float loss_scale = trainer.getNextLossScale();
    
    assert(loss_scale > 0);
    
    // Simulate training loop
    for (int step = 0; step < 10; step++) {
        float scaled_loss = loss * loss_scale;
        trainer.recordOverflow(false);  // No overflow
    }
    
    int mem_savings = trainer.getMemorySavingsPercent();
    assert(mem_savings > 40 && mem_savings < 60);  // Should be ~50%
    
    std::cout << "Mixed precision memory savings: " << mem_savings << "%\n";
    std::cout << "✓ Mixed precision trainer passed\n\n";
}

void test_gradient_accumulator() {
    std::cout << "Testing gradient accumulator...\n";
    
    const int numParams = 1000;
    const int accumSteps = 4;
    
    GradientAccumulator accumulator(numParams, accumSteps);
    
    // Simulate gradient accumulation
    for (int step = 0; step < accumSteps; step++) {
        std::vector<float> gradients(numParams, 0.1f * (step + 1));
        accumulator.accumulateGradients(gradients.data(), numParams);
    }
    
    assert(accumulator.isReadyForUpdate());
    
    auto accumulated = accumulator.getAccumulatedGradients();
    
    // Average should be 0.1 * (1 + 2 + 3 + 4) / 4 = 0.25
    for (int i = 0; i < 10; i++) {
        assert(std::abs(accumulated[i] - 0.25f) < 0.01f);
    }
    
    int effective_batch = accumulator.getEffectiveBatchSize(32);
    assert(effective_batch == 32 * accumSteps);
    
    std::cout << "Effective batch size: " << effective_batch << "\n";
    std::cout << "✓ Gradient accumulator passed\n\n";
}

void test_adaptive_scheduler() {
    std::cout << "Testing adaptive scheduler...\n";
    
    auto profile = HardwareDetector::detectHardware();
    
    TrainingSchedule schedule = AdaptiveScheduler::optimize(
        profile,
        100000000,  // 100M parameters
        4096,       // sequence length
        1000000,    // dataset size
        120         // target 2 min per epoch
    );
    
    assert(schedule.batchSize > 0);
    assert(schedule.batchSize <= 256);  // Reasonable limit
    assert(schedule.accumSteps >= 1);
    assert(schedule.learningRate > 0);
    assert(schedule.estimatedEpochTimeMins > 0);
    
    std::cout << "Optimized schedule:\n";
    std::cout << "  Batch size: " << schedule.batchSize << "\n";
    std::cout << "  Accumulation steps: " << schedule.accumSteps << "\n";
    std::cout << "  Learning rate: " << schedule.learningRate << "\n";
    std::cout << "  Estimated epoch time: " << schedule.estimatedEpochTimeMins << " mins\n";
    std::cout << "  Mixed precision: " << (schedule.useMixedPrecision ? "Yes" : "No") << "\n";
    
    std::cout << "✓ Adaptive scheduler passed\n\n";
}

void test_training_optimizer_orchestration() {
    std::cout << "Testing training optimizer orchestration...\n";
    
    TrainingOptimizer optimizer;
    optimizer.detectHardware();
    
    // Generate recommendations
    auto recs = optimizer.getRecommendations();
    
    assert(recs.timeReductionPercent >= 0);
    assert(recs.timeReductionPercent <= 100);
    assert(recs.recommendations.size() > 0);
    
    std::cout << "Expected time reduction: " << recs.timeReductionPercent << "%\n";
    std::cout << "Recommendations: " << recs.recommendations.size() << "\n";
    for (size_t i = 0; i < std::min(size_t(3), recs.recommendations.size()); i++) {
        std::cout << "  - " << recs.recommendations[i] << "\n";
    }
    
    std::cout << "✓ Training optimizer orchestration passed\n\n";
}

int main() {
    std::cout << "========== Training Optimizer Test Suite ==========\n\n";
    
    try {
        test_hardware_detection();
        test_training_profiler();
        test_simd_optimizer();
        test_mixed_precision_trainer();
        test_gradient_accumulator();
        test_adaptive_scheduler();
        test_training_optimizer_orchestration();
        
        std::cout << "========== All Tests Passed! ==========\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
