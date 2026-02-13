#pragma once

#include "VulkanManager.hpp"
#include <cstdint>
#include <vector>

namespace RawrXD::Agentic::Vulkan {

/// Fabric control block for multi-process coordination
struct FabricControlBlock {
    uint32_t magic;
    uint32_t version;
    uint32_t processCount;
    uint32_t processId;
    uint32_t shardCount;
    
    uint32_t readyBarrier;
    uint32_t barrierTarget;
    
    uint64_t shardBaseAddresses[16];
    uint32_t shardAssignments[16];
    
    // Vulkan coordination
    uint32_t vulkanContextReady;
    uint8_t vulkanDeviceUUID[16];
    uint32_t vulkanMemoryOpaqueFd;
    uint64_t vulkanMemoryHandle;
};

/// Fabric configuration for 800B model sharding
struct FabricConfig {
    uint64_t baseAddress = 0x0000070000000000ULL;
    uint64_t totalSize = 0x0000064000000000ULL;      // 400GB
    uint64_t shardSize = 0x0000002000000000ULL;      // 8GB per shard
    uint32_t maxShards = 16;
    bool enableVulkan = true;
};

/// Neon fabric manager for distributed 800B model inference
class NeonFabric {
public:
    /// Initialize fabric with config
    static bool initialize(const FabricConfig& config);
    
    /// Shutdown fabric
    static void shutdown();
    
    /// Map shard to current process
    static void* mapShard(uint32_t shardId);
    
    /// Unmap shard
    static bool unmapShard(uint32_t shardId);
    
    /// Get control block
    static FabricControlBlock* getControlBlock();
    
    /// Synchronize with other processes
    static bool synchronize();
    
    /// Broadcast bitmask to all shards via Vulkan
    static bool broadcastBitmask(const void* bitmask, size_t size);
    
    /// Get Vulkan context for shard
    static VulkanContext* getVulkanContext(uint32_t shardId);
    
    /// Check if fabric is initialized
    static bool isInitialized();
    
    /// Get shard count
    static uint32_t getShardCount();
    
private:
    static FabricConfig s_config;
    static FabricControlBlock* s_controlBlock;
    static std::vector<void*> s_mappedShards;
    static std::vector<VulkanContext> s_vulkanContexts;
    static bool s_initialized;
};

} // namespace RawrXD::Agentic::Vulkan
