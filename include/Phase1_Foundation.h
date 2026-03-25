<<<<<<< HEAD
#pragma once

#include <cstdint>
#include <cstring>
#include <windows.h>

/*
================================================================================
 PHASE1_FOUNDATION.H - Public API for RawrXD Foundation Layer
 
 This header provides the C++ interface to the x64 assembly-based Phase 1
 bootstrap layer. All higher phases (2-5) depend on this foundation.
 
 Architecture Stack:
   Phase 5: Orchestrator        (depends on Phase 1)
   Phase 4: Swarm Inference     (depends on Phase 1)
   Phase 3: Agent Kernel        (depends on Phase 1)
   Phase 2: Model Loader        (depends on Phase 1)
   Phase 1: Foundation          ◄─── YOU ARE HERE
================================================================================
*/

namespace Phase1 {

/*================================================================================
 FORWARD DECLARATIONS
================================================================================*/
struct PHASE1_CONTEXT;
struct CPU_CAPABILITIES;
struct HARDWARE_TOPOLOGY;
struct MEMORY_ARENA;

/*================================================================================
 CONSTANTS
================================================================================*/
constexpr uint32_t MAX_NUMA_NODES = 64;
constexpr uint32_t MAX_PROCESSORS = 1024;
constexpr uint32_t PAGE_SIZE = 0x1000;
constexpr uint32_t LARGE_PAGE_SIZE = 0x200000;
constexpr uint32_t CACHE_LINE_SIZE = 0x40;

/*================================================================================
 CPUID FEATURE FLAGS
================================================================================*/
struct CPU_CAPABILITIES {
    // Vendor identification
    char vendor_id[12];
    char brand_string[48];
    
    // Basic CPU properties
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    
    // Core topology
    uint32_t physical_cores;
    uint32_t logical_cores;
    uint32_t threads_per_core;
    uint32_t numa_nodes;
    
    // Cache hierarchy
    uint32_t l1d_cache_size;
    uint32_t l1i_cache_size;
    uint32_t l2_cache_size;
    uint32_t l3_cache_size;
    uint32_t l1d_cacheline;
    uint32_t l1i_cacheline;
    uint32_t l2_cacheline;
    uint32_t l3_cacheline;
    
    // Feature detection
    uint32_t has_sse;
    uint32_t has_sse2;
    uint32_t has_sse3;
    uint32_t has_ssse3;
    uint32_t has_sse41;
    uint32_t has_sse42;
    uint32_t has_avx;
    uint32_t has_avx2;
    uint32_t has_avx512f;
    uint32_t has_avx512dq;
    uint32_t has_avx512bw;
    uint32_t has_avx512vl;
    uint32_t has_avx512cd;
    uint32_t has_avx512er;
    uint32_t has_avx512pf;
    uint32_t has_fma;
    uint32_t has_f16c;
    uint32_t has_popcnt;
    uint32_t has_lzcnt;
    uint32_t has_bmi1;
    uint32_t has_bmi2;
    uint32_t has_aes;
    uint32_t has_sha;
    uint32_t has_rdrand;
    uint32_t has_rdseed;
    uint32_t has_tsc;
    uint32_t has_tsc_deadline;
    uint32_t has_invariant_tsc;
    
    // Extended CPU state
    uint32_t xsave_area_size;
    uint32_t max_xsave_size;
    uint32_t has_xsaveopt;
    uint32_t has_xsaves;
    uint32_t has_xsavec;
    
    // Performance
    uint64_t tsc_frequency_hz;
    uint32_t nominal_frequency_mhz;
    uint32_t max_frequency_mhz;
};

/*================================================================================
 HARDWARE TOPOLOGY
================================================================================*/
struct NUMA_NODE_INFO {
    uint32_t node_number;
    uint64_t processor_mask;
    uint64_t memory_size;
    uint64_t free_memory;
    uint32_t arena_count;
    uint32_t padding;
};

struct HARDWARE_TOPOLOGY {
    CPU_CAPABILITIES cpu;
    
    uint32_t numa_node_count;
    uint32_t padding1;
    uint64_t numa_nodes[MAX_NUMA_NODES];
    
    uint64_t total_physical_memory;
    uint64_t available_memory;
    uint64_t total_virtual_memory;
    uint64_t available_virtual;
    
    uint64_t page_size;
    uint64_t allocation_granularity;
    uint32_t processor_count;
    uint64_t active_processor_mask;
    
    uint32_t has_large_pages;
    uint32_t has_huge_pages;
    uint32_t has_numa;
    uint32_t has_processor_groups;
};

/*================================================================================
 MEMORY MANAGEMENT
================================================================================*/
struct MEMORY_ARENA {
    uint64_t base_address;
    uint64_t current_offset;
    uint64_t committed_size;
    uint64_t reserved_size;
    uint64_t block_size;
    uint32_t flags;
    uint32_t numa_node;
    uint64_t lock;  // SRWLOCK
    
    // Helper methods
    void* Allocate(size_t size, size_t alignment = 16);
    void Reset();
    size_t GetUsedMemory() const { return current_offset; }
    size_t GetAvailableMemory() const { return reserved_size - current_offset; }
};

/*================================================================================
 THREAD POOL
================================================================================*/
struct THREAD_POOL_TASK {
    uint64_t next;
    uint64_t callback;
    uint64_t context;
    uint32_t priority;
    uint32_t flags;
    uint64_t submit_time;
    uint32_t target_numa_node;
    uint32_t processor_affinity;
};

struct THREAD_POOL_WORKER {
    uint64_t thread_handle;
    uint32_t thread_id;
    uint32_t state;  // 0=IDLE, 1=BUSY, 2=SHUTDOWN
    uint64_t current_task;
    uint64_t task_count;
    uint32_t numa_node;
    uint32_t ideal_processor;
    MEMORY_ARENA worker_arena;
};

/*================================================================================
 PHASE 1 CONTEXT - Root initialization structure
================================================================================*/
struct PHASE1_CONTEXT {
    HARDWARE_TOPOLOGY topology;
    
    // Memory management
    MEMORY_ARENA system_arena;
    uint64_t numa_arenas[MAX_NUMA_NODES];
    
    // Thread pool
    uint32_t worker_thread_count;
    uint32_t io_thread_count;
    uint32_t padding1;
    uint64_t completion_port;
    
    // Synchronization primitives
    uint64_t cs_pool;
    uint64_t event_pool;
    uint64_t semaphore_pool;
    
    // Performance monitoring
    uint64_t perf_frequency;
    uint64_t perf_counter_start;
    
    // Initialization state
    uint32_t initialized;
    uint32_t init_flags;
    
    // Error handling
    uint32_t last_error_code;
    uint32_t padding2;
    uint64_t error_handler;
    
    // Logging
    uint64_t log_buffer;
    uint64_t log_write_ptr;
    uint64_t log_capacity;
    
    // Reserved for expansion
    uint8_t reserved[4096];
    
    // Helper methods
    bool IsInitialized() const { return initialized != 0; }
    const CPU_CAPABILITIES& GetCPU() const { return topology.cpu; }
    const HARDWARE_TOPOLOGY& GetTopology() const { return topology; }
    void* AllocateFromSystemArena(size_t size, size_t alignment = 16);
    void* AllocateFromNUMANode(uint32_t numa_node, size_t size, size_t alignment = 16);
};

/*================================================================================
 C++ WRAPPER CLASS - Convenient high-level interface
================================================================================*/
class Foundation {
public:
    // Singleton access
    static Foundation& GetInstance();
    
    // Initialization
    static PHASE1_CONTEXT* Initialize(uint32_t flags = 0);
    static bool IsInitialized();
    
    // Hardware information
    const CPU_CAPABILITIES& GetCPUCapabilities() const;
    const HARDWARE_TOPOLOGY& GetHardwareTopology() const;
    uint32_t GetPhysicalCoreCount() const;
    uint32_t GetLogicalCoreCount() const;
    uint32_t GetNUMANodeCount() const;
    bool HasAVX512() const;
    bool HasAVX2() const;
    bool HasAVX() const;
    uint64_t GetTSCFrequency() const;
    
    // Memory management
    void* AllocateSystemMemory(size_t size, size_t alignment = 16);
    void* AllocateNUMAMemory(uint32_t numa_node, size_t size, size_t alignment = 16);
    
    // Performance timing
    uint64_t ReadTSC() const;
    uint64_t GetElapsedMicroseconds() const;
    double GetElapsedMilliseconds() const;
    double GetElapsedSeconds() const;
    
    // Raw context access
    PHASE1_CONTEXT* GetRawContext() const { return m_context; }
    
private:
    Foundation();
    ~Foundation() = default;
    
    PHASE1_CONTEXT* m_context;
    static Foundation* s_instance;
};

/*================================================================================
 EXTERNAL C FUNCTIONS (provided by Phase1_Master.asm)
================================================================================*/

extern "C" {
    // Main initialization
    PHASE1_CONTEXT* __stdcall Phase1Initialize(uint32_t flags);
    
    // Hardware detection
    uint32_t __stdcall DetectCpuCapabilities(PHASE1_CONTEXT* ctx);
    uint32_t __stdcall DetectMemoryTopology(PHASE1_CONTEXT* ctx);
    
    // Memory management
    uint32_t __stdcall InitializeMemoryArenas(PHASE1_CONTEXT* ctx);
    void* __stdcall ArenaAllocate(MEMORY_ARENA* arena, uint64_t size, uint64_t alignment);
    
    // Timing
    uint64_t __stdcall ReadTsc(void);
    uint64_t __stdcall GetElapsedMicroseconds(PHASE1_CONTEXT* ctx);
    
    // Threading
    uint32_t __stdcall InitializeThreadPoolInfrastructure(PHASE1_CONTEXT* ctx);
    
    // Logging
    void __stdcall Phase1LogMessage(PHASE1_CONTEXT* ctx, const char* message);
    
    // Global context access
    extern PHASE1_CONTEXT* g_Phase1Context;
    extern uint32_t g_Phase1Initialized;
}

/*================================================================================
 UTILITY MACROS FOR PHASE 1 OPERATIONS
================================================================================*/

// Quick access to global context
#define PHASE1() Phase1::Foundation::GetInstance()
#define PHASE1_CTX() PHASE1().GetRawContext()

// Memory allocation helpers
#define PHASE1_MALLOC(size) PHASE1().AllocateSystemMemory(size)
#define PHASE1_MALLOC_ALIGN(size, align) PHASE1().AllocateSystemMemory(size, align)
#define PHASE1_NUMA_MALLOC(node, size) PHASE1().AllocateNUMAMemory(node, size)

// Timing helpers
#define PHASE1_CYCLES() PHASE1().ReadTSC()
#define PHASE1_MICROS() PHASE1().GetElapsedMicroseconds()
#define PHASE1_MILLIS() PHASE1().GetElapsedMilliseconds()

// Feature detection
#define PHASE1_HAS_AVX512() PHASE1().HasAVX512()
#define PHASE1_HAS_AVX2() PHASE1().HasAVX2()
#define PHASE1_CORES() PHASE1().GetPhysicalCoreCount()
#define PHASE1_THREADS() PHASE1().GetLogicalCoreCount()

}  // namespace Phase1

#endif // PHASE1_FOUNDATION_H
=======
#pragma once

#include <cstdint>
#include <cstring>
#include <windows.h>

/*
================================================================================
 PHASE1_FOUNDATION.H - Public API for RawrXD Foundation Layer
 
 This header provides the C++ interface to the x64 assembly-based Phase 1
 bootstrap layer. All higher phases (2-5) depend on this foundation.
 
 Architecture Stack:
   Phase 5: Orchestrator        (depends on Phase 1)
   Phase 4: Swarm Inference     (depends on Phase 1)
   Phase 3: Agent Kernel        (depends on Phase 1)
   Phase 2: Model Loader        (depends on Phase 1)
   Phase 1: Foundation          ◄─── YOU ARE HERE
================================================================================
*/

namespace Phase1 {

/*================================================================================
 FORWARD DECLARATIONS
================================================================================*/
struct PHASE1_CONTEXT;
struct CPU_CAPABILITIES;
struct HARDWARE_TOPOLOGY;
struct MEMORY_ARENA;

/*================================================================================
 CONSTANTS
================================================================================*/
constexpr uint32_t MAX_NUMA_NODES = 64;
constexpr uint32_t MAX_PROCESSORS = 1024;
constexpr uint32_t PAGE_SIZE = 0x1000;
constexpr uint32_t LARGE_PAGE_SIZE = 0x200000;
constexpr uint32_t CACHE_LINE_SIZE = 0x40;

/*================================================================================
 CPUID FEATURE FLAGS
================================================================================*/
struct CPU_CAPABILITIES {
    // Vendor identification
    char vendor_id[12];
    char brand_string[48];
    
    // Basic CPU properties
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    
    // Core topology
    uint32_t physical_cores;
    uint32_t logical_cores;
    uint32_t threads_per_core;
    uint32_t numa_nodes;
    
    // Cache hierarchy
    uint32_t l1d_cache_size;
    uint32_t l1i_cache_size;
    uint32_t l2_cache_size;
    uint32_t l3_cache_size;
    uint32_t l1d_cacheline;
    uint32_t l1i_cacheline;
    uint32_t l2_cacheline;
    uint32_t l3_cacheline;
    
    // Feature detection
    uint32_t has_sse;
    uint32_t has_sse2;
    uint32_t has_sse3;
    uint32_t has_ssse3;
    uint32_t has_sse41;
    uint32_t has_sse42;
    uint32_t has_avx;
    uint32_t has_avx2;
    uint32_t has_avx512f;
    uint32_t has_avx512dq;
    uint32_t has_avx512bw;
    uint32_t has_avx512vl;
    uint32_t has_avx512cd;
    uint32_t has_avx512er;
    uint32_t has_avx512pf;
    uint32_t has_fma;
    uint32_t has_f16c;
    uint32_t has_popcnt;
    uint32_t has_lzcnt;
    uint32_t has_bmi1;
    uint32_t has_bmi2;
    uint32_t has_aes;
    uint32_t has_sha;
    uint32_t has_rdrand;
    uint32_t has_rdseed;
    uint32_t has_tsc;
    uint32_t has_tsc_deadline;
    uint32_t has_invariant_tsc;
    
    // Extended CPU state
    uint32_t xsave_area_size;
    uint32_t max_xsave_size;
    uint32_t has_xsaveopt;
    uint32_t has_xsaves;
    uint32_t has_xsavec;
    
    // Performance
    uint64_t tsc_frequency_hz;
    uint32_t nominal_frequency_mhz;
    uint32_t max_frequency_mhz;
};

/*================================================================================
 HARDWARE TOPOLOGY
================================================================================*/
struct NUMA_NODE_INFO {
    uint32_t node_number;
    uint64_t processor_mask;
    uint64_t memory_size;
    uint64_t free_memory;
    uint32_t arena_count;
    uint32_t padding;
};

struct HARDWARE_TOPOLOGY {
    CPU_CAPABILITIES cpu;
    
    uint32_t numa_node_count;
    uint32_t padding1;
    uint64_t numa_nodes[MAX_NUMA_NODES];
    
    uint64_t total_physical_memory;
    uint64_t available_memory;
    uint64_t total_virtual_memory;
    uint64_t available_virtual;
    
    uint64_t page_size;
    uint64_t allocation_granularity;
    uint32_t processor_count;
    uint64_t active_processor_mask;
    
    uint32_t has_large_pages;
    uint32_t has_huge_pages;
    uint32_t has_numa;
    uint32_t has_processor_groups;
};

/*================================================================================
 MEMORY MANAGEMENT
================================================================================*/
struct MEMORY_ARENA {
    uint64_t base_address;
    uint64_t current_offset;
    uint64_t committed_size;
    uint64_t reserved_size;
    uint64_t block_size;
    uint32_t flags;
    uint32_t numa_node;
    uint64_t lock;  // SRWLOCK
    
    // Helper methods
    void* Allocate(size_t size, size_t alignment = 16);
    void Reset();
    size_t GetUsedMemory() const { return current_offset; }
    size_t GetAvailableMemory() const { return reserved_size - current_offset; }
};

/*================================================================================
 THREAD POOL
================================================================================*/
struct THREAD_POOL_TASK {
    uint64_t next;
    uint64_t callback;
    uint64_t context;
    uint32_t priority;
    uint32_t flags;
    uint64_t submit_time;
    uint32_t target_numa_node;
    uint32_t processor_affinity;
};

struct THREAD_POOL_WORKER {
    uint64_t thread_handle;
    uint32_t thread_id;
    uint32_t state;  // 0=IDLE, 1=BUSY, 2=SHUTDOWN
    uint64_t current_task;
    uint64_t task_count;
    uint32_t numa_node;
    uint32_t ideal_processor;
    MEMORY_ARENA worker_arena;
};

/*================================================================================
 PHASE 1 CONTEXT - Root initialization structure
================================================================================*/
struct PHASE1_CONTEXT {
    HARDWARE_TOPOLOGY topology;
    
    // Memory management
    MEMORY_ARENA system_arena;
    uint64_t numa_arenas[MAX_NUMA_NODES];
    
    // Thread pool
    uint32_t worker_thread_count;
    uint32_t io_thread_count;
    uint32_t padding1;
    uint64_t completion_port;
    
    // Synchronization primitives
    uint64_t cs_pool;
    uint64_t event_pool;
    uint64_t semaphore_pool;
    
    // Performance monitoring
    uint64_t perf_frequency;
    uint64_t perf_counter_start;
    
    // Initialization state
    uint32_t initialized;
    uint32_t init_flags;
    
    // Error handling
    uint32_t last_error_code;
    uint32_t padding2;
    uint64_t error_handler;
    
    // Logging
    uint64_t log_buffer;
    uint64_t log_write_ptr;
    uint64_t log_capacity;
    
    // Reserved for expansion
    uint8_t reserved[4096];
    
    // Helper methods
    bool IsInitialized() const { return initialized != 0; }
    const CPU_CAPABILITIES& GetCPU() const { return topology.cpu; }
    const HARDWARE_TOPOLOGY& GetTopology() const { return topology; }
    void* AllocateFromSystemArena(size_t size, size_t alignment = 16);
    void* AllocateFromNUMANode(uint32_t numa_node, size_t size, size_t alignment = 16);
};

/*================================================================================
 C++ WRAPPER CLASS - Convenient high-level interface
================================================================================*/
class Foundation {
public:
    // Singleton access
    static Foundation& GetInstance();
    
    // Initialization
    static PHASE1_CONTEXT* Initialize(uint32_t flags = 0);
    static bool IsInitialized();
    
    // Hardware information
    const CPU_CAPABILITIES& GetCPUCapabilities() const;
    const HARDWARE_TOPOLOGY& GetHardwareTopology() const;
    uint32_t GetPhysicalCoreCount() const;
    uint32_t GetLogicalCoreCount() const;
    uint32_t GetNUMANodeCount() const;
    bool HasAVX512() const;
    bool HasAVX2() const;
    bool HasAVX() const;
    uint64_t GetTSCFrequency() const;
    
    // Memory management
    void* AllocateSystemMemory(size_t size, size_t alignment = 16);
    void* AllocateNUMAMemory(uint32_t numa_node, size_t size, size_t alignment = 16);
    
    // Performance timing
    uint64_t ReadTSC() const;
    uint64_t GetElapsedMicroseconds() const;
    double GetElapsedMilliseconds() const;
    double GetElapsedSeconds() const;
    
    // Raw context access
    PHASE1_CONTEXT* GetRawContext() const { return m_context; }
    
private:
    Foundation();
    ~Foundation() = default;
    
    PHASE1_CONTEXT* m_context;
    static Foundation* s_instance;
};

/*================================================================================
 EXTERNAL C FUNCTIONS (provided by Phase1_Master.asm)
================================================================================*/

extern "C" {
    // Main initialization
    PHASE1_CONTEXT* __stdcall Phase1Initialize(uint32_t flags);
    
    // Hardware detection
    uint32_t __stdcall DetectCpuCapabilities(PHASE1_CONTEXT* ctx);
    uint32_t __stdcall DetectMemoryTopology(PHASE1_CONTEXT* ctx);
    
    // Memory management
    uint32_t __stdcall InitializeMemoryArenas(PHASE1_CONTEXT* ctx);
    void* __stdcall ArenaAllocate(MEMORY_ARENA* arena, uint64_t size, uint64_t alignment);
    
    // Timing
    uint64_t __stdcall ReadTsc(void);
    uint64_t __stdcall GetElapsedMicroseconds(PHASE1_CONTEXT* ctx);
    
    // Threading
    uint32_t __stdcall InitializeThreadPoolInfrastructure(PHASE1_CONTEXT* ctx);
    
    // Logging
    void __stdcall Phase1LogMessage(PHASE1_CONTEXT* ctx, const char* message);
    
    // Global context access
    extern PHASE1_CONTEXT* g_Phase1Context;
    extern uint32_t g_Phase1Initialized;
}  // extern "C"

/*================================================================================
 UTILITY MACROS FOR PHASE 1 OPERATIONS
================================================================================*/

// Quick access to global context
#define PHASE1() Phase1::Foundation::GetInstance()
#define PHASE1_CTX() PHASE1().GetRawContext()

// Memory allocation helpers
#define PHASE1_MALLOC(size) PHASE1().AllocateSystemMemory(size)
#define PHASE1_MALLOC_ALIGN(size, align) PHASE1().AllocateSystemMemory(size, align)
#define PHASE1_NUMA_MALLOC(node, size) PHASE1().AllocateNUMAMemory(node, size)

// Timing helpers
#define PHASE1_CYCLES() PHASE1().ReadTSC()
#define PHASE1_MICROS() PHASE1().GetElapsedMicroseconds()
#define PHASE1_MILLIS() PHASE1().GetElapsedMilliseconds()

// Feature detection
#define PHASE1_HAS_AVX512() PHASE1().HasAVX512()
#define PHASE1_HAS_AVX2() PHASE1().HasAVX2()
#define PHASE1_CORES() PHASE1().GetPhysicalCoreCount()
#define PHASE1_THREADS() PHASE1().GetLogicalCoreCount()

}  // namespace Phase1
>>>>>>> origin/main
