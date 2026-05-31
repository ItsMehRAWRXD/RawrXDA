// progressive_engine.h - Progressive Layer Loading Engine
// Manages layer loading, prefetching, and memory tier management
// Part of RawrXD 14-Day Production-Ready Expansion

#ifndef PROGRESSIVE_ENGINE_H
#define PROGRESSIVE_ENGINE_H

#include "auto_configurator.h"
#include "live_hotpatch.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MEMORY TIERS
// ============================================================================

typedef enum {
    MEMORY_TIER_VRAM = 0,    // GPU VRAM - fastest
    MEMORY_TIER_RAM = 1,      // System RAM - medium
    MEMORY_TIER_DISK = 2,     // SSD/HDD - slowest
    MEMORY_TIER_COUNT = 3
} MemoryTier;

// ============================================================================
// LAYER STATE
// ============================================================================

typedef struct {
    uint32_t layer_index;
    MemoryTier current_tier;
    MemoryTier target_tier;
    
    // Layer metadata
    uint64_t size_bytes;
    uint32_t num_heads;
    uint32_t num_experts;
    float importance_score;
    
    // Loading state
    bool is_loaded;
    bool is_loading;
    bool is_prefetched;
    bool needs_recompute;
    
    // Performance tracking
    uint64_t load_time_ns;
    uint64_t last_access_time;
    uint32_t access_count;
    uint32_t cache_misses;
    
    // Quantization state
    QuantType current_quant;
    QuantType target_quant;
    float sparsity;
    
} LayerState;

// ============================================================================
// PREFETCH QUEUE
// ============================================================================

typedef struct {
    uint32_t layer_indices[32];
    uint32_t count;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    bool is_processing;
} PrefetchQueue;

// ============================================================================
// UNIFIED MEMORY MANAGER
// ============================================================================

typedef struct {
    // Memory pools
    uint64_t vram_pool_size;
    uint64_t ram_pool_size;
    uint64_t disk_pool_size;
    
    // Current usage
    uint64_t vram_used;
    uint64_t ram_used;
    uint64_t disk_used;
    
    // Allocation tracking
    void* vram_pool;
    void* ram_pool;
    void* disk_pool;
    
    // Statistics
    uint64_t vram_allocations;
    uint64_t ram_allocations;
    uint64_t disk_allocations;
    uint64_t vram_frees;
    uint64_t ram_frees;
    uint64_t disk_frees;
    
} UnifiedMemory;

// ============================================================================
// PROGRESSIVE ENGINE
// ============================================================================

typedef struct {
    AutoConfig* config;
    LiveHotpatch* hotpatch;
    UnifiedMemory* memory;
    
    // Layer management
    LayerState* layers;
    uint32_t num_layers;
    uint32_t* layer_priority;     // Importance scores
    uint8_t* layer_resident;      // Which memory tier
    float* layer_importance;      // Dynamic importance
    
    // Prefetch queue
    PrefetchQueue prefetch_queue;
    
    // Inference state
    uint32_t current_layer;
    uint32_t current_token;
    uint32_t context_length;
    
    // Statistics
    uint64_t total_inferences;
    uint64_t total_layer_loads;
    uint64_t total_layer_swaps;
    uint64_t total_prefetch_hits;
    uint64_t total_prefetch_misses;
    uint64_t total_cache_hits;
    uint64_t total_cache_misses;
    
    // Performance tracking
    uint64_t total_inference_time_ns;
    uint64_t total_load_time_ns;
    uint64_t total_swap_time_ns;
    
    // Configuration
    bool prefetch_enabled;
    bool hotpatch_enabled;
    bool auto_tier_enabled;
    uint32_t prefetch_depth;
    uint32_t max_layers_in_vram;
    uint32_t max_layers_in_ram;
    
} ProgressiveEngine;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Lifecycle
ProgressiveEngine* progressive_engine_create(AutoConfig* config);
void progressive_engine_destroy(ProgressiveEngine* engine);

// Memory management
UnifiedMemory* unified_memory_create(uint64_t vram_size, uint64_t ram_size, uint64_t disk_size);
void unified_memory_destroy(UnifiedMemory* memory);
void* memory_allocate(UnifiedMemory* memory, MemoryTier tier, uint64_t size);
void memory_free(UnifiedMemory* memory, MemoryTier tier, void* ptr);
uint64_t memory_get_available(UnifiedMemory* memory, MemoryTier tier);

// Layer management
void init_layer_states(ProgressiveEngine* engine);
void update_layer_importance(ProgressiveEngine* engine, uint32_t layer, float importance);
void optimize_resident_layers(ProgressiveEngine* engine);
void rebalance_memory_tiers(ProgressiveEngine* engine);

// Inference
int progressive_inference(ProgressiveEngine* engine, uint32_t token);
void progressive_batch_inference(ProgressiveEngine* engine, uint32_t* tokens, uint32_t count);
int progressive_inference_with_prefetch(ProgressiveEngine* engine, uint32_t token);

// Prefetching
void prefetch_layer(ProgressiveEngine* engine, uint32_t layer_idx);
void prefetch_next_layers(ProgressiveEngine* engine, uint32_t current_layer, uint32_t count);
void process_prefetch_queue(ProgressiveEngine* engine);
void clear_prefetch_queue(ProgressiveEngine* engine);

// Layer loading
uint64_t load_layer_to_vram(ProgressiveEngine* engine, uint32_t layer_idx);
uint64_t load_layer_to_ram(ProgressiveEngine* engine, uint32_t layer_idx);
uint64_t swap_layer_out(ProgressiveEngine* engine, uint32_t layer_idx, MemoryTier target_tier);
uint64_t swap_layer_in(ProgressiveEngine* engine, uint32_t layer_idx, MemoryTier target_tier);

// Statistics
void get_progressive_stats(ProgressiveEngine* engine, ProgressiveStats* stats);
void reset_progressive_stats(ProgressiveEngine* engine);
void print_progressive_stats(ProgressiveEngine* engine);

// Configuration
void set_prefetch_depth(ProgressiveEngine* engine, uint32_t depth);
void set_max_layers(ProgressiveEngine* engine, uint32_t vram_layers, uint32_t ram_layers);
void enable_prefetch(ProgressiveEngine* engine, bool enable);
void enable_hotpatch(ProgressiveEngine* engine, bool enable);
void enable_auto_tier(ProgressiveEngine* engine, bool enable);

// Utility
float calculate_layer_importance(ProgressiveEngine* engine, uint32_t layer_idx);
uint64_t estimate_layer_size(ProgressiveEngine* engine, uint32_t layer_idx, QuantType quant);
uint32_t find_least_important_layer(ProgressiveEngine* engine, MemoryTier tier);
uint32_t find_most_important_layer(ProgressiveEngine* engine, MemoryTier tier);

// ============================================================================
// STATISTICS STRUCTURE
// ============================================================================

typedef struct {
    uint64_t total_inferences;
    uint64_t total_layer_loads;
    uint64_t total_layer_swaps;
    uint64_t total_prefetch_hits;
    uint64_t total_prefetch_misses;
    uint64_t total_cache_hits;
    uint64_t total_cache_misses;
    
    float avg_inference_time_ns;
    float avg_load_time_ns;
    float avg_swap_time_ns;
    
    float prefetch_hit_rate;
    float cache_hit_rate;
    
    uint64_t vram_used;
    uint64_t ram_used;
    uint64_t disk_used;
    
    uint32_t layers_in_vram;
    uint32_t layers_in_ram;
    uint32_t layers_on_disk;
    
} ProgressiveStats;

#ifdef __cplusplus
}
#endif

#endif // PROGRESSIVE_ENGINE_H
