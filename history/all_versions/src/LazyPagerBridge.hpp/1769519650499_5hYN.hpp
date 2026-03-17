// LazyPagerBridge.hpp
// C++ bridge for RawrXD_LazyTensorPager MASM module
// Wire layer-wise demand paging into InferenceEngine for 800B-class models
#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle from MASM pager
typedef void* HLAZYPAGER;

// MASM exports (link against LazyTensorPager.obj)
HLAZYPAGER __cdecl LazyPager_Create(HANDLE hFile, uint64_t layerCount, uint32_t thermalThreshold);
int        __cdecl LazyPager_AttachModel(HLAZYPAGER ctx, uint64_t fileSize);
void*      __cdecl LazyPager_MapLayer(HLAZYPAGER ctx, uint64_t layerId, uint64_t fileOffset, uint64_t size);
void*      __cdecl LazyPager_ReadTensor(HLAZYPAGER ctx, uint64_t layerId, uint64_t tensorOffset, uint64_t size);
void       __cdecl LazyPager_ThermalThrottle(HLAZYPAGER ctx, uint32_t tempC);
void       __cdecl LazyPager_Destroy(HLAZYPAGER ctx);

#ifdef __cplusplus
}

namespace RawrXD {

// Configuration constants matching MASM module
constexpr uint32_t DEFAULT_THERMAL_LIMIT = 85;      // Celsius
constexpr uint64_t LAZY_PAGER_THRESHOLD  = 32ULL * 1024 * 1024 * 1024; // 32GB

/**
 * @brief RAII wrapper for the MASM lazy tensor pager
 * 
 * Manages memory-mapped layer-wise access to large GGUF models.
 * Maintains a fixed resident working set (~4GB for 8 layers) while
 * providing transparent access to the full 36GB+ model file.
 * 
 * Usage:
 *   HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, ...);
 *   LazyTensorPager pager(hFile, 80, 85);
 *   pager.Attach(fileSize);
 *   void* layer0 = pager.PinLayer(0, offset0, size0);
 */
class LazyTensorPager {
    HLAZYPAGER m_hPager;
    HANDLE     m_hFile;      // Owned file handle
    uint64_t   m_layerCount;
    bool       m_attached;
    
public:
    /**
     * @brief Construct pager and create internal context
     * @param hGgufFile   File handle to GGUF model (GENERIC_READ, FILE_SHARE_READ)
     * @param layers      Number of transformer layers (typically 80 for 800B)
     * @param thermalLimit Temperature threshold (Celsius) for backpressure
     */
    explicit LazyTensorPager(HANDLE hGgufFile, uint64_t layers, uint32_t thermalLimit = DEFAULT_THERMAL_LIMIT)
        : m_hFile(hGgufFile)
        , m_layerCount(layers)
        , m_attached(false)
    {
        m_hPager = LazyPager_Create(hGgufFile, layers, thermalLimit);
        if (!m_hPager) {
            throw std::runtime_error("LazyPager_Create failed: unable to initialize pager context");
        }
    }
    
    ~LazyTensorPager() {
        if (m_hPager) {
            LazyPager_Destroy(m_hPager);
            m_hPager = nullptr;
        }
        if (m_hFile && m_hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(m_hFile);
            m_hFile = INVALID_HANDLE_VALUE;
        }
    }
    
    // Non-copyable
    LazyTensorPager(const LazyTensorPager&) = delete;
    LazyTensorPager& operator=(const LazyTensorPager&) = delete;
    
    // Movable
    LazyTensorPager(LazyTensorPager&& other) noexcept
        : m_hPager(other.m_hPager)
        , m_hFile(other.m_hFile)
        , m_layerCount(other.m_layerCount)
        , m_attached(other.m_attached)
    {
        other.m_hPager = nullptr;
        other.m_hFile = INVALID_HANDLE_VALUE;
        other.m_attached = false;
    }
    
    LazyTensorPager& operator=(LazyTensorPager&& other) noexcept {
        if (this != &other) {
            if (m_hPager) LazyPager_Destroy(m_hPager);
            if (m_hFile && m_hFile != INVALID_HANDLE_VALUE) CloseHandle(m_hFile);
            
            m_hPager = other.m_hPager;
            m_hFile = other.m_hFile;
            m_layerCount = other.m_layerCount;
            m_attached = other.m_attached;
            
            other.m_hPager = nullptr;
            other.m_hFile = INVALID_HANDLE_VALUE;
            other.m_attached = false;
        }
        return *this;
    }
    
    /**
     * @brief Attach to GGUF file and create sparse memory mapping
     * @param fileSize Total file size in bytes
     * @return true on success, false on mapping failure
     */
    bool Attach(uint64_t fileSize) {
        if (!m_hPager) return false;
        m_attached = (LazyPager_AttachModel(m_hPager, fileSize) != 0);
        return m_attached;
    }
    
    /**
     * @brief Bring a layer into the resident working set
     * 
     * If the layer is already resident, returns immediately with LRU update.
     * Otherwise, may evict the LRU layer to make room.
     * 
     * @param layerId      Layer index (0 to layerCount-1)
     * @param offsetInFile Byte offset of layer data in GGUF file
     * @param size         Size of layer data in bytes
     * @return Host pointer to resident layer data, or nullptr on failure
     */
    void* PinLayer(uint64_t layerId, uint64_t offsetInFile, uint64_t size) {
        if (!m_hPager || !m_attached) return nullptr;
        return LazyPager_MapLayer(m_hPager, layerId, offsetInFile, size);
    }
    
    /**
     * @brief Fast-path tensor access within a resident layer
     * 
     * Assumes the layer is already pinned. If not, returns nullptr
     * and caller should call PinLayer first.
     * 
     * @param layerId          Layer index
     * @param tensorOffset     Offset of tensor within layer's data block
     * @param size             Tensor size (for validation, not used by pager)
     * @return Pointer to tensor data, or nullptr if layer not resident
     */
    void* AccessTensor(uint64_t layerId, uint64_t tensorOffset, uint64_t size) {
        if (!m_hPager || !m_attached) return nullptr;
        return LazyPager_ReadTensor(m_hPager, layerId, tensorOffset, size);
    }
    
    /**
     * @brief Update thermal status for backpressure control
     * 
     * When temperature exceeds the threshold, the pager introduces
     * delays on tensor reads to prevent thermal runaway.
     * 
     * @param tempCelsius Current CPU/GPU temperature
     */
    void UpdateThermal(uint32_t tempCelsius) {
        if (m_hPager) {
            LazyPager_ThermalThrottle(m_hPager, tempCelsius);
        }
    }
    
    // Accessors
    bool IsAttached() const { return m_attached; }
    uint64_t LayerCount() const { return m_layerCount; }
    HLAZYPAGER GetNativeHandle() const { return m_hPager; }
};

/**
 * @brief Factory function for creating pager from file path
 * @param ggufPath Path to GGUF model file
 * @param layers   Number of layers
 * @param thermalLimit Temperature threshold
 * @return unique_ptr to pager, or nullptr on file open failure
 */
inline std::unique_ptr<LazyTensorPager> CreateLazyPagerFromPath(
    const char* ggufPath,
    uint64_t layers,
    uint32_t thermalLimit = DEFAULT_THERMAL_LIMIT)
{
    HANDLE hFile = CreateFileA(
        ggufPath,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,    // No unbuffered - incompatible with memory mapping
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return nullptr;
    }
    
    try {
        return std::make_unique<LazyTensorPager>(hFile, layers, thermalLimit);
    } catch (...) {
        CloseHandle(hFile);
        return nullptr;
    }
}

} // namespace RawrXD

#endif // __cplusplus
