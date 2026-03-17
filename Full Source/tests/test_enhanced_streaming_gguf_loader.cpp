#include "streaming_gguf_loader_enhanced.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <iostream>
#include <chrono>

#include "logging/logger.h"
static Logger s_logger("test_enhanced_streaming_gguf_loader");

class EnhancedStreamingGGUFLoaderTest : public ::testing::Test {
protected:
    EnhancedStreamingGGUFLoader loader;
    std::filesystem::path test_model;
    
    void SetUp() override {
        // Use same synthetic GGUF from base test
        test_model = std::filesystem::temp_directory_path() / "test_model_enhanced.gguf";
    }
    
    void TearDown() override {
        if (std::filesystem::exists(test_model)) {
            std::filesystem::remove(test_model);
        }
    }
};

// ============================================================================
// PREDICTIVE CACHING TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, PredictiveCache_SequentialAccess)
{
    // Test sequential access pattern detection
    loader.UpdateAccessPattern(0);
    loader.UpdateAccessPattern(1);
    loader.UpdateAccessPattern(2);
    
    // Should predict zones 3, 4, 5, ...
    auto predictions = loader.PredictNextZones(5);
    EXPECT_GE(predictions.size(), 1);
    EXPECT_EQ(predictions[0], 3);  // Expected next zone
    
    // Confidence should be high for sequential pattern
    float conf = loader.GetPredictionConfidence(0);
    EXPECT_GT(conf, 0.5f);
}

TEST_F(EnhancedStreamingGGUFLoaderTest, PredictiveCache_StrideAccess)
{
    // Test strided access pattern (e.g., 0, 2, 4, 6)
    loader.UpdateAccessPattern(0);
    loader.UpdateAccessPattern(2);
    loader.UpdateAccessPattern(4);
    
    auto predictions = loader.PredictNextZones(3);
    
    // Should predict stride of 2
    EXPECT_GE(predictions.size(), 1);
    // Confidence lower than sequential but still predictable
    float conf = loader.GetPredictionConfidence(0);
    EXPECT_GT(conf, 0.2f);
}

TEST_F(EnhancedStreamingGGUFLoaderTest, PredictiveCache_RandomAccess)
{
    // Test random access pattern
    loader.UpdateAccessPattern(5);
    loader.UpdateAccessPattern(12);
    loader.UpdateAccessPattern(3);
    
    // Confidence should be low
    float conf = loader.GetPredictionConfidence(5);
    EXPECT_LT(conf, 0.5f);
}

TEST_F(EnhancedStreamingGGUFLoaderTest, AccessFrequency_Tracking)
{
    // Test access frequency tracking
    loader.UpdateAccessPattern(7);
    loader.UpdateAccessPattern(7);
    loader.UpdateAccessPattern(7);
    
    uint32_t freq = loader.GetAccessFrequency(7);
    EXPECT_EQ(freq, 3);
}

// ============================================================================
// PREFETCHING TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, Prefetching_Async)
{
    // Test async prefetch queueing
    bool result = loader.PrefetchZoneAsync(10);
    EXPECT_TRUE(result);
    
    auto prefetching = loader.GetPrefetchingZones();
    EXPECT_GE(prefetching.size(), 1);
}

TEST_F(EnhancedStreamingGGUFLoaderTest, Prefetching_MultipleZones)
{
    // Queue multiple prefetches
    loader.PrefetchZoneAsync(0);
    loader.PrefetchZoneAsync(1);
    loader.PrefetchZoneAsync(2);
    
    auto prefetching = loader.GetPrefetchingZones();
    EXPECT_GE(prefetching.size(), 3);
}

// ============================================================================
// HUGE PAGES TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, HugePages_Allocation)
{
    // Test huge page pool allocation
    bool result = loader.AllocateHugePages(512);  // 512MB
    EXPECT_TRUE(result);
    
    // Verify pool initialized
    auto usage = loader.GetHugePageUsage();
    EXPECT_GE(usage, 0);
}

TEST_F(EnhancedStreamingGGUFLoaderTest, HugePages_AllocateFromPool)
{
    loader.AllocateHugePages(256);
    
    // Allocate memory from pool
    void* ptr1 = loader.AllocateHugePage(1024 * 1024);  // 1MB
    EXPECT_NE(ptr1, nullptr);
    
    void* ptr2 = loader.AllocateHugePage(2097152);  // 2MB
    EXPECT_NE(ptr2, nullptr);
    
    // Pointers should be different
    EXPECT_NE(ptr1, ptr2);
}

// ============================================================================
// COMPUTE DEVICE DETECTION TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, ComputeDevices_Detection)
{
    int device_count = loader.DetectComputeDevices();
    EXPECT_GE(device_count, 1);  // At least CPU
}

TEST_F(EnhancedStreamingGGUFLoaderTest, ComputeDevices_Count)
{
    int count = loader.GetComputeDeviceCount();
    EXPECT_GT(count, 0);
}

// ============================================================================
// COMPRESSION TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, Compression_PreferenceSet)
{
    loader.SetCompressionPreference(2);  // Balanced
    EXPECT_EQ(loader.GetCompressionCodec(), 2);
}

// ============================================================================
// PERFORMANCE METRICS TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, Metrics_Initialization)
{
    auto metrics = loader.GetMetrics();
    EXPECT_EQ(metrics.total_tensor_loads, 0);
    EXPECT_EQ(metrics.cache_hits, 0);
    EXPECT_EQ(metrics.cache_misses, 0);
}

TEST_F(EnhancedStreamingGGUFLoaderTest, Metrics_Reset)
{
    loader.ResetMetrics();
    auto metrics = loader.GetMetrics();
    EXPECT_EQ(metrics.total_tensor_loads, 0);
}

// ============================================================================
// NVME/IORING CAPABILITY TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, NVMe_Capability)
{
    // Check if NVMe is available (may not be on all systems)
    bool result = loader.EnableNVMeDirectIO();
    
    if (result) {
        EXPECT_TRUE(loader.IsNVMeEnabled());
        loader.DisableNVMeDirectIO();
        EXPECT_FALSE(loader.IsNVMeEnabled());
    }
}

TEST_F(EnhancedStreamingGGUFLoaderTest, IOring_Capability)
{
    // Check if IORING is available (Windows 11 22H2+ only)
    bool result = loader.EnableIOring();
    
    if (result) {
        EXPECT_TRUE(loader.IsIOringEnabled());
        loader.DisableIOring();
        EXPECT_FALSE(loader.IsIOringEnabled());
    }
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, Integration_SequentialLoad)
{
    // Test typical sequential loading scenario
    loader.AllocateHugePages(256);
    
    // Simulate sequential zone access
    for (int i = 0; i < 10; ++i) {
        loader.UpdateAccessPattern(i);
    }
    
    // Verify prediction
    auto predictions = loader.PredictNextZones(3);
    EXPECT_GE(predictions.size(), 1);
}

TEST_F(EnhancedStreamingGGUFLoaderTest, Integration_PrefetchPipeline)
{
    loader.AllocateHugePages(512);
    
    // Access zone 0, triggering prefetch of zones 1-8
    loader.UpdateAccessPattern(0);
    auto predictions = loader.PredictNextZones(8);
    
    // Queue prefetch
    for (auto zone_id : predictions) {
        loader.PrefetchZoneAsync(zone_id);
    }
    
    auto prefetching = loader.GetPrefetchingZones();
    EXPECT_GE(prefetching.size(), 1);
}

// ============================================================================
// STRESS TESTS
// ============================================================================

TEST_F(EnhancedStreamingGGUFLoaderTest, Stress_RapidAccessPatternUpdates)
{
    // Simulate rapid tensor accesses (like inference loop)
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        loader.UpdateAccessPattern(i % 50);
    }
    
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
    
    s_logger.info("1000 pattern updates: ");
    
    // Should be very fast (<10ms for 1000 updates)
    EXPECT_LT(duration_ms.count(), 10);
}

TEST_F(EnhancedStreamingGGUFLoaderTest, Stress_PrefetchQueue)
{
    // Queue many prefetch operations
    for (int i = 0; i < 100; ++i) {
        loader.PrefetchZoneAsync(i % 64);
    }
    
    auto prefetching = loader.GetPrefetchingZones();
    EXPECT_GT(prefetching.size(), 0);
}

// ============================================================================
// BENCHMARK TESTS
// ============================================================================

class EnhancedLoaderBenchmark : public ::testing::Test {
protected:
    EnhancedStreamingGGUFLoader loader;
};

TEST_F(EnhancedLoaderBenchmark, Benchmark_PredictorLookup)
{
    // Measure predictor table lookup latency
    loader.UpdateAccessPattern(42);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100000; ++i) {
        auto conf = loader.GetPredictionConfidence(42);
        (void)conf;  // Use result to prevent optimization
    }
    
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
    
    double avg_us = static_cast<double>(duration_us.count()) / 100000.0;
    s_logger.info("Predictor lookup: ");
    
    // Should be sub-microsecond
    EXPECT_LT(avg_us, 1.0);
}

TEST_F(EnhancedLoaderBenchmark, Benchmark_PredictionGeneration)
{
    // Setup pattern
    for (int i = 0; i < 16; ++i) {
        loader.UpdateAccessPattern(i);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 10000; ++i) {
        auto predictions = loader.PredictNextZones(8);
        (void)predictions;
    }
    
    auto elapsed = std::chrono::high_resolution_clock::now() - start;
    auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
    
    double avg_us = static_cast<double>(duration_us.count()) / 10000.0;
    s_logger.info("Prediction generation: ");
    
    // Should be <100μs
    EXPECT_LT(avg_us, 100.0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
