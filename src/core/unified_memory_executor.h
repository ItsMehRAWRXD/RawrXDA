#pragma once

// ============================================================================
// RawrXD Unified Memory Executor (AMD SAM Edition)
// Apple Silicon-style unified memory on AMD RX 7800 XT (16GB)
// CPU and GPU share same address space - zero copy, zero transfer
// ============================================================================

#include <cstdint>
#include <memory>
#include <string>
#include <expected>
#include <mutex>
#include <atomic>

namespace RawrXD {
namespace UnifiedMemory {

// ============================================================================
// Error Types
// ============================================================================

enum class UnifiedMemoryError {
    Success = 0,
    SAMNotEnabled,          // Resizable BAR not enabled in BIOS
    BARMappingFailed,       // Failed to map BAR0 to CPU address space
    InsufficientMemory,     // Not enough unified memory available
    InvalidParameter,       // Invalid function parameter
    AlreadyInitialized,     // Unified memory already initialized
    NotInitialized,         // Unified memory not initialized
    AllocationFailed,       // Memory allocation failed
    ModelLoadFailed,        // Failed to load model into unified memory
    CoherencyTestFailed     // Memory coherency test failed
};

// ============================================================================
// Unified Memory Constants
// ============================================================================

constexpr uint64_t SAM_ENABLED = 1;
constexpr uint64_t BAR0_SIZE_16GB = 0x400000000ULL;  // 16GB flat mapping
constexpr uint64_t PAGE_SIZE = 0x1000ULL;             // 4KB pages
constexpr uint64_t SYSTEM_RAM_RESERVE = 0x40000000ULL; // 1GB reserved for OS/system
constexpr uint64_t GPU_ACCESSIBLE = 0x3C0000000ULL;    // 15GB for unified compute

// ============================================================================
// Execution Modes
// ============================================================================

enum class ExecutionMode {
    CPU_ONLY = 0,
    GPU_ONLY = 1,
    HETEROGENEOUS = 2  // CPU prep + GPU compute
};

// ============================================================================
// Unified Memory Buffer
// ============================================================================

struct UnifiedBuffer {
    void* ptr = nullptr;           // Unified pointer (valid on both CPU and GPU)
    uint64_t sizeBytes = 0;        // Size in bytes
    uint64_t alignment = 0;        // Alignment requirement
    bool isMapped = false;         // Whether buffer is mapped
    uint32_t bufferId = 0;         // Unique buffer identifier
};

// ============================================================================
// Unified Memory Executor
// ============================================================================

class UnifiedMemoryExecutor {
public:
    static UnifiedMemoryExecutor& instance();

    // Lifecycle
    std::expected<void, UnifiedMemoryError> initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    // Memory Management
    std::expected<UnifiedBuffer, UnifiedMemoryError> allocate(uint64_t sizeBytes, uint64_t alignment = 64);
    std::expected<void, UnifiedMemoryError> free(UnifiedBuffer& buffer);

    // Model Loading (zero-copy)
    std::expected<UnifiedBuffer, UnifiedMemoryError> loadModelUnified(
        const std::string& filePath, uint64_t fileSize);

    // Execution
    std::expected<void, UnifiedMemoryError> executeLayerUnified(
        uint32_t layerIndex, const UnifiedBuffer& input, UnifiedBuffer& output);

    std::expected<void, UnifiedMemoryError> streamingExecutorUnified();

    std::expected<void, UnifiedMemoryError> heterogeneousScheduler();

    // Coherency and Synchronization
    std::expected<void, UnifiedMemoryError> testUnifiedCoherency();
    void unifiedFence();  // Lightweight fence (no cache flush on unified)
    void waitForGpuUnified();  // Poll unified flag for GPU completion

    // Statistics
    struct Stats {
        uint64_t totalAllocatedBytes = 0;
        uint64_t peakAllocatedBytes = 0;
        uint64_t allocationCount = 0;
        uint64_t freeCount = 0;
        uint64_t modelLoadCount = 0;
        uint64_t layerExecutionCount = 0;
    };

    Stats getStats() const;

    // BAR0 Information
    struct BAR0Info {
        uint64_t physicalAddress = 0;
        void* virtualAddress = nullptr;
        uint64_t sizeBytes = 0;
        bool samEnabled = false;
    };

    std::expected<BAR0Info, UnifiedMemoryError> getBAR0Info() const;

    /** True when hardware SAM BAR mapping failed and a host RAM arena is used (dev / no driver). */
    bool isHostBackedMode() const { return m_hostBackedUnified; }

    /** Bump-allocator span (bytes) after reserve; meaningful once initialized. */
    uint64_t getUnifiedHeapCapacityBytes() const { return m_heapLimit; }

private:
    UnifiedMemoryExecutor() = default;
    ~UnifiedMemoryExecutor() { shutdown(); }

    // Non-copyable
    UnifiedMemoryExecutor(const UnifiedMemoryExecutor&) = delete;
    UnifiedMemoryExecutor& operator=(const UnifiedMemoryExecutor&) = delete;

    // Internal Implementation
    std::expected<void, UnifiedMemoryError> initializeBAR0();
    std::expected<void, UnifiedMemoryError> mapBAR0ToCPU();
    std::expected<void, UnifiedMemoryError> establishUnifiedHeap();
    std::expected<void, UnifiedMemoryError> initializeGPUUnifiedMemory();
    std::expected<void, UnifiedMemoryError> initializeHostBackedUnified();

    // State
    std::atomic<bool> m_initialized{false};
    bool m_hostBackedUnified{false};
    mutable std::mutex m_mutex;

    // BAR0 State
    uint64_t m_bar0Physical = 0;
    void* m_bar0Virtual = nullptr;
    uint64_t m_bar0Size = BAR0_SIZE_16GB;
    bool m_samEnabled = false;

    // Unified Heap State
    void* m_heapBase = nullptr;
    std::atomic<uint64_t> m_heapPtr{0};
    uint64_t m_heapLimit = 0;

    // Model State
    void* m_modelBase = nullptr;
    uint64_t m_modelSize = 0;

    // Synchronization
    std::atomic<uint64_t> m_fenceCounter{0};
    std::atomic<uint64_t> m_gpuCompletion{0};

    // Statistics
    mutable std::mutex m_statsMutex;
    Stats m_stats;
    uint32_t m_nextBufferId = 1;
};

}  // namespace UnifiedMemory
}  // namespace RawrXD
