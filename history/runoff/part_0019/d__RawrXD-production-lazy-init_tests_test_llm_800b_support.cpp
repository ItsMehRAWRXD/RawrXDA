#include "../include/llm_800b_support.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace LLM800B;

void test_model_800b_config() {
    std::cout << "Testing 800B model configuration...\n";
    
    Model800BConfig config;
    
    // Verify 800B parameter configuration
    int total_params = config.numLayers * config.hiddenSize * 
                       (config.hiddenSize +  // self-attention
                        config.hiddenSize * 4);  // FFN (simplified)
    
    std::cout << "Model configuration:\n";
    std::cout << "  Layers: " << config.numLayers << "\n";
    std::cout << "  Hidden size: " << config.hiddenSize << "\n";
    std::cout << "  Heads: " << config.numHeads << "\n";
    std::cout << "  Head dim: " << (config.hiddenSize / config.numHeads) << "\n";
    std::cout << "  FFN size: " << config.ffnSize << "\n";
    
    assert(config.numLayers == 80);
    assert(config.hiddenSize == 7168);
    assert(config.numHeads == 56);
    
    std::cout << "✓ Model 800B configuration passed\n\n";
}

void test_streaming_inference_engine() {
    std::cout << "Testing streaming inference engine...\n";
    
    Model800BConfig config;
    config.vocabSize = 32000;
    config.hiddenSize = 1024;  // Smaller for testing
    config.numLayers = 4;
    config.numHeads = 8;
    
    StreamingInferenceEngine engine(config);
    
    // Test initialization
    std::cout << "Streaming engine initialized for config:\n";
    std::cout << "  Vocab size: " << config.vocabSize << "\n";
    std::cout << "  Hidden size: " << config.hiddenSize << "\n";
    std::cout << "  Layers: " << config.numLayers << "\n";
    
    // Simulate token generation
    std::vector<int> tokens = {1, 2, 3};
    
    for (int i = 0; i < 5; i++) {
        auto next = engine.generateNextToken(tokens, 0.8f, 0.9f);
        
        assert(next.tokenId >= 0 && next.tokenId < config.vocabSize);
        assert(next.probability >= 0.0f && next.probability <= 1.0f);
        
        tokens.push_back(next.tokenId);
    }
    
    double speed = engine.getInferenceSpeed();
    std::cout << "Inference speed: " << speed << " tokens/sec\n";
    assert(speed >= 0);
    
    long long mem = engine.getCurrentMemoryUsage();
    std::cout << "Memory usage: " << (mem / 1e6) << " MB\n";
    assert(mem > 0);
    
    std::cout << "✓ Streaming inference engine passed\n\n";
}

void test_model_sharding() {
    std::cout << "Testing model sharding...\n";
    
    const long long total_params = 800000000000LL;  // 800B
    const int num_devices = 8;
    
    ModelShardingManager sharding(
        total_params,
        num_devices,
        ModelShardingManager::ShardStrategy::PIPELINE_PARALLEL
    );
    
    // Test layer range distribution
    for (int device_id = 0; device_id < num_devices; device_id++) {
        auto layer_range = sharding.getLayerRange(device_id);
        std::cout << "Device " << device_id << " handles layers " 
                  << layer_range.first << "-" << layer_range.second << "\n";
        
        assert(layer_range.first >= 0);
        assert(layer_range.second > layer_range.first);
    }
    
    // Test communication cost estimation
    double comm_time = sharding.estimateCommunicationCost(
        1000000,  // 1M float32s
        600       // 600 GB/s for NVLink
    );
    
    std::cout << "Communication time: " << comm_time << " ms\n";
    assert(comm_time > 0);
    
    std::cout << "✓ Model sharding passed\n\n";
}

void test_kv_cache_manager() {
    std::cout << "Testing KV cache manager...\n";
    
    KVCacheManager cache(
        4096,   // max sequence length
        1024,   // hidden size
        8       // num heads
    );
    
    // Simulate caching tokens
    for (int pos = 0; pos < 100; pos++) {
        std::vector<float> key(1024, 0.5f);
        std::vector<float> value(1024, 0.3f);
        
        cache.storeKV(pos, key, value);
    }
    
    long long mem = cache.getMemoryUsage();
    std::cout << "KV cache memory: " << (mem / 1e6) << " MB\n";
    assert(mem > 0);
    
    // Test compression
    cache.compress(0.5f);  // 50% compression
    
    long long compressed_mem = cache.getMemoryUsage();
    std::cout << "After compression: " << (compressed_mem / 1e6) << " MB\n";
    assert(compressed_mem < mem);
    
    std::cout << "✓ KV cache manager passed\n\n";
}

void test_speculative_decoding() {
    std::cout << "Testing speculative decoding...\n";
    
    Model800BConfig large_config;
    large_config.hiddenSize = 2048;
    
    Model800BConfig small_config = large_config;
    small_config.numLayers = 16;  // Small draft model
    
    SpeculativeDecoding spec(large_config, small_config);
    
    std::vector<int> tokens = {1, 2, 3};
    
    // Test speculative sampling
    auto result = spec.speculativeSample(tokens, 4, 1.0f);
    
    float speedup = spec.getSpeedupFactor();
    std::cout << "Speculative decoding speedup: " << speedup << "x\n";
    
    // With 4 draft tokens and 70% acceptance: speedup ~= 1 + 4*0.7 = 3.8x
    assert(speedup > 1.0f && speedup < 5.0f);
    
    std::cout << "✓ Speculative decoding passed\n\n";
}

void test_large_model_quantization() {
    std::cout << "Testing large model quantization...\n";
    
    // Test different quantization levels
    std::vector<LargeModelQuantization::QuantizationLevel> levels = {
        LargeModelQuantization::QuantizationLevel::Q8_0,
        LargeModelQuantization::QuantizationLevel::Q4_0,
        LargeModelQuantization::QuantizationLevel::Q4_1,
        LargeModelQuantization::QuantizationLevel::INT3
    };
    
    for (auto level : levels) {
        LargeModelQuantization quant(level);
        
        // Create test weights
        std::vector<float> weights(1024, 1.5f);
        std::vector<float> scales, zeros;
        
        auto quantized = quant.quantizeWeights(weights, scales, zeros);
        
        double ratio = quant.getCompressionRatio();
        double loss = quant.getAccuracyLoss() * 100;
        
        std::cout << "Quantization level " << static_cast<int>(level) << ":\n";
        std::cout << "  Compression: " << ratio << "x\n";
        std::cout << "  Accuracy loss: " << loss << "%\n";
        
        assert(ratio > 1.0 && ratio <= 12.0);
        assert(loss >= 0.0 && loss <= 15.0);
    }
    
    std::cout << "✓ Large model quantization passed\n\n";
}

void test_distributed_training_engine() {
    std::cout << "Testing distributed training engine...\n";
    
    Model800BConfig config;
    DistributedTrainingEngine engine(config, 8);  // 8 GPUs
    
    // Initialize training
    engine.setDeviceId(0);
    engine.setGlobalBatchSize(256);
    engine.setLearningRate(0.001f);
    
    // Simulate training step
    std::vector<float> gradients(1000, 0.01f);
    engine.optimizerStep(gradients.data(), gradients.size());
    
    // Check metrics
    double loss = engine.getCurrentLoss();
    double throughput = engine.getThroughput();
    
    std::cout << "Training metrics:\n";
    std::cout << "  Current loss: " << loss << "\n";
    std::cout << "  Throughput: " << throughput << " tokens/sec\n";
    
    assert(loss > 0);
    assert(throughput >= 0);
    
    std::cout << "✓ Distributed training engine passed\n\n";
}

void test_large_model_inference() {
    std::cout << "Testing large model inference utilities...\n";
    
    Model800BConfig config;
    config.numLayers = 80;
    config.hiddenSize = 7168;
    
    LargeModelInference inference;
    
    // Calculate memory requirements
    long long model_mem = inference.calculateMemoryNeeded(
        800000000000LL,  // 800B params
        1.0,             // fp32 scale
        true             // include gradients
    );
    
    std::cout << "Memory needed for 800B model:\n";
    std::cout << "  Model + gradients: " << (model_mem / 1e9) << " GB\n";
    
    assert(model_mem > 1e9);  // At least 1GB
    
    // Test auto-tuning
    inference.autoTune(8, 600);  // 8 GPUs, 600 GB/s bandwidth
    
    std::cout << "✓ Large model inference passed\n\n";
}

int main() {
    std::cout << "========== LLM 800B Support Test Suite ==========\n\n";
    
    try {
        test_model_800b_config();
        test_streaming_inference_engine();
        test_model_sharding();
        test_kv_cache_manager();
        test_speculative_decoding();
        test_large_model_quantization();
        test_distributed_training_engine();
        test_large_model_inference();
        
        std::cout << "========== All Tests Passed! ==========\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
