#pragma once

#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <cstdint>
#include <memory>
#include <algorithm>

/**
 * @file frictionless_model_sharding.h
 * @brief Frictionless Model Artifact Sharding System
 * 
 * Automatically divides model artifacts into parameter-based shards
 * using mathematical optimization for efficient distributed inference.
 * 
 * Key Features:
 * - Automatic shard calculation based on model parameters
 * - Mathematical optimization for cluster distribution
 * - Parameter-proportional loading strategy
 * - Efficient memory management across clusters
 * - Support for models from 1M to 800B+ parameters
 */

namespace Frictionless {

/// Model size categories in billions of parameters
enum class ModelSize : int {
    TINY      = 1,      // 1B
    SMALL     = 7,      // 7B
    MEDIUM    = 13,     // 13B
    LARGE     = 33,     // 33B
    XLARGE    = 65,     // 65B
    XXL       = 120,    // 120B
    MASSIVE   = 200,    // 200B
    GIGANTIC  = 400,    // 400B
    COLOSSAL  = 800,    // 800B
};

/// Shard distribution strategy
enum class ShardStrategy {
    SEQUENTIAL,         // Load shards 1->N sequentially
    PARALLEL,           // Load all shards in parallel
    ADAPTIVE,           // Load based on available resources
    HIERARCHICAL,       // Tree-based loading pattern
};

/// Artifact shard metadata
/// Memory pool status
enum class MemoryPoolStatus {
    AVAILABLE,          // Memory available for allocation
    ALLOCATED,          // Memory currently allocated
    FRAGMENTED,         // Memory fragmented, defragmentation recommended
    DEPLETED,           // No available memory
};

/// Device resource information
struct DeviceResource {
    int device_id;
    long long total_memory;
    long long allocated_memory;
    long long available_memory;
    double utilization_percent;
    int shard_count;
    std::vector<int> shard_ids;
};

/// Memory allocation statistics
struct MemoryStats {
    long long total_allocated;
    long long total_available;
    long long peak_allocated;
    long long fragmented_bytes;
    int allocation_count;
    int deallocation_count;
    double fragmentation_ratio;
    MemoryPoolStatus status;
};
/// Memory pool allocator
class MemoryPoolAllocator {
public:
    static void initialize(long long total_memory, long long block_size = 1024);
    static void* allocate(long long size);
    static void deallocate(void* ptr);
    static long long defragment();
    static MemoryStats getMemoryStats();
    static void reset();
};

/// Artifact shard metadata
struct ArtifactShard {
    int shard_id;                    // Unique shard identifier (0 to N-1)
    long long parameter_count;       // Parameters in this shard
    long long memory_bytes;          // Memory footprint
    std::string file_path;           // Path to shard artifact
    float compression_ratio;         // Compression applied (1.0 = none)
    double estimated_load_time_ms;   // Estimated load time
    int priority;                    // Loading priority (0 = highest)
    std::vector<uint8_t> checksum;   // SHA256 checksum
    bool is_loaded;                  // Whether shard is in memory
    long long actual_load_time_ms;   // Actual load time
    void* allocated_memory_ptr;      // Pointer to allocated memory
};

/// Mathematical shard calculation
struct ShardCalculation {
    int total_shards;                // Number of shards needed
    long long shard_size_params;     // Parameters per shard
    long long shard_size_bytes;      // Bytes per shard
    std::vector<int> cluster_map;    // Which cluster each shard goes to
    double compression_factor;       // Suggested compression ratio
    int expected_load_time_seconds;  // Total expected load time
};

/**
 * @class FrictionlessShardingEngine
 * @brief Main sharding engine for model artifact division
 * 
 * Implements mathematical optimization to automatically divide
 * model artifacts into efficient shards for distributed loading.
 */
class FrictionlessShardingEngine {
public:
    static ShardCalculation calculateOptimalShards(
        long long total_params,
        long long available_memory_bytes,
        int num_devices,
        ShardStrategy strategy = ShardStrategy::ADAPTIVE
    );

    static std::vector<ArtifactShard> generateShards(
        const ShardCalculation& calc,
        const std::string& model_path
    );

    static DeviceResource getDeviceResources(int device_id);
    static void updateDeviceAllocation(int device_id, int shard_id, long long memory_bytes);

    static long long calculateBytesPerParameter(
        int precision = 4,
        double overhead_factor = 1.1
    );

    static float recommendCompressionLevel(
        long long model_params,
        long long available_memory_bytes
    );

    static std::vector<int> calculateClusterDistribution(
        int num_shards,
        int num_devices,
        const std::vector<long long>& device_memory
    );

    static double estimateTotalLoadTime(
        const std::vector<ArtifactShard>& shards,
        double bandwidth_gbps,
        int num_devices
    );

    static bool loadShards(
        std::vector<ArtifactShard>& shards,
        ShardStrategy strategy,
        int num_threads = 8
    );

    static int calculateShardPriority(int shard_id, int total_shards);
};

/**
 * @class ModelSizeCalculator
 * @brief Calculate resource requirements for model sizes
 */
class ModelSizeCalculator {
public:
    /**
     * Get parameters for well-known model size
     * 
     * @param size ModelSize enum value
     * @return Total parameters
     */
    static long long getParameterCount(ModelSize size) {
        return static_cast<long long>(size) * 1000000000;  // Convert billions to absolute
    }

     /**
      * Get human-readable model name
      * 
      * @param size ModelSize enum value
      * @return String like "7B", "13B", "800B"
      */
    static std::string getModelName(ModelSize size);

    /**
     * Estimate memory needed for inference
     * Accounts for model weights, activations, KV cache
     * 
     * @param params Total parameters
     * @param seq_length Sequence length
     * @param use_kv_cache Include KV cache in calculation
     * @return Memory in bytes
     */
    static long long estimateMemoryNeeded(
        long long params,
        int seq_length,
        bool use_kv_cache
    );

    /**
     * Estimate memory needed for training
     * Accounts for weights, gradients, optimizer state
     * 
     * @param params Total parameters
     * @param use_mixed_precision Use FP16 (reduces by ~50%)
     * @return Memory in bytes
     */
    static long long estimateTrainingMemory(
        long long params,
        bool use_mixed_precision
    );

    /**
     * Get minimum GPUs needed for model
     * 
     * @param params Total parameters
     * @param gpu_memory_gb Memory per GPU
     * @return Minimum GPUs needed
     */
    static int getMinimumGPUsNeeded(
        long long params,
        int gpu_memory_gb = 40
    );
};

/**
 * @class ShardIOManager
 * @brief Handle I/O operations for shards
 */
class ShardIOManager {
public:
    /**
     * Compute checksum for a shard
     * 
     * @param data Raw shard bytes
     * @return SHA256-like checksum bytes
     */
    static std::vector<uint8_t> computeChecksum(const std::vector<uint8_t>& data);

    /**
     * Validate checksum against expected
     * 
     * @param data Raw shard bytes
     * @param expected_checksum Expected checksum
     * @return True if checksum matches
     */
    static bool validateChecksum(const std::vector<uint8_t>& data, const std::vector<uint8_t>& expected_checksum);

    /**
     * Save shard to disk with compression
     * 
     * @param shard Shard to save
     * @param output_path Output file path
     * @param compression_level Compression (1-9, 0 = none)
     * @return True if successful
     */
    static bool saveShard(
        const ArtifactShard& shard,
        const std::string& output_path,
        int compression_level = 4
    );

    /**
     * Load shard from disk
     * 
     * @param shard_path Path to shard file
     * @return Loaded shard data (raw bytes)
     */
    static std::vector<uint8_t> loadShard(const std::string& shard_path);

    /**
     * Load shard with integrity verification
     * 
     * @param shard Shard metadata (contains checksum)
     * @return Loaded shard data, empty if verification fails
     */
    static std::vector<uint8_t> loadAndVerifyShard(const ArtifactShard& shard);

    /**
     * Verify shard integrity via checksum
     * 
     * @param shard Shard to verify
     * @return True if checksum matches
     */
    static bool verifyShard(const ArtifactShard& shard);

    /**
     * Get shard file size
     * 
     * @param shard_path Path to shard file
     * @return File size in bytes, -1 if not found
     */
    static long long getShardFileSize(const std::string& shard_path);

    /**
     * Verify shard file integrity
     * 
     * @param shard_path Path to shard file
     * @return True if file is valid and readable
     */
    static bool verifyShardFileIntegrity(const std::string& shard_path);

    /**
     * Cache frequently accessed shards
     * 
     * @param shards Shards to potentially cache
     * @param cache_memory_bytes Memory available for cache
     * @return Number of shards cached
     */
    static int cacheShards(
        std::vector<ArtifactShard>& shards,
        long long cache_memory_bytes
    );

    /**
     * Clear shard cache
     */
    static void clearShardCache();

    /**
     * Get cache statistics
     * 
     * @return Number of shards in cache
     */
    static int getCacheSize();
};

/**
 * @class ShardMetrics
 * @brief Metrics and monitoring for shard operations
 */
class ShardMetrics {
public:
    struct LoadMetrics {
        double total_time_seconds;
        double throughput_gbps;
        double compression_ratio;
        int shards_loaded;
        int shards_failed;
        long long total_bytes_transferred;
    };

    /**
     * Track shard loading operation
     */
    static void startLoadOperation();

    /**
     * Complete shard loading operation and get metrics
     * 
     * @return Metrics from operation
     */
    static LoadMetrics endLoadOperation();

    /**
     * Get average throughput across all operations
     * 
     * @return Average GB/s
     */
    static double getAverageThroughput();

    /**
     * Log shard operation to telemetry
     * 
     * @param operation_name Name of operation
     * @param duration_ms Duration in milliseconds
     * @param bytes_processed Bytes processed
     */
    static void logOperation(
        const std::string& operation_name,
        double duration_ms,
        long long bytes_processed
    );
};

}  // namespace Frictionless
