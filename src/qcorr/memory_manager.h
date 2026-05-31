// ============================================================================
// MEMORY_MANAGER - Unified Memory for 800B Models on 16GB VRAM
// ============================================================================

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Memory Tiers
// ============================================================================
typedef enum {
    MEM_VRAM = 0,      // GPU memory (fastest, limited)
    MEM_RAM = 1,       // System RAM (medium speed)
    MEM_DISK = 2,      // SSD/HDD (slowest, unlimited)
    MEM_NETWORK = 3    // Remote storage
} MemoryTier;

// ============================================================================
// Memory Pool
// ============================================================================
typedef struct {
    void* data;
    size_t size;
    size_t used;
    MemoryTier tier;
    uint32_t priority;
    uint64_t last_access;
    uint32_t ref_count;
    char tag[64];
} MemoryPool;

// ============================================================================
// Layer Residency Tracking
// ============================================================================
typedef struct {
    uint32_t layer_idx;
    MemoryTier current_tier;
    MemoryTier desired_tier;
    uint32_t access_count;
    uint64_t load_time_us;
    uint64_t last_access;
    float importance_score;
} LayerResidency;

// ============================================================================
// Unified Memory System
// ============================================================================
typedef struct {
    // Memory pools
    MemoryPool* vram_pools;
    uint32_t num_vram_pools;
    
    MemoryPool* ram_pools;
    uint32_t num_ram_pools;
    
    MemoryPool* disk_pools;
    uint32_t num_disk_pools;
    
    // Layer tracking
    LayerResidency* layer_status;
    uint32_t num_layers;
    
    // Statistics
    uint64_t vram_capacity;
    uint64_t vram_used;
    uint64_t ram_capacity;
    uint64_t ram_used;
    uint64_t disk_capacity;
    
    // Configuration
    uint32_t max_layers_in_vram;
    uint32_t prefetch_layers;
    uint32_t offload_threshold;
    
    // Performance tracking
    uint64_t total_vram_swaps;
    uint64_t total_ram_swaps;
    uint64_t total_disk_reads;
    uint64_t cache_hits;
    uint64_t cache_misses;
} UnifiedMemory;

// ============================================================================
// Memory-Mapped Model
// ============================================================================
typedef struct {
    void* base_ptr;
    size_t file_size;
    int fd;
    
    uint64_t* layer_offsets;
    uint32_t* layer_sizes;
    uint32_t num_layers;
    
    uint8_t* layer_loaded;
    void** layer_ptrs;
    
    uint32_t* prefetch_queue;
    uint32_t prefetch_count;
} MMapModel;

// ============================================================================
// Memory Strategy for 16GB
// ============================================================================
typedef struct {
    uint32_t layers_in_vram;
    uint32_t layers_in_ram;
    uint32_t layers_on_disk;
    
    uint32_t embed_in_vram;
    uint32_t embed_cache_size;
    
    uint32_t kv_cache_layers;
    uint32_t kv_cache_size;
    
    uint32_t recompute_layers;
} MemoryStrategy16GB;

// ============================================================================
// Function Declarations
// ============================================================================
UnifiedMemory* umemory_create(uint64_t vram_size, uint64_t ram_size);
void umemory_destroy(UnifiedMemory* mem);

void* umemory_alloc(UnifiedMemory* mem, size_t size, MemoryTier tier, const char* tag);
void umemory_free(UnifiedMemory* mem, void* ptr);
int umemory_move(UnifiedMemory* mem, void* ptr, MemoryTier new_tier);

int umemory_load_layer(UnifiedMemory* mem, MMapModel* model, uint32_t layer_idx);
int umemory_unload_layer(UnifiedMemory* mem, MMapModel* model, uint32_t layer_idx);
int umemory_offload_layer(UnifiedMemory* mem, MMapModel* model, uint32_t layer_idx, MemoryTier target_tier);

void umemory_update_importance(UnifiedMemory* mem, uint32_t layer_idx, float score);
void umemory_make_space(UnifiedMemory* mem, size_t required, MemoryTier tier);

uint32_t umemory_evict_lru(UnifiedMemory* mem, MemoryTier tier);
void umemory_prefetch_next(UnifiedMemory* mem, MMapModel* model, uint32_t current_layer);

void umemory_print_stats(UnifiedMemory* mem);
float umemory_get_hit_rate(UnifiedMemory* mem);

MMapModel* mmap_model_load(const char* path);
void mmap_model_unload(MMapModel* model);

MemoryStrategy16GB get_recommended_strategy_16gb(void);

#ifdef __cplusplus
}
#endif

#endif // MEMORY_MANAGER_H
