#include <gtest/gtest.h>
#include "../include/vulkan_compute_stub.hpp"
#include <vector>
#include <thread>
#include <chrono>

/**
 * @class VulkanComputeTest
 * @brief Unit tests for VulkanCompute GPU memory management
 * 
 * Tests cover:
 * - Initialization and cleanup
 * - Tensor allocation with bounds checking
 * - Data upload/download
 * - Memory tracking
 * - Error conditions
 */

class VulkanComputeTest : public ::testing::Test {
protected:
    VulkanCompute compute;
    
    void SetUp() override {
        // Initialize GPU before each test
        ASSERT_TRUE(compute.Initialize());
    }
    
    void TearDown() override {
        // Cleanup after each test
        compute.Cleanup();
    }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST(VulkanComputeInitTest, InitializeSuccess) {
    VulkanCompute gpu;
    bool result = gpu.Initialize();
    EXPECT_TRUE(result);
    EXPECT_TRUE(gpu.IsInitialized());
    gpu.Cleanup();
}

TEST(VulkanComputeInitTest, CleanupResets) {
    VulkanCompute gpu;
    ASSERT_TRUE(gpu.Initialize());
    gpu.Cleanup();
    EXPECT_FALSE(gpu.IsInitialized());
}

TEST(VulkanComputeInitTest, GetMemoryAfterInit) {
    VulkanCompute gpu;
    ASSERT_TRUE(gpu.Initialize());
    
    EXPECT_EQ(gpu.GetMemoryUsed(), 0);
    EXPECT_GT(gpu.GetMaxMemory(), 0);
    EXPECT_EQ(gpu.GetMaxMemory(), 2048 * 1024 * 1024); // 2GB default
    
    gpu.Cleanup();
}

// ============================================================================
// Tensor Allocation Tests
// ============================================================================

TEST_F(VulkanComputeTest, AllocateTensorSmall) {
    bool result = compute.AllocateTensor("small_tensor", 1024);
    EXPECT_TRUE(result);
    EXPECT_EQ(compute.GetMemoryUsed(), 1024);
}

TEST_F(VulkanComputeTest, AllocateTensorMedium) {
    size_t size = 10 * 1024 * 1024; // 10MB
    bool result = compute.AllocateTensor("medium_tensor", size);
    EXPECT_TRUE(result);
    EXPECT_EQ(compute.GetMemoryUsed(), size);
}

TEST_F(VulkanComputeTest, AllocateTensorMultiple) {
    size_t size1 = 1024 * 1024; // 1MB
    size_t size2 = 2 * 1024 * 1024; // 2MB
    
    ASSERT_TRUE(compute.AllocateTensor("tensor1", size1));
    ASSERT_TRUE(compute.AllocateTensor("tensor2", size2));
    
    EXPECT_EQ(compute.GetMemoryUsed(), size1 + size2);
}

TEST_F(VulkanComputeTest, AllocateTensorExceedsLimit) {
    // Try to allocate more than 2GB
    size_t huge_size = 3ul * 1024 * 1024 * 1024;
    
    bool result = compute.AllocateTensor("huge_tensor", huge_size);
    EXPECT_FALSE(result);
    EXPECT_EQ(compute.GetMemoryUsed(), 0); // No memory used
}

TEST_F(VulkanComputeTest, AllocateTensorPartialFull) {
    // Allocate 1.5GB, then try to allocate 1.5GB more
    size_t size1 = 1536 * 1024 * 1024; // 1.5GB
    size_t size2 = 1536 * 1024 * 1024; // 1.5GB
    
    ASSERT_TRUE(compute.AllocateTensor("tensor1", size1));
    
    // Second allocation should fail (would exceed 2GB)
    bool result = compute.AllocateTensor("tensor2", size2);
    EXPECT_FALSE(result);
    EXPECT_EQ(compute.GetMemoryUsed(), size1);
}

TEST_F(VulkanComputeTest, AllocateTensorDuplicate) {
    // Allocate same tensor name twice
    compute.AllocateTensor("dup_tensor", 1024);
    bool result = compute.AllocateTensor("dup_tensor", 2048);
    
    // Behavior depends on implementation (may overwrite or fail)
    // Test documents actual behavior
    EXPECT_GT(compute.GetMemoryUsed(), 0);
}

// ============================================================================
// Data Transfer Tests
// ============================================================================

TEST_F(VulkanComputeTest, UploadTensorData) {
    size_t size = 256;
    ASSERT_TRUE(compute.AllocateTensor("upload_test", size));
    
    std::vector<uint8_t> data(size, 0xAB);
    bool result = compute.UploadTensor("upload_test", data.data(), size);
    
    EXPECT_TRUE(result);
}

TEST_F(VulkanComputeTest, UploadTensorWrongSize) {
    size_t allocated = 256;
    size_t upload_size = 512; // Wrong size
    
    ASSERT_TRUE(compute.AllocateTensor("size_test", allocated));
    
    std::vector<uint8_t> data(upload_size);
    bool result = compute.UploadTensor("size_test", data.data(), upload_size);
    
    EXPECT_FALSE(result); // Should fail due to size mismatch
}

TEST_F(VulkanComputeTest, UploadTensorNotFound) {
    std::vector<uint8_t> data(256, 0xAB);
    bool result = compute.UploadTensor("nonexistent", data.data(), 256);
    
    EXPECT_FALSE(result);
}

TEST_F(VulkanComputeTest, DownloadTensorData) {
    size_t size = 256;
    ASSERT_TRUE(compute.AllocateTensor("download_test", size));
    
    // Upload data
    std::vector<uint8_t> upload_data(size, 0xCD);
    ASSERT_TRUE(compute.UploadTensor("download_test", upload_data.data(), size));
    
    // Download data
    std::vector<uint8_t> download_data(size);
    bool result = compute.DownloadTensor("download_test", download_data.data(), size);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(upload_data, download_data);
}

TEST_F(VulkanComputeTest, DownloadTensorNotFound) {
    std::vector<uint8_t> data(256);
    bool result = compute.DownloadTensor("nonexistent", data.data(), 256);
    
    EXPECT_FALSE(result);
}

TEST_F(VulkanComputeTest, RoundTripData) {
    // Test upload then download with various patterns
    std::vector<uint8_t> patterns = {0x00, 0xFF, 0xAA, 0x55};
    
    for (uint8_t pattern : patterns) {
        size_t size = 512;
        std::string tensor_name = "pattern_" + std::to_string(pattern);
        
        ASSERT_TRUE(compute.AllocateTensor(tensor_name, size));
        
        std::vector<uint8_t> original(size, pattern);
        ASSERT_TRUE(compute.UploadTensor(tensor_name, original.data(), size));
        
        std::vector<uint8_t> retrieved(size);
        ASSERT_TRUE(compute.DownloadTensor(tensor_name, retrieved.data(), size));
        
        EXPECT_EQ(original, retrieved);
    }
}

// ============================================================================
// Cleanup Tests
// ============================================================================

TEST_F(VulkanComputeTest, ReleaseTensorsClears) {
    ASSERT_TRUE(compute.AllocateTensor("tensor1", 1024));
    ASSERT_TRUE(compute.AllocateTensor("tensor2", 2048));
    
    EXPECT_GT(compute.GetMemoryUsed(), 0);
    
    compute.ReleaseTensors();
    
    EXPECT_EQ(compute.GetMemoryUsed(), 0);
}

TEST_F(VulkanComputeTest, ReleaseTensorsAllowsRealloc) {
    ASSERT_TRUE(compute.AllocateTensor("tensor", 1024));
    compute.ReleaseTensors();
    
    // Should be able to allocate again
    bool result = compute.AllocateTensor("tensor", 2048);
    EXPECT_TRUE(result);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(VulkanComputeTest, AllocationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();
    
    compute.AllocateTensor("perf_test", 1024);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete within 100 microseconds
    EXPECT_LT(duration.count(), 100);
}

TEST_F(VulkanComputeTest, UploadPerformance) {
    size_t size = 10 * 1024 * 1024; // 10MB
    ASSERT_TRUE(compute.AllocateTensor("upload_perf", size));
    
    std::vector<uint8_t> data(size);
    
    auto start = std::chrono::high_resolution_clock::now();
    compute.UploadTensor("upload_perf", data.data(), size);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within 50ms for 10MB
    EXPECT_LT(duration.count(), 50);
}

TEST_F(VulkanComputeTest, DownloadPerformance) {
    size_t size = 10 * 1024 * 1024; // 10MB
    ASSERT_TRUE(compute.AllocateTensor("download_perf", size));
    
    std::vector<uint8_t> upload_data(size, 0xAA);
    ASSERT_TRUE(compute.UploadTensor("download_perf", upload_data.data(), size));
    
    std::vector<uint8_t> download_data(size);
    
    auto start = std::chrono::high_resolution_clock::now();
    compute.DownloadTensor("download_perf", download_data.data(), size);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within 50ms for 10MB
    EXPECT_LT(duration.count(), 50);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(VulkanComputeTest, AllocateMaxMemory) {
    size_t max_mem = compute.GetMaxMemory();
    
    // Allocate close to maximum
    size_t alloc_size = max_mem - (10 * 1024 * 1024); // Leave 10MB buffer
    bool result = compute.AllocateTensor("almost_full", alloc_size);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(compute.GetMemoryUsed(), alloc_size);
}

TEST_F(VulkanComputeTest, AllocateFragmented) {
    // Allocate many small tensors
    for (int i = 0; i < 100; ++i) {
        std::string name = "fragment_" + std::to_string(i);
        size_t size = 10 * 1024; // 10KB each
        
        bool result = compute.AllocateTensor(name, size);
        EXPECT_TRUE(result);
    }
    
    EXPECT_EQ(compute.GetMemoryUsed(), 100 * 10 * 1024);
}

TEST_F(VulkanComputeTest, LargeTransfer) {
    size_t size = 100 * 1024 * 1024; // 100MB
    if (compute.GetMaxMemory() >= size) {
        ASSERT_TRUE(compute.AllocateTensor("large", size));
        
        std::vector<uint8_t> data(size, 0x42);
        bool upload = compute.UploadTensor("large", data.data(), size);
        EXPECT_TRUE(upload);
        
        std::vector<uint8_t> retrieved(size);
        bool download = compute.DownloadTensor("large", retrieved.data(), size);
        EXPECT_TRUE(download);
    }
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(VulkanComputeTest, AllocateZeroSize) {
    bool result = compute.AllocateTensor("zero", 0);
    // Behavior depends on implementation
    // Document whether this is allowed or rejected
}

TEST_F(VulkanComputeTest, EmptyTensorName) {
    bool result = compute.AllocateTensor("", 1024);
    // Test documents behavior with empty name
}

TEST_F(VulkanComputeTest, VeryLongTensorName) {
    std::string long_name(1000, 'a');
    bool result = compute.AllocateTensor(long_name, 1024);
    // Test documents behavior with very long names
}

TEST_F(VulkanComputeTest, NullPointerUpload) {
    ASSERT_TRUE(compute.AllocateTensor("null_test", 256));
    
    bool result = compute.UploadTensor("null_test", nullptr, 256);
    EXPECT_FALSE(result); // Should reject null pointer
}

TEST_F(VulkanComputeTest, NullPointerDownload) {
    ASSERT_TRUE(compute.AllocateTensor("null_test2", 256));
    
    bool result = compute.DownloadTensor("null_test2", nullptr, 256);
    EXPECT_FALSE(result); // Should reject null pointer
}

// ============================================================================
// Test Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
