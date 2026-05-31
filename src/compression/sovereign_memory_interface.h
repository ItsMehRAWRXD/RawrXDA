// ============================================================================
// RawrXD Sovereign Memory Mapping C++ Interface
// Bridge between MASM kernels and C++ inference engine
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <functional>
#include <vector>

namespace rawrxd {

// Forward declarations for MASM functions
extern "C" {
    // Memory mapper
    int32_t SovMem_Init();
    void* SovMem_MapModel(const wchar_t* path, void* desiredBase, uint64_t size);
    void SovMem_UnmapModel();
    void* SovMem_GetMappedBase();
    uint64_t SovMem_GetMapSize();
    void* SovMem_AllocateLargePage(uint64_t size);
    void SovMem_FreeLargePage(void* addr);
    
    // Spin-lock primitives
    int32_t SovSpin_Init();
    int32_t SovSpin_Acquire(uint32_t slot);
    void SovSpin_Release(uint32_t slot);
    int32_t SovSpin_TryAcquire(uint32_t slot);
    int32_t SovSpin_IsHeld(uint32_t slot);
    uint64_t SovSpin_GetOwner(uint32_t slot);
    int32_t SovMem_ReadWithLock(uint32_t slot, void* src, void* dst, uint64_t size);
    int32_t SovMem_WriteWithLock(uint32_t slot, void* dst, void* src, uint64_t size);
    
    // IQ2_M dequantization
    int32_t IQ2M_Init();
    int32_t IQ2M_DequantBlock(void* compressed, void* output);
    int32_t IQ2M_DequantLayer(void* layerData, uint64_t weightCount, void* output);
    int32_t IQ2M_GetBlockSize();
    int32_t IQ2M_GetWeightsPerBlock();
}

// ============================================================================
// Expert Gating Configuration
// ============================================================================

struct ExpertGateConfig {
    uint32_t numExperts = 8;           // Number of experts in MoE layer
    uint32_t activeExperts = 2;        // Number of experts to activate
    uint64_t expertSizeBytes = 0;      // Size of each expert's weights
    uint32_t lockSlotBase = 0;         // Starting spin-lock slot for this layer
    bool useLargePages = true;         // Use MEM_LARGE_PAGES for staging
    bool prefetchNextExpert = true;  // Prefetch next predicted expert
};

// ============================================================================
// Sovereign Memory Mapper
// ============================================================================

class SovereignMemoryMapper {
public:
    SovereignMemoryMapper();
    ~SovereignMemoryMapper();
    
    // Initialize the mapper subsystem
    bool Initialize();
    
    // Map a model file into virtual address space
    bool MapModel(const std::wstring& path, uint64_t size = 0);
    
    // Unmap current model
    void UnmapModel();
    
    // Get mapped base address
    void* GetBaseAddress() const;
    
    // Get mapped size
    uint64_t GetSize() const;
    
    // Check if a model is currently mapped
    bool IsMapped() const;
    
    // Allocate staging buffer for expert weights
    void* AllocateStagingBuffer(uint64_t size);
    
    // Free staging buffer
    void FreeStagingBuffer(void* buffer);
    
    // Read expert weights from mapped region with lock
    bool ReadExpertWeights(uint32_t expertId, void* dst, uint64_t offset, uint64_t size);
    
    // Write expert weights to mapped region with lock
    bool WriteExpertWeights(uint32_t expertId, void* src, uint64_t offset, uint64_t size);

private:
    void* m_baseAddress = nullptr;
    uint64_t m_mapSize = 0;
    bool m_initialized = false;
    std::vector<void*> m_stagingBuffers;
};

// ============================================================================
// Expert Gate Manager
// ============================================================================

class ExpertGateManager {
public:
    ExpertGateManager(SovereignMemoryMapper& mapper);
    ~ExpertGateManager();
    
    // Configure expert gating for a layer
    bool ConfigureLayer(uint32_t layerId, const ExpertGateConfig& config);
    
    // Activate specific experts for inference
    bool ActivateExperts(uint32_t layerId, const uint32_t* expertIds, uint32_t count);
    
    // Deactivate all experts for a layer
    void DeactivateExperts(uint32_t layerId);
    
    // Get pointer to active expert weights (must call ActivateExperts first)
    void* GetExpertWeights(uint32_t layerId, uint32_t expertId);
    
    // Prefetch expert weights into cache
    void PrefetchExpert(uint32_t layerId, uint32_t expertId);
    
    // Get currently active expert count
    uint32_t GetActiveExpertCount(uint32_t layerId) const;

private:
    SovereignMemoryMapper& m_mapper;
    std::unordered_map<uint32_t, ExpertGateConfig> m_layerConfigs;
    std::unordered_map<uint32_t, std::vector<uint32_t>> m_activeExperts;
    std::unordered_map<uint32_t, std::vector<void*>> m_stagingRegions;
};

// ============================================================================
// IQ2_M Dequantization Engine
// ============================================================================

class IQ2MDequantEngine {
public:
    IQ2MDequantEngine();
    ~IQ2MDequantEngine();
    
    // Initialize dequantization kernels
    bool Initialize();
    
    // Dequantize a single block (256 weights)
    bool DequantBlock(const void* compressed, float* output);
    
    // Dequantize an entire layer
    bool DequantLayer(const void* layerData, uint64_t weightCount, float* output);
    
    // Get compressed block size
    uint32_t GetBlockSize() const;
    
    // Get weights per block
    uint32_t GetWeightsPerBlock() const;
    
    // Calculate output size for given weight count
    uint64_t CalculateOutputSize(uint64_t weightCount) const;

private:
    bool m_initialized = false;
    uint32_t m_blockSize = 0;
    uint32_t m_weightsPerBlock = 0;
};

// ============================================================================
// Staged Paging Manager
// ============================================================================

class StagedPagingManager {
public:
    StagedPagingManager(SovereignMemoryMapper& mapper);
    ~StagedPagingManager();
    
    // Reserve virtual address space for full model
    bool ReserveVirtualSpace(uint64_t totalSize);
    
    // Commit physical pages for active region
    bool CommitRegion(uint64_t offset, uint64_t size);
    
    // Decommit physical pages (return to reserved state)
    bool DecommitRegion(uint64_t offset, uint64_t size);
    
    // Get statistics
    struct Stats {
        uint64_t reservedBytes = 0;
        uint64_t committedBytes = 0;
        uint64_t peakCommittedBytes = 0;
        uint32_t commitCount = 0;
        uint32_t decommitCount = 0;
    };
    Stats GetStats() const;

private:
    SovereignMemoryMapper& m_mapper;
    Stats m_stats;
};

// ============================================================================
// Convenience Functions
// ============================================================================

// Initialize entire sovereign memory subsystem
bool InitializeSovereignMemory();

// Shutdown sovereign memory subsystem
void ShutdownSovereignMemory();

// Get global memory mapper instance
SovereignMemoryMapper& GetGlobalMemoryMapper();

// Get global expert gate manager
ExpertGateManager& GetGlobalExpertGateManager();

// Get global dequantization engine
IQ2MDequantEngine& GetGlobalDequantEngine();

} // namespace rawrxd
