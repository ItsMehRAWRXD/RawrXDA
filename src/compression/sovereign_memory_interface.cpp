// ============================================================================
// RawrXD Sovereign Memory Mapping C++ Implementation
// ============================================================================

#include "sovereign_memory_interface.h"
#include "thread_lifecycle_registry.h"
#include <windows.h>
#include <psapi.h>
#include <algorithm>

namespace rawrxd {

// ============================================================================
// SovereignMemoryMapper Implementation
// ============================================================================

SovereignMemoryMapper::SovereignMemoryMapper() = default;

SovereignMemoryMapper::~SovereignMemoryMapper() {
    UnmapModel();
}

bool SovereignMemoryMapper::Initialize() {
    if (m_initialized) return true;
    
    int32_t result = SovMem_Init();
    if (result != 0) {
        return false;
    }
    
    result = SovSpin_Init();
    if (result != 0) {
        return false;
    }
    
    m_initialized = true;
    return true;
}

bool SovereignMemoryMapper::MapModel(const std::wstring& path, uint64_t size) {
    if (!m_initialized) {
        if (!Initialize()) return false;
    }
    
    UnmapModel();
    
    void* base = SovMem_MapModel(path.c_str(), nullptr, size);
    if (!base) {
        return false;
    }
    
    m_baseAddress = base;
    m_mapSize = size > 0 ? size : SovMem_GetMapSize();
    
    return true;
}

void SovereignMemoryMapper::UnmapModel() {
    if (m_baseAddress) {
        SovMem_UnmapModel();
        m_baseAddress = nullptr;
        m_mapSize = 0;
    }
    
    // Free staging buffers
    for (void* buf : m_stagingBuffers) {
        SovMem_FreeLargePage(buf);
    }
    m_stagingBuffers.clear();
}

void* SovereignMemoryMapper::GetBaseAddress() const {
    return m_baseAddress;
}

uint64_t SovereignMemoryMapper::GetSize() const {
    return m_mapSize;
}

bool SovereignMemoryMapper::IsMapped() const {
    return m_baseAddress != nullptr;
}

void* SovereignMemoryMapper::AllocateStagingBuffer(uint64_t size) {
    void* buf = SovMem_AllocateLargePage(size);
    if (buf) {
        m_stagingBuffers.push_back(buf);
    }
    return buf;
}

void SovereignMemoryMapper::FreeStagingBuffer(void* buffer) {
    if (!buffer) return;
    
    auto it = std::find(m_stagingBuffers.begin(), m_stagingBuffers.end(), buffer);
    if (it != m_stagingBuffers.end()) {
        SovMem_FreeLargePage(buffer);
        m_stagingBuffers.erase(it);
    }
}

bool SovereignMemoryMapper::ReadExpertWeights(uint32_t expertId, void* dst, 
                                               uint64_t offset, uint64_t size) {
    if (!m_baseAddress || !dst) return false;
    
    return SovMem_ReadWithLock(expertId, 
                               static_cast<char*>(m_baseAddress) + offset,
                               dst, size) != 0;
}

bool SovereignMemoryMapper::WriteExpertWeights(uint32_t expertId, void* src,
                                                uint64_t offset, uint64_t size) {
    if (!m_baseAddress || !src) return false;
    
    return SovMem_WriteWithLock(expertId,
                                static_cast<char*>(m_baseAddress) + offset,
                                src, size) != 0;
}

// ============================================================================
// ExpertGateManager Implementation
// ============================================================================

ExpertGateManager::ExpertGateManager(SovereignMemoryMapper& mapper) 
    : m_mapper(mapper) {}

ExpertGateManager::~ExpertGateManager() {
    // Deactivate all experts
    for (auto& [layerId, _] : m_layerConfigs) {
        DeactivateExperts(layerId);
    }
}

bool ExpertGateManager::ConfigureLayer(uint32_t layerId, 
                                        const ExpertGateConfig& config) {
    if (config.numExperts == 0 || config.activeExperts == 0) {
        return false;
    }
    
    if (config.activeExperts > config.numExperts) {
        return false;
    }
    
    m_layerConfigs[layerId] = config;
    
    // Pre-allocate staging regions for active experts
    uint64_t stagingSize = config.expertSizeBytes * config.activeExperts;
    if (stagingSize > 0) {
        void* staging = m_mapper.AllocateStagingBuffer(stagingSize);
        if (staging) {
            m_stagingRegions[layerId].resize(config.activeExperts);
            char* ptr = static_cast<char*>(staging);
            for (uint32_t i = 0; i < config.activeExperts; ++i) {
                m_stagingRegions[layerId][i] = ptr + (i * config.expertSizeBytes);
            }
        }
    }
    
    return true;
}

bool ExpertGateManager::ActivateExperts(uint32_t layerId, 
                                          const uint32_t* expertIds, 
                                          uint32_t count) {
    auto it = m_layerConfigs.find(layerId);
    if (it == m_layerConfigs.end()) {
        return false;
    }
    
    const ExpertGateConfig& config = it->second;
    
    if (count > config.activeExperts) {
        return false;
    }
    
    // Validate expert IDs
    for (uint32_t i = 0; i < count; ++i) {
        if (expertIds[i] >= config.numExperts) {
            return false;
        }
    }
    
    // Deactivate current experts first
    DeactivateExperts(layerId);
    
    // Activate new experts
    m_activeExperts[layerId].assign(expertIds, expertIds + count);
    
    // Load weights into staging regions
    auto stagingIt = m_stagingRegions.find(layerId);
    if (stagingIt != m_stagingRegions.end()) {
        for (uint32_t i = 0; i < count; ++i) {
            uint64_t offset = expertIds[i] * config.expertSizeBytes;
            uint32_t lockSlot = config.lockSlotBase + expertIds[i];
            
            m_mapper.ReadExpertWeights(lockSlot, stagingIt->second[i],
                                       offset, config.expertSizeBytes);
        }
    }
    
    return true;
}

void ExpertGateManager::DeactivateExperts(uint32_t layerId) {
    auto it = m_activeExperts.find(layerId);
    if (it == m_activeExperts.end()) return;
    
    auto configIt = m_layerConfigs.find(layerId);
    if (configIt != m_layerConfigs.end()) {
        // Release locks for active experts
        for (uint32_t expertId : it->second) {
            uint32_t lockSlot = configIt->second.lockSlotBase + expertId;
            SovSpin_Release(lockSlot);
        }
    }
    
    it->second.clear();
}

void* ExpertGateManager::GetExpertWeights(uint32_t layerId, uint32_t expertId) {
    auto activeIt = m_activeExperts.find(layerId);
    if (activeIt == m_activeExperts.end()) {
        return nullptr;
    }
    
    // Find index of expertId in active list
    auto idxIt = std::find(activeIt->second.begin(), 
                           activeIt->second.end(), expertId);
    if (idxIt == activeIt->second.end()) {
        return nullptr;
    }
    
    size_t idx = std::distance(activeIt->second.begin(), idxIt);
    
    auto stagingIt = m_stagingRegions.find(layerId);
    if (stagingIt == m_stagingRegions.end() || 
        idx >= stagingIt->second.size()) {
        return nullptr;
    }
    
    return stagingIt->second[idx];
}

void ExpertGateManager::PrefetchExpert(uint32_t layerId, uint32_t expertId) {
    auto configIt = m_layerConfigs.find(layerId);
    if (configIt == m_layerConfigs.end()) return;
    
    const ExpertGateConfig& config = configIt->second;
    if (expertId >= config.numExperts) return;
    
    uint64_t offset = expertId * config.expertSizeBytes;
    void* base = m_mapper.GetBaseAddress();
    if (!base) return;
    
    // Prefetch into cache using Windows API
    char* addr = static_cast<char*>(base) + offset;
    for (uint64_t i = 0; i < config.expertSizeBytes; i += 4096) {
        PrefetchVirtualMemory(GetCurrentProcess(), 
                              reinterpret_cast<PVOID>(addr + i), 4096, 0);
    }
}

uint32_t ExpertGateManager::GetActiveExpertCount(uint32_t layerId) const {
    auto it = m_activeExperts.find(layerId);
    if (it == m_activeExperts.end()) {
        return 0;
    }
    return static_cast<uint32_t>(it->second.size());
}

// ============================================================================
// IQ2MDequantEngine Implementation
// ============================================================================

IQ2MDequantEngine::IQ2MDequantEngine() = default;

IQ2MDequantEngine::~IQ2MDequantEngine() = default;

bool IQ2MDequantEngine::Initialize() {
    if (m_initialized) return true;
    
    int32_t result = IQ2M_Init();
    if (result != 0) {
        return false;
    }
    
    m_blockSize = static_cast<uint32_t>(IQ2M_GetBlockSize());
    m_weightsPerBlock = static_cast<uint32_t>(IQ2M_GetWeightsPerBlock());
    
    m_initialized = true;
    return true;
}

bool IQ2MDequantEngine::DequantBlock(const void* compressed, float* output) {
    if (!m_initialized || !compressed || !output) {
        return false;
    }
    
    int32_t result = IQ2M_DequantBlock(
        const_cast<void*>(compressed), output);
    
    return result == m_weightsPerBlock;
}

bool IQ2MDequantEngine::DequantLayer(const void* layerData, 
                                      uint64_t weightCount, 
                                      float* output) {
    if (!m_initialized || !layerData || !output || weightCount == 0) {
        return false;
    }
    
    int32_t result = IQ2M_DequantLayer(
        const_cast<void*>(layerData), weightCount, output);
    
    return result > 0;
}

uint32_t IQ2MDequantEngine::GetBlockSize() const {
    return m_blockSize;
}

uint32_t IQ2MDequantEngine::GetWeightsPerBlock() const {
    return m_weightsPerBlock;
}

uint64_t IQ2MDequantEngine::CalculateOutputSize(uint64_t weightCount) const {
    if (m_weightsPerBlock == 0) return 0;
    
    uint64_t numBlocks = (weightCount + m_weightsPerBlock - 1) / m_weightsPerBlock;
    return numBlocks * m_weightsPerBlock * sizeof(float);
}

// ============================================================================
// StagedPagingManager Implementation
// ============================================================================

StagedPagingManager::StagedPagingManager(SovereignMemoryMapper& mapper)
    : m_mapper(mapper) {}

StagedPagingManager::~StagedPagingManager() = default;

bool StagedPagingManager::ReserveVirtualSpace(uint64_t totalSize) {
    // Reserve virtual address space without committing
    void* addr = VirtualAlloc(nullptr, totalSize, 
                              MEM_RESERVE, PAGE_NOACCESS);
    if (!addr) {
        return false;
    }
    
    m_stats.reservedBytes = totalSize;
    return true;
}

bool StagedPagingManager::CommitRegion(uint64_t offset, uint64_t size) {
    void* base = m_mapper.GetBaseAddress();
    if (!base) return false;
    
    char* addr = static_cast<char*>(base) + offset;
    
    void* result = VirtualAlloc(addr, size, 
                                MEM_COMMIT, PAGE_READWRITE);
    if (!result) {
        return false;
    }
    
    m_stats.committedBytes += size;
    m_stats.peakCommittedBytes = std::max(m_stats.peakCommittedBytes, 
                                          m_stats.committedBytes);
    m_stats.commitCount++;
    
    return true;
}

bool StagedPagingManager::DecommitRegion(uint64_t offset, uint64_t size) {
    void* base = m_mapper.GetBaseAddress();
    if (!base) return false;
    
    char* addr = static_cast<char*>(base) + offset;
    
    BOOL result = VirtualFree(addr, size, MEM_DECOMMIT);
    if (!result) {
        return false;
    }
    
    m_stats.committedBytes -= std::min(m_stats.committedBytes, size);
    m_stats.decommitCount++;
    
    return true;
}

StagedPagingManager::Stats StagedPagingManager::GetStats() const {
    return m_stats;
}

// ============================================================================
// Global Instances
// ============================================================================

static std::unique_ptr<SovereignMemoryMapper> g_memoryMapper;
static std::unique_ptr<ExpertGateManager> g_expertGateManager;
static std::unique_ptr<IQ2MDequantEngine> g_dequantEngine;

bool InitializeSovereignMemory() {
    if (!g_memoryMapper) {
        g_memoryMapper = std::make_unique<SovereignMemoryMapper>();
    }
    
    if (!g_memoryMapper->Initialize()) {
        return false;
    }
    
    if (!g_expertGateManager) {
        g_expertGateManager = std::make_unique<ExpertGateManager>(
            *g_memoryMapper);
    }
    
    if (!g_dequantEngine) {
        g_dequantEngine = std::make_unique<IQ2MDequantEngine>();
    }
    
    return g_dequantEngine->Initialize();
}

void ShutdownSovereignMemory() {
    g_expertGateManager.reset();
    g_dequantEngine.reset();
    g_memoryMapper.reset();
}

SovereignMemoryMapper& GetGlobalMemoryMapper() {
    if (!g_memoryMapper) {
        g_memoryMapper = std::make_unique<SovereignMemoryMapper>();
    }
    return *g_memoryMapper;
}

ExpertGateManager& GetGlobalExpertGateManager() {
    if (!g_expertGateManager) {
        if (!g_memoryMapper) {
            g_memoryMapper = std::make_unique<SovereignMemoryMapper>();
        }
        g_expertGateManager = std::make_unique<ExpertGateManager>(
            *g_memoryMapper);
    }
    return *g_expertGateManager;
}

IQ2MDequantEngine& GetGlobalDequantEngine() {
    if (!g_dequantEngine) {
        g_dequantEngine = std::make_unique<IQ2MDequantEngine>();
    }
    return *g_dequantEngine;
}

} // namespace rawrxd
