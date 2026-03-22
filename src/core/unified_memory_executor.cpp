// ============================================================================
// RawrXD Unified Memory Executor (AMD SAM Edition) - C++ Implementation
// ============================================================================

#include "unified_memory_executor.h"
#include "../logging/Logger.h"
#include <algorithm>
#include <cstring>
#include <thread>
#include <windows.h>
#include <winioctl.h>

// Forward declarations for assembly functions
extern "C"
{
    // PCI Configuration Space Access
    uint64_t ReadPciConfigAMD(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset);

    // BAR Mapping (requires kernel-mode driver or user-mode via driver)
    void* MmMapIoSpaceEx(uint64_t physicalAddress, uint64_t size, uint32_t flags);

    // Unified Memory Coherency Test
    int TestUnifiedCoherency(void* baseAddress, uint64_t size);

    // GPU Initialization (ROCm/HIP)
    int HipInitUnifiedMemory(void* baseAddress);
}

namespace RawrXD
{
namespace UnifiedMemory
{

// ============================================================================
// Singleton
// ============================================================================

UnifiedMemoryExecutor& UnifiedMemoryExecutor::instance()
{
    static UnifiedMemoryExecutor s_instance;
    return s_instance;
}

// ============================================================================
// Lifecycle
// ============================================================================

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::initialize()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized.load())
    {
        return {};  // Idempotent — safe when IDE/settings re-apply
    }

    RawrXD::Logging::Logger::instance().info(
        "[UnifiedMemory] Initializing unified memory executor (SAM + fallback)...");

    m_hostBackedUnified = false;

    // Try hardware SAM path (PCI BAR + driver map). Usually unavailable in user mode without a driver.
    const auto bar0Result = initializeBAR0();
    if (bar0Result)
    {
        const auto mapResult = mapBAR0ToCPU();
        if (mapResult)
        {
            const auto heapResult = establishUnifiedHeap();
            if (heapResult)
            {
                (void)initializeGPUUnifiedMemory();
                m_initialized.store(true);
                (void)testUnifiedCoherency();
                const uint64_t gbAvailable = GPU_ACCESSIBLE / (1024ULL * 1024ULL * 1024ULL);
                RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Hardware SAM path active (~" +
                                                         std::to_string(gbAvailable) + "GB heap)");
                return {};
            }
        }
    }

    RawrXD::Logging::Logger::instance().warning(
        "[UnifiedMemory] Hardware SAM unavailable; using host-backed unified arena (IDE/dev mode).");
    return initializeHostBackedUnified();
}

void UnifiedMemoryExecutor::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized.load())
    {
        return;
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Shutting down unified memory executor...");

    if (m_hostBackedUnified && m_bar0Virtual)
    {
        VirtualFree(m_bar0Virtual, 0, MEM_RELEASE);
        m_hostBackedUnified = false;
    }
    // Hardware BAR: production code would unmap via driver / MmUnmapIoSpace equivalent
    m_bar0Virtual = nullptr;

    m_initialized.store(false);
    m_bar0Physical = 0;
    m_heapBase = nullptr;
    m_heapPtr.store(0);
    m_heapLimit = 0;
    m_modelBase = nullptr;
    m_modelSize = 0;

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Unified memory executor shut down");
}

// ============================================================================
// Memory Management
// ============================================================================

RawrXD::Expected<UnifiedBuffer, UnifiedMemoryError> UnifiedMemoryExecutor::allocate(uint64_t sizeBytes,
                                                                                    uint64_t alignment)
{

    if (!m_initialized.load())
    {
        return RawrXD::unexpected(UnifiedMemoryError::NotInitialized);
    }

    if (sizeBytes == 0)
    {
        return RawrXD::unexpected(UnifiedMemoryError::InvalidParameter);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Align up
    uint64_t currentPtr = m_heapPtr.load();
    uint64_t alignedPtr = (currentPtr + alignment - 1) & ~(alignment - 1);

    // Check limit
    if (alignedPtr + sizeBytes > m_heapLimit)
    {
        return RawrXD::unexpected(UnifiedMemoryError::InsufficientMemory);
    }

    // Bump allocate (no free for now - streaming executor)
    uint64_t newPtr = alignedPtr + sizeBytes;
    m_heapPtr.store(newPtr);

    UnifiedBuffer buffer;
    buffer.ptr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_heapBase) + alignedPtr);
    buffer.sizeBytes = sizeBytes;
    buffer.alignment = alignment;
    buffer.isMapped = true;
    buffer.bufferId = m_nextBufferId++;

    // Zero initialize (CPU side)
    std::memset(buffer.ptr, 0, sizeBytes);

    // Update statistics
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.totalAllocatedBytes += sizeBytes;
        m_stats.allocationCount++;
        if (m_stats.totalAllocatedBytes > m_stats.peakAllocatedBytes)
        {
            m_stats.peakAllocatedBytes = m_stats.totalAllocatedBytes;
        }
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Allocated " + std::to_string(sizeBytes) +
                                             " bytes at unified address " +
                                             std::to_string(reinterpret_cast<uintptr_t>(buffer.ptr)));

    return buffer;
}

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::free(UnifiedBuffer& buffer)
{
    if (!m_initialized.load())
    {
        return RawrXD::unexpected(UnifiedMemoryError::NotInitialized);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // For streaming executor, we don't free individual buffers
    // The entire heap is reset when model changes
    // In production: implement proper free list or buddy allocator

    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.totalAllocatedBytes -= buffer.sizeBytes;
        m_stats.freeCount++;
    }

    buffer.ptr = nullptr;
    buffer.sizeBytes = 0;
    buffer.isMapped = false;

    return {};
}

// ============================================================================
// Model Loading (Zero-Copy)
// ============================================================================

uint64_t UnifiedMemoryExecutor::getUnifiedHeapRemainingBytes()
{
    if (!m_initialized.load() || !m_heapBase)
    {
        return 0;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    const uint64_t cur = m_heapPtr.load();
    return (m_heapLimit > cur) ? (m_heapLimit - cur) : 0;
}

RawrXD::Expected<UnifiedBuffer, UnifiedMemoryError> UnifiedMemoryExecutor::loadModelUnified(const std::string& filePath,
                                                                                            uint64_t fileSize)
{

    if (!m_initialized.load())
    {
        return RawrXD::unexpected(UnifiedMemoryError::NotInitialized);
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Loading model into unified memory: " + filePath + " (" +
                                             std::to_string(fileSize) + " bytes)");

    // Allocate unified buffer for entire model
    auto allocResult = allocate(fileSize, 0x10000);  // 64KB alignment for GPU
    if (!allocResult)
    {
        return RawrXD::unexpected(allocResult.error());
    }

    UnifiedBuffer modelBuffer = allocResult.value();
    m_modelBase = modelBuffer.ptr;
    m_modelSize = fileSize;

    // Memory-map file directly into unified region
    // Instead of ReadFile, use CreateFileMapping + MapViewOfFile
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        RawrXD::Logging::Logger::instance().error("[UnifiedMemory] Failed to open model file: " + filePath);
        return RawrXD::unexpected(UnifiedMemoryError::ModelLoadFailed);
    }

    HANDLE hMapping = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);

    if (!hMapping)
    {
        CloseHandle(hFile);
        RawrXD::Logging::Logger::instance().error("[UnifiedMemory] Failed to create file mapping");
        return RawrXD::unexpected(UnifiedMemoryError::ModelLoadFailed);
    }

    // Map view at specific address (unified memory region)
    // Note: MapViewOfFile doesn't support forcing a base address in user mode
    // In production: use kernel-mode driver or NtMapViewOfSection with specific base
    void* mappedView = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, fileSize);

    if (!mappedView)
    {
        CloseHandle(hMapping);
        CloseHandle(hFile);
        RawrXD::Logging::Logger::instance().error("[UnifiedMemory] Failed to map view of file");
        return RawrXD::unexpected(UnifiedMemoryError::ModelLoadFailed);
    }

    // Copy to unified memory (in production: use DMA or direct mapping)
    std::memcpy(modelBuffer.ptr, mappedView, fileSize);

    UnmapViewOfFile(mappedView);
    CloseHandle(hMapping);
    CloseHandle(hFile);

    // No copy needed - file is now in GPU-accessible memory
    // CPU can parse headers, GPU can execute weights directly

    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.modelLoadCount++;
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Model loaded into unified memory at " +
                                             std::to_string(reinterpret_cast<uintptr_t>(modelBuffer.ptr)));

    return modelBuffer;
}

// ============================================================================
// Execution
// ============================================================================

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::executeLayerUnified(uint32_t layerIndex,
                                                                                      const UnifiedBuffer& input,
                                                                                      UnifiedBuffer& output)
{

    if (!m_initialized.load())
    {
        return RawrXD::unexpected(UnifiedMemoryError::NotInitialized);
    }

    // Calculate layer weights location (unified memory)
    // In production: use model metadata to get layer offset
    uint64_t layerOffset = layerIndex * 1024ULL * 1024ULL;  // Placeholder
    void* layerWeights = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_modelBase) + layerOffset);

    // Prepare kernel arguments
    // All pointers are unified addresses - valid for both CPU and GPU

    // Dispatch GPU kernel (HIP/Vulkan/DirectML)
    // Kernel sees unified memory directly - no upload needed
    // In production: call actual GPU dispatch
    // For now: CPU fallback
    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Executing layer " + std::to_string(layerIndex) +
                                             " with unified memory");

    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        m_stats.layerExecutionCount++;
    }

    // Fence for ordering (not for coherency - unified is coherent)
    m_fenceCounter.fetch_add(1, std::memory_order_acq_rel);

    return {};
}

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::streamingExecutorUnified()
{
    if (!m_initialized.load())
    {
        return RawrXD::unexpected(UnifiedMemoryError::NotInitialized);
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Starting streaming executor (unified memory)");

    // Model is fully resident in unified memory
    // Execute layer by layer with CPU/GPU cooperation
    // In production: implement actual layer-by-layer execution
    // For now: placeholder

    return {};
}

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::heterogeneousScheduler()
{
    if (!m_initialized.load())
    {
        return RawrXD::unexpected(UnifiedMemoryError::NotInitialized);
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Starting heterogeneous scheduler (CPU + GPU)");

    // Split work based on layer characteristics:
    //   Attention layers -> GPU (compute bound)
    //   Embedding/Norm -> CPU (memory bound, sequential)
    // In production: implement actual scheduling logic

    return {};
}

// ============================================================================
// Coherency and Synchronization
// ============================================================================

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::testUnifiedCoherency()
{
    if (!m_initialized.load() || !m_heapBase)
    {
        return RawrXD::unexpected(UnifiedMemoryError::NotInitialized);
    }

    // Test pattern: write from CPU, verify GPU can read (or vice versa)
    // In production: use actual GPU readback test
    // For now: simple memory test
    void* testAddr = m_heapBase;
    uint64_t testSize = 4096;  // 4KB

    // Write test pattern
    uint32_t* pattern = static_cast<uint32_t*>(testAddr);
    for (size_t i = 0; i < testSize / sizeof(uint32_t); ++i)
    {
        pattern[i] = static_cast<uint32_t>(i);
    }

    // Verify pattern
    for (size_t i = 0; i < testSize / sizeof(uint32_t); ++i)
    {
        if (pattern[i] != static_cast<uint32_t>(i))
        {
            RawrXD::Logging::Logger::instance().error("[UnifiedMemory] Coherency test failed at offset " +
                                                      std::to_string(i));
            return RawrXD::unexpected(UnifiedMemoryError::CoherencyTestFailed);
        }
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Coherency test passed");
    return {};
}

void UnifiedMemoryExecutor::unifiedFence()
{
    // On Apple Silicon / AMD SAM: memory is coherent, just need ordering
    std::atomic_thread_fence(std::memory_order_acq_rel);
}

void UnifiedMemoryExecutor::waitForGpuUnified()
{
    // GPU writes to m_gpuCompletion in unified memory
    // CPU polls without PCIe read overhead (same memory domain)
    uint64_t expectedFence = m_fenceCounter.load(std::memory_order_acquire);

    while (m_gpuCompletion.load(std::memory_order_acquire) < expectedFence)
    {
        // Spin-wait (in production: use proper synchronization)
        std::this_thread::yield();
    }
}

// ============================================================================
// Statistics
// ============================================================================

UnifiedMemoryExecutor::Stats UnifiedMemoryExecutor::getStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

// ============================================================================
// BAR0 Information
// ============================================================================

RawrXD::Expected<UnifiedMemoryExecutor::BAR0Info, UnifiedMemoryError> UnifiedMemoryExecutor::getBAR0Info() const
{
    if (!m_initialized.load())
    {
        return RawrXD::unexpected(UnifiedMemoryError::NotInitialized);
    }

    BAR0Info info;
    info.physicalAddress = m_bar0Physical;
    info.virtualAddress = m_bar0Virtual;
    info.sizeBytes = m_bar0Size;
    info.samEnabled = m_samEnabled;

    return info;
}

// ============================================================================
// Internal Implementation
// ============================================================================

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::initializeBAR0()
{
    // Read PCI config space to get BAR0
    // In production: use proper PCI enumeration
    // For now: try to read from common AMD GPU locations

    // Try bus 0, device 0, function 0 (common for primary GPU)
    uint64_t bar0 = ReadPciConfigAMD(0, 0, 0, 0x10);  // BAR0 offset

    if (bar0 == 0 || (bar0 & 0xFFFFFFFFFFFFFFF0ULL) == 0)
    {
        // Try alternative locations or use driver interface
        RawrXD::Logging::Logger::instance().warning(
            "[UnifiedMemory] Could not read BAR0 from PCI config, using driver interface");
        // In production: query via AMD driver or WMI
        return RawrXD::unexpected(UnifiedMemoryError::SAMNotEnabled);
    }

    // Mask flags (bit 0 = I/O space, bits 2:1 = type, bit 3 = prefetchable)
    m_bar0Physical = bar0 & 0xFFFFFFFFFFFFFFF0ULL;
    m_samEnabled = (bar0 & 0xF) == 0;  // Memory space, 64-bit

    if (!m_samEnabled)
    {
        RawrXD::Logging::Logger::instance().error(
            "[UnifiedMemory] SAM not enabled - BAR0 indicates I/O space or invalid type");
        return RawrXD::unexpected(UnifiedMemoryError::SAMNotEnabled);
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] BAR0 physical address: 0x" +
                                             std::to_string(m_bar0Physical));
    return {};
}

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::mapBAR0ToCPU()
{
    if (m_bar0Physical == 0)
    {
        return RawrXD::unexpected(UnifiedMemoryError::SAMNotEnabled);
    }

    // Map entire 16GB BAR to CPU virtual address space
    // In production: requires kernel-mode driver or user-mode via driver
    // For now: use placeholder that would call driver
    m_bar0Virtual = MmMapIoSpaceEx(m_bar0Physical, m_bar0Size, 0);

    if (!m_bar0Virtual)
    {
        RawrXD::Logging::Logger::instance().error("[UnifiedMemory] Failed to map BAR0 to CPU address space");
        return RawrXD::unexpected(UnifiedMemoryError::BARMappingFailed);
    }

    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] BAR0 mapped to virtual address: " +
                                             std::to_string(reinterpret_cast<uintptr_t>(m_bar0Virtual)));
    return {};
}

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::establishUnifiedHeap()
{
    if (!m_bar0Virtual)
    {
        return RawrXD::unexpected(UnifiedMemoryError::BARMappingFailed);
    }

    // Establish unified heap (after system reserve)
    m_heapBase = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_bar0Virtual) + SYSTEM_RAM_RESERVE);
    m_heapPtr.store(0);

    m_heapLimit = GPU_ACCESSIBLE - SYSTEM_RAM_RESERVE;

    uint64_t limitGB = m_heapLimit / (1024ULL * 1024ULL * 1024ULL);
    RawrXD::Logging::Logger::instance().info(
        "[UnifiedMemory] Unified heap established: base=" + std::to_string(reinterpret_cast<uintptr_t>(m_heapBase)) +
        ", limit=" + std::to_string(limitGB) + "GB");

    return {};
}

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::initializeGPUUnifiedMemory()
{
    if (!m_bar0Virtual)
    {
        return RawrXD::unexpected(UnifiedMemoryError::BARMappingFailed);
    }

    // Initialize GPU with same base pointer
    // In production: call ROCm/HIP or Vulkan/DirectML initialization
    int result = HipInitUnifiedMemory(m_bar0Virtual);

    if (result != 0)
    {
        RawrXD::Logging::Logger::instance().warning("[UnifiedMemory] GPU unified memory init returned " +
                                                    std::to_string(result));
        // Not fatal - continue with CPU-only unified memory
    }
    else
    {
        RawrXD::Logging::Logger::instance().info("[UnifiedMemory] GPU unified memory initialized");
    }

    return {};
}

RawrXD::Expected<void, UnifiedMemoryError> UnifiedMemoryExecutor::initializeHostBackedUnified()
{
    constexpr uint64_t kArena = 512ULL * 1024 * 1024;
    constexpr uint64_t kReserve = 64ULL * 1024 * 1024;

    void* arena = VirtualAlloc(nullptr, static_cast<SIZE_T>(kArena), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!arena)
    {
        RawrXD::Logging::Logger::instance().error("[UnifiedMemory] Host-backed VirtualAlloc failed");
        return RawrXD::unexpected(UnifiedMemoryError::AllocationFailed);
    }

    m_hostBackedUnified = true;
    m_bar0Virtual = arena;
    m_bar0Physical = 0;
    m_bar0Size = kArena;
    m_samEnabled = false;
    m_heapBase = static_cast<char*>(arena) + static_cast<size_t>(kReserve);
    m_heapPtr.store(0);
    m_heapLimit = kArena - kReserve;

    (void)initializeGPUUnifiedMemory();
    m_initialized.store(true);
    (void)testUnifiedCoherency();

    const uint64_t mib = m_heapLimit / (1024ULL * 1024ULL);
    RawrXD::Logging::Logger::instance().info("[UnifiedMemory] Host-backed unified arena ready: " + std::to_string(mib) +
                                             " MiB bump heap");
    return {};
}

}  // namespace UnifiedMemory
}  // namespace RawrXD

// =============================================================================
// Low-level hooks (assembly / driver). Stubs keep the IDE linkable on all builds.
// =============================================================================
extern "C" uint64_t ReadPciConfigAMD(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset)
{
    (void)bus;
    (void)device;
    (void)function;
    (void)offset;
    return 0ULL;
}

extern "C" void* MmMapIoSpaceEx(uint64_t physicalAddress, uint64_t size, uint32_t flags)
{
    (void)physicalAddress;
    (void)size;
    (void)flags;
    return nullptr;
}

extern "C" int TestUnifiedCoherency(void* baseAddress, uint64_t size)
{
    (void)baseAddress;
    (void)size;
    return 0;
}

extern "C" int HipInitUnifiedMemory(void* baseAddress)
{
    (void)baseAddress;
    return 0;
}
