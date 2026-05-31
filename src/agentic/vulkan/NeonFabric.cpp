#include "NeonFabric.hpp"

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif
#include <cstring>
#include <iostream>

namespace RawrXD::Agentic::Vulkan {

// Static members
FabricConfig NeonFabric::s_config{};
FabricControlBlock* NeonFabric::s_controlBlock = nullptr;
std::vector<void*> NeonFabric::s_mappedShards;
std::vector<VulkanContext> NeonFabric::s_vulkanContexts;
bool NeonFabric::s_initialized = false;
#ifdef _WIN32
HANDLE NeonFabric::s_hMapFile = nullptr;
#endif

bool NeonFabric::initialize(const FabricConfig& config) {
    if (s_initialized) {
        return true;
    }

    s_config = config;
    
    // 1. Create shared memory for control block
#ifdef _WIN32
    HANDLE hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
        0, sizeof(FabricControlBlock), L"RawrXD_NeonFabric_ControlBlock");
    if (!hMapFile) {
        std::cerr << "[NeonFabric] Failed to create shared memory: " << GetLastError() << std::endl;
        return false;
    }
    s_controlBlock = static_cast<FabricControlBlock*>(
        MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(FabricControlBlock)));
    if (!s_controlBlock) {
        std::cerr << "[NeonFabric] Failed to map control block: " << GetLastError() << std::endl;
        CloseHandle(hMapFile);
        return false;
    }
    s_hMapFile = hMapFile;  // Store handle for cleanup in shutdown()
#else
    int fd = shm_open("/rawrxd_neonfabric_cb", O_CREAT | O_RDWR, 0666);
    if (fd < 0) return false;
    ftruncate(fd, sizeof(FabricControlBlock));
    s_controlBlock = static_cast<FabricControlBlock*>(
        mmap(nullptr, sizeof(FabricControlBlock), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    close(fd);
    if (s_controlBlock == MAP_FAILED) { s_controlBlock = nullptr; return false; }
#endif

    // Initialize control block
    memset(s_controlBlock, 0, sizeof(FabricControlBlock));
    s_controlBlock->magic = 0x4E454F4E;  // 'NEON'
    s_controlBlock->version = 1;
    s_controlBlock->processId = static_cast<uint32_t>(GetCurrentProcessId());
    s_controlBlock->processCount = 1;
    s_controlBlock->shardCount = config.maxShards;
    s_controlBlock->readyBarrier = 0;
    s_controlBlock->barrierTarget = 1;

    // 2. Initialize Vulkan contexts for each shard
    if (config.enableVulkan) {
        s_vulkanContexts.reserve(config.maxShards);
        for (uint32_t i = 0; i < config.maxShards; ++i) {
            VulkanContext ctx{};
            if (VulkanManager::createContext(ctx, config.shardSize)) {
                s_vulkanContexts.push_back(ctx);
                s_controlBlock->shardAssignments[i] = s_controlBlock->processId;
                std::cout << "[NeonFabric] Vulkan context created for shard " << i << std::endl;
            } else {
                std::cerr << "[NeonFabric] WARNING: Failed to create Vulkan context for shard " << i << std::endl;
                s_vulkanContexts.push_back(VulkanContext{});  // Empty placeholder
            }
        }
        s_controlBlock->vulkanContextReady = 1;
    }

    // 3. Pre-allocate shard mapping slots
    s_mappedShards.resize(config.maxShards, nullptr);

    std::cout << "[NeonFabric] Initialized with " << config.maxShards << " shards, "
              << "Vulkan=" << (config.enableVulkan ? "ON" : "OFF") << std::endl;
    
    s_initialized = true;
    return true;
}

void NeonFabric::shutdown() {
    if (!s_initialized) {
        return;
    }

    // Unmap all shards
    for (uint32_t i = 0; i < s_mappedShards.size(); ++i) {
        unmapShard(i);
    }
    
    // Cleanup Vulkan contexts
    for (auto& ctx : s_vulkanContexts) {
        VulkanManager::destroyContext(ctx);
    }
    s_vulkanContexts.clear();

    // Release shared memory control block
    if (s_controlBlock) {
#ifdef _WIN32
        UnmapViewOfFile(s_controlBlock);
        if (s_hMapFile) {
            CloseHandle(s_hMapFile);
            s_hMapFile = nullptr;
        }
#else
        munmap(s_controlBlock, sizeof(FabricControlBlock));
        shm_unlink("/rawrxd_neonfabric_cb");
#endif
        s_controlBlock = nullptr;
    }
    
    std::cout << "[NeonFabric] Shutdown complete" << std::endl;
    s_initialized = false;
}

void* NeonFabric::mapShard(uint32_t shardId) {
    if (shardId >= s_config.maxShards) {
        return nullptr;
    }

    // Map shard memory region at computed address
    uintptr_t shardAddr = s_config.baseAddress + (static_cast<uint64_t>(shardId) * s_config.shardSize);

#ifdef _WIN32
    void* mapped = VirtualAlloc(
        reinterpret_cast<void*>(shardAddr), s_config.shardSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!mapped) {
        // Fallback: allocate at any address
        mapped = VirtualAlloc(nullptr, s_config.shardSize,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
#else
    void* mapped = mmap(
        reinterpret_cast<void*>(shardAddr), s_config.shardSize,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) mapped = nullptr;
#endif

    if (!mapped) {
        std::cerr << "[NeonFabric] Failed to map shard " << shardId << std::endl;
        return nullptr;
    }

    // Register with Vulkan for GPU access if available
    if (s_config.enableVulkan && shardId < s_vulkanContexts.size()) {
        VulkanManager::registerHostMemory(s_vulkanContexts[shardId], mapped, s_config.shardSize);
    }

    s_mappedShards[shardId] = mapped;
    if (s_controlBlock) {
        s_controlBlock->shardBaseAddresses[shardId] = reinterpret_cast<uint64_t>(mapped);
    }

    std::cout << "[NeonFabric] Mapped shard " << shardId << " at " << mapped << std::endl;
    return mapped;
}

bool NeonFabric::unmapShard(uint32_t shardId) {
    if (shardId >= s_mappedShards.size()) {
        return false;
    }
    
    // Unmap shard memory
    void* shardPtr = s_mappedShards[shardId];
    if (!shardPtr) return true;  // Already unmapped

    // Deregister from Vulkan
    if (s_config.enableVulkan && shardId < s_vulkanContexts.size()) {
        VulkanManager::unregisterHostMemory(s_vulkanContexts[shardId], shardPtr);
    }

#ifdef _WIN32
    VirtualFree(shardPtr, 0, MEM_RELEASE);
#else
    munmap(shardPtr, s_config.shardSize);
#endif

    s_mappedShards[shardId] = nullptr;
    if (s_controlBlock) {
        s_controlBlock->shardBaseAddresses[shardId] = 0;
    }
    
    return true;
}

FabricControlBlock* NeonFabric::getControlBlock() {
    return s_controlBlock;
}

bool NeonFabric::synchronize() {
    if (!s_controlBlock) {
        return false;
    }
    
    // Barrier synchronization: atomic increment + spin-wait until all processes arrive
#ifdef _WIN32
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(&s_controlBlock->readyBarrier));

    // Spin-wait until all processes have hit the barrier
    uint32_t spinCount = 0;
    const uint32_t maxSpins = 10000000;  // ~10 second timeout at typical spin rates
    while (s_controlBlock->readyBarrier < s_controlBlock->barrierTarget) {
        if (++spinCount > maxSpins) {
            std::cerr << "[NeonFabric] Barrier synchronization timed out" << std::endl;
            return false;
        }
        YieldProcessor();
    }
#else
    __sync_fetch_and_add(&s_controlBlock->readyBarrier, 1);
    while (s_controlBlock->readyBarrier < s_controlBlock->barrierTarget) {
        __builtin_ia32_pause();
    }
#endif

    // Reset barrier for next sync cycle
    s_controlBlock->readyBarrier = 0;
    return true;
}

bool NeonFabric::broadcastBitmask(const void* bitmask, size_t size) {
    if (!s_initialized || !s_config.enableVulkan) {
        return false;
    }

    // Update bitmask in all Vulkan contexts
    for (auto& vkContext : s_vulkanContexts) {
        if (!VulkanManager::updateBitmask(vkContext, bitmask, size)) {
            return false;
        }
    }
    
    return true;
}

VulkanContext* NeonFabric::getVulkanContext(uint32_t shardId) {
    if (shardId >= s_vulkanContexts.size()) {
        return nullptr;
    }
    return &s_vulkanContexts[shardId];
}

bool NeonFabric::isInitialized() {
    return s_initialized;
}

uint32_t NeonFabric::getShardCount() {
    return static_cast<uint32_t>(s_mappedShards.size());
}

} // namespace RawrXD::Agentic::Vulkan

