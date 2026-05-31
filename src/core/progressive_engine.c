// progressive_engine.c - Progressive Layer Loading Engine Implementation
// Manages layer loading, prefetching, and memory tier management
// Part of RawrXD 14-Day Production-Ready Expansion

#define PROGRESSIVE_ENGINE_IMPLEMENTATION
#include "progressive_engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// LIFECYCLE
// ============================================================================

ProgressiveEngine* progressive_engine_create(AutoConfig* config) {
    ProgressiveEngine* engine = (ProgressiveEngine*)calloc(1, sizeof(ProgressiveEngine));
    if (!engine) return NULL;
    
    engine->config = config;
    
    // Default configuration
    engine->num_layers = 80;  // Default for large models
    engine->prefetch_depth = 3;
    engine->max_layers_in_vram = 40;
    engine->max_layers_in_ram = 30;
    engine->prefetch_enabled = true;
    engine->hotpatch_enabled = true;
    engine->auto_tier_enabled = true;
    
    // Allocate layer management arrays
    engine->layers = (LayerState*)calloc(engine->num_layers, sizeof(LayerState));
    engine->layer_priority = (uint32_t*)calloc(engine->num_layers, sizeof(uint32_t));
    engine->layer_resident = (uint8_t*)calloc(engine->num_layers, sizeof(uint8_t));
    engine->layer_importance = (float*)calloc(engine->num_layers, sizeof(float));
    
    if (!engine->layers || !engine->layer_priority || 
        !engine->layer_resident || !engine->layer_importance) {
        progressive_engine_destroy(engine);
        return NULL;
    }
    
    // Initialize layer states
    init_layer_states(engine);
    
    // Initialize prefetch queue
    engine->prefetch_queue.capacity = 32;
    engine->prefetch_queue.count = 0;
    engine->prefetch_queue.head = 0;
    engine->prefetch_queue.tail = 0;
    engine->prefetch_queue.is_processing = false;
    
    return engine;
}

void progressive_engine_destroy(ProgressiveEngine* engine) {
    if (!engine) return;
    
    free(engine->layers);
    free(engine->layer_priority);
    free(engine->layer_resident);
    free(engine->layer_importance);
    
    if (engine->memory) {
        unified_memory_destroy(engine->memory);
    }
    
    free(engine);
}

// ============================================================================
// MEMORY MANAGEMENT
// ============================================================================

UnifiedMemory* unified_memory_create(uint64_t vram_size, uint64_t ram_size, uint64_t disk_size) {
    UnifiedMemory* memory = (UnifiedMemory*)calloc(1, sizeof(UnifiedMemory));
    if (!memory) return NULL;
    
    memory->vram_pool_size = vram_size;
    memory->ram_pool_size = ram_size;
    memory->disk_pool_size = disk_size;
    
    // In practice, would allocate actual memory pools
    // For now, just track sizes
    
    return memory;
}

void unified_memory_destroy(UnifiedMemory* memory) {
    if (!memory) return;
    
    // Free pools if allocated
    if (memory->vram_pool) free(memory->vram_pool);
    if (memory->ram_pool) free(memory->ram_pool);
    if (memory->disk_pool) free(memory->disk_pool);
    
    free(memory);
}

void* memory_allocate(UnifiedMemory* memory, MemoryTier tier, uint64_t size) {
    if (!memory) return NULL;
    
    void* ptr = NULL;
    
    switch (tier) {
        case MEMORY_TIER_VRAM:
            if (memory->vram_used + size <= memory->vram_pool_size) {
                ptr = malloc((size_t)size);  // Would use GPU memory in practice
                if (ptr) {
                    memory->vram_used += size;
                    memory->vram_allocations++;
                }
            }
            break;
            
        case MEMORY_TIER_RAM:
            if (memory->ram_used + size <= memory->ram_pool_size) {
                ptr = malloc((size_t)size);
                if (ptr) {
                    memory->ram_used += size;
                    memory->ram_allocations++;
                }
            }
            break;
            
        case MEMORY_TIER_DISK:
            // Would use memory-mapped file in practice
            ptr = malloc((size_t)size);
            if (ptr) {
                memory->disk_used += size;
                memory->disk_allocations++;
            }
            break;
            
        default:
            break;
    }
    
    return ptr;
}

void memory_free(UnifiedMemory* memory, MemoryTier tier, void* ptr) {
    if (!memory || !ptr) return;
    
    free(ptr);
    
    switch (tier) {
        case MEMORY_TIER_VRAM:
            memory->vram_frees++;
            break;
        case MEMORY_TIER_RAM:
            memory->ram_frees++;
            break;
        case MEMORY_TIER_DISK:
            memory->disk_frees++;
            break;
        default:
            break;
    }
}

uint64_t memory_get_available(UnifiedMemory* memory, MemoryTier tier) {
    if (!memory) return 0;
    
    switch (tier) {
        case MEMORY_TIER_VRAM:
            return memory->vram_pool_size - memory->vram_used;
        case MEMORY_TIER_RAM:
            return memory->ram_pool_size - memory->ram_used;
        case MEMORY_TIER_DISK:
            return memory->disk_pool_size - memory->disk_used;
        default:
            return 0;
    }
}

// ============================================================================
// LAYER MANAGEMENT
// ============================================================================

void init_layer_states(ProgressiveEngine* engine) {
    if (!engine) return;
    
    for (uint32_t i = 0; i < engine->num_layers; i++) {
        LayerState* layer = &engine->layers[i];
        memset(layer, 0, sizeof(LayerState));
        
        layer->layer_index = i;
        layer->current_tier = MEMORY_TIER_DISK;
        layer->target_tier = MEMORY_TIER_VRAM;
        layer->is_loaded = false;
        layer->is_loading = false;
        layer->is_prefetched = false;
        layer->needs_recompute = false;
        
        // Default layer size (would be computed from model)
        layer->size_bytes = 1024 * 1024 * 1024;  // 1GB per layer
        layer->num_heads = 32;
        layer->num_experts = 0;
        
        // Importance: early and late layers more important
        if (i < 8) {
            layer->importance_score = 1.5f;  // Embedding layers
        } else if (i >= engine->num_layers - 8) {
            layer->importance_score = 1.3f;  // Output layers
        } else {
            layer->importance_score = 1.0f;  // Middle layers
        }
        
        // Default quantization
        layer->current_quant = QINT4;
        layer->target_quant = QINT4;
        layer->sparsity = 0.0f;
        
        // Priority (reverse order - later layers first to evict)
        engine->layer_priority[i] = engine->num_layers - 1 - i;
        engine->layer_resident[i] = 3;  // Start on disk
        engine->layer_importance[i] = layer->importance_score;
    }
}

void update_layer_importance(ProgressiveEngine* engine, uint32_t layer, float importance) {
    if (!engine || layer >= engine->num_layers) return;
    
    engine->layer_importance[layer] = importance;
    engine->layers[layer].importance_score = importance;
    
    // Re-sort priorities if needed
    // (would use priority queue in practice)
}

void optimize_resident_layers(ProgressiveEngine* engine) {
    if (!engine || !engine->config) return;
    
    // Calculate memory available
    uint64_t vram_available = engine->config->hw.vram_bytes;
    uint64_t ram_available = engine->config->hw.ram_bytes;
    
    // Calculate layer sizes
    float layer_size_gb;
    if (engine->config->quant.weight_quant == QINT4) {
        layer_size_gb = 1.25f;  // 1.25GB per layer for INT4
    } else if (engine->config->quant.weight_quant == QINT8) {
        layer_size_gb = 2.5f;   // 2.5GB per layer for INT8
    } else {
        layer_size_gb = 10.0f;  // 10GB per layer for FP16
    }
    
    uint64_t layer_size = (uint64_t)(layer_size_gb * 1024 * 1024 * 1024);
    
    // How many layers fit in VRAM?
    uint32_t vram_layers = (uint32_t)(vram_available / layer_size);
    vram_layers = (vram_layers > engine->num_layers) ? engine->num_layers : vram_layers;
    
    // How many in RAM?
    uint64_t ram_for_layers = ram_available - 2048 * 1024 * 1024;  // Leave 2GB for system
    uint32_t ram_layers = (uint32_t)(ram_for_layers / layer_size);
    ram_layers = (ram_layers > engine->num_layers - vram_layers) ? 
                  engine->num_layers - vram_layers : ram_layers;
    
    // Update config
    engine->config->quant.layers_in_vram = vram_layers;
    engine->config->quant.layers_in_ram = ram_layers;
    
    // Mark layers by tier
    for (uint32_t i = 0; i < engine->num_layers; i++) {
        if (i < vram_layers) {
            engine->layer_resident[i] = 1;  // VRAM
            engine->layers[i].current_tier = MEMORY_TIER_VRAM;
        } else if (i < vram_layers + ram_layers) {
            engine->layer_resident[i] = 2;  // RAM
            engine->layers[i].current_tier = MEMORY_TIER_RAM;
        } else {
            engine->layer_resident[i] = 3;  // Disk
            engine->layers[i].current_tier = MEMORY_TIER_DISK;
        }
    }
    
    engine->max_layers_in_vram = vram_layers;
    engine->max_layers_in_ram = ram_layers;
}

void rebalance_memory_tiers(ProgressiveEngine* engine) {
    if (!engine) return;
    
    // Count layers in each tier
    uint32_t vram_count = 0;
    uint32_t ram_count = 0;
    uint32_t disk_count = 0;
    
    for (uint32_t i = 0; i < engine->num_layers; i++) {
        switch (engine->layers[i].current_tier) {
            case MEMORY_TIER_VRAM: vram_count++; break;
            case MEMORY_TIER_RAM: ram_count++; break;
            case MEMORY_TIER_DISK: disk_count++; break;
            default: break;
        }
    }
    
    // Check if rebalancing needed
    if (vram_count > engine->max_layers_in_vram) {
        // Need to move some layers out of VRAM
        uint32_t to_move = vram_count - engine->max_layers_in_vram;
        
        // Find least important layers in VRAM
        for (uint32_t i = 0; i < to_move; i++) {
            uint32_t victim = find_least_important_layer(engine, MEMORY_TIER_VRAM);
            if (victim < engine->num_layers) {
                swap_layer_out(engine, victim, MEMORY_TIER_RAM);
            }
        }
    }
    
    if (ram_count > engine->max_layers_in_ram) {
        // Need to move some layers out of RAM
        uint32_t to_move = ram_count - engine->max_layers_in_ram;
        
        // Find least important layers in RAM
        for (uint32_t i = 0; i < to_move; i++) {
            uint32_t victim = find_least_important_layer(engine, MEMORY_TIER_RAM);
            if (victim < engine->num_layers) {
                swap_layer_out(engine, victim, MEMORY_TIER_DISK);
            }
        }
    }
}

// ============================================================================
// INFERENCE
// ============================================================================

int progressive_inference(ProgressiveEngine* engine, uint32_t token) {
    if (!engine) return -1;
    
    uint64_t start_time = get_time_ns();
    
    // Process token through all layers progressively
    for (uint32_t layer = 0; layer < engine->num_layers; layer++) {
        // Check if layer is resident
        if (engine->layers[layer].current_tier != MEMORY_TIER_VRAM) {
            // Need to load layer
            engine->total_layer_loads++;
            
            // If not in RAM either, need to load from disk
            if (engine->layers[layer].current_tier == MEMORY_TIER_DISK) {
                // Load from disk to RAM first
                load_layer_to_ram(engine, layer);
            }
            
            // Move layer to VRAM
            load_layer_to_vram(engine, layer);
            
            // Update resident status
            engine->layers[layer].current_tier = MEMORY_TIER_VRAM;
            engine->layers[layer].is_loaded = true;
        }
        
        // Update access tracking
        engine->layers[layer].access_count++;
        engine->layers[layer].last_access_time = get_time_ns();
        
        // Process layer (would call actual inference kernel)
        // ...
        
        // Prefetch next layer(s)
        if (engine->prefetch_enabled && layer < engine->num_layers - 1) {
            prefetch_next_layers(engine, layer, engine->prefetch_depth);
        }
    }
    
    engine->total_inferences++;
    engine->current_token = token;
    engine->context_length++;
    
    engine->total_inference_time_ns += get_time_ns() - start_time;
    
    return 0;  // Success
}

void progressive_batch_inference(ProgressiveEngine* engine, uint32_t* tokens, uint32_t count) {
    if (!engine || !tokens) return;
    
    for (uint32_t i = 0; i < count; i++) {
        progressive_inference(engine, tokens[i]);
    }
}

int progressive_inference_with_prefetch(ProgressiveEngine* engine, uint32_t token) {
    if (!engine) return -1;
    
    // Process current token
    int result = progressive_inference(engine, token);
    
    // Process prefetch queue
    process_prefetch_queue(engine);
    
    return result;
}

// ============================================================================
// PREFETCHING
// ============================================================================

void prefetch_layer(ProgressiveEngine* engine, uint32_t layer_idx) {
    if (!engine || layer_idx >= engine->num_layers) return;
    
    PrefetchQueue* queue = &engine->prefetch_queue;
    
    // Add to queue if not already there
    for (uint32_t i = 0; i < queue->count; i++) {
        if (queue->layer_indices[i] == layer_idx) {
            return;  // Already queued
        }
    }
    
    if (queue->count < queue->capacity) {
        queue->layer_indices[queue->tail] = layer_idx;
        queue->tail = (queue->tail + 1) % queue->capacity;
        queue->count++;
        
        engine->layers[layer_idx].is_prefetched = true;
    }
}

void prefetch_next_layers(ProgressiveEngine* engine, uint32_t current_layer, uint32_t count) {
    if (!engine) return;
    
    for (uint32_t i = 1; i <= count; i++) {
        uint32_t next_layer = current_layer + i;
        if (next_layer < engine->num_layers) {
            prefetch_layer(engine, next_layer);
        }
    }
}

void process_prefetch_queue(ProgressiveEngine* engine) {
    if (!engine) return;
    
    PrefetchQueue* queue = &engine->prefetch_queue;
    
    if (queue->count == 0 || queue->is_processing) {
        return;
    }
    
    queue->is_processing = true;
    
    while (queue->count > 0) {
        uint32_t layer_idx = queue->layer_indices[queue->head];
        queue->head = (queue->head + 1) % queue->capacity;
        queue->count--;
        
        // Load layer if not already in VRAM
        if (engine->layers[layer_idx].current_tier != MEMORY_TIER_VRAM) {
            uint64_t load_time = load_layer_to_vram(engine, layer_idx);
            
            if (load_time > 0) {
                engine->total_prefetch_hits++;
            } else {
                engine->total_prefetch_misses++;
            }
        }
    }
    
    queue->is_processing = false;
}

void clear_prefetch_queue(ProgressiveEngine* engine) {
    if (!engine) return;
    
    engine->prefetch_queue.count = 0;
    engine->prefetch_queue.head = 0;
    engine->prefetch_queue.tail = 0;
    engine->prefetch_queue.is_processing = false;
}

// ============================================================================
// LAYER LOADING
// ============================================================================

uint64_t load_layer_to_vram(ProgressiveEngine* engine, uint32_t layer_idx) {
    if (!engine || layer_idx >= engine->num_layers) return 0;
    
    uint64_t start_time = get_time_ns();
    
    LayerState* layer = &engine->layers[layer_idx];
    
    // Check if already in VRAM
    if (layer->current_tier == MEMORY_TIER_VRAM) {
        engine->total_cache_hits++;
        return 0;
    }
    
    // Need to swap out if VRAM is full
    uint64_t vram_available = memory_get_available(engine->memory, MEMORY_TIER_VRAM);
    if (vram_available < layer->size_bytes) {
        // Find victim layer to swap out
        uint32_t victim = find_least_important_layer(engine, MEMORY_TIER_VRAM);
        if (victim < engine->num_layers) {
            swap_layer_out(engine, victim, MEMORY_TIER_RAM);
        }
    }
    
    // Load layer
    // In practice, would use CUDA memcpy or similar
    layer->current_tier = MEMORY_TIER_VRAM;
    layer->is_loaded = true;
    layer->load_time_ns = get_time_ns() - start_time;
    
    engine->total_layer_loads++;
    engine->total_load_time_ns += layer->load_time_ns;
    
    return layer->load_time_ns;
}

uint64_t load_layer_to_ram(ProgressiveEngine* engine, uint32_t layer_idx) {
    if (!engine || layer_idx >= engine->num_layers) return 0;
    
    uint64_t start_time = get_time_ns();
    
    LayerState* layer = &engine->layers[layer_idx];
    
    // Check if already in RAM or VRAM
    if (layer->current_tier == MEMORY_TIER_RAM || 
        layer->current_tier == MEMORY_TIER_VRAM) {
        return 0;
    }
    
    // Need to swap out if RAM is full
    uint64_t ram_available = memory_get_available(engine->memory, MEMORY_TIER_RAM);
    if (ram_available < layer->size_bytes) {
        // Find victim layer to swap out
        uint32_t victim = find_least_important_layer(engine, MEMORY_TIER_RAM);
        if (victim < engine->num_layers) {
            swap_layer_out(engine, victim, MEMORY_TIER_DISK);
        }
    }
    
    // Load layer from disk
    // In practice, would use file I/O
    layer->current_tier = MEMORY_TIER_RAM;
    layer->is_loaded = true;
    layer->load_time_ns = get_time_ns() - start_time;
    
    engine->total_layer_loads++;
    engine->total_load_time_ns += layer->load_time_ns;
    
    return layer->load_time_ns;
}

uint64_t swap_layer_out(ProgressiveEngine* engine, uint32_t layer_idx, MemoryTier target_tier) {
    if (!engine || layer_idx >= engine->num_layers) return 0;
    
    uint64_t start_time = get_time_ns();
    
    LayerState* layer = &engine->layers[layer_idx];
    
    // Can only swap to lower tier
    if (layer->current_tier >= target_tier) {
        return 0;
    }
    
    // Swap out
    // In practice, would use CUDA memcpy or file I/O
    layer->current_tier = target_tier;
    layer->is_loaded = (target_tier != MEMORY_TIER_DISK);
    
    engine->total_layer_swaps++;
    engine->total_swap_time_ns += get_time_ns() - start_time;
    
    return get_time_ns() - start_time;
}

uint64_t swap_layer_in(ProgressiveEngine* engine, uint32_t layer_idx, MemoryTier target_tier) {
    if (!engine || layer_idx >= engine->num_layers) return 0;
    
    LayerState* layer = &engine->layers[layer_idx];
    
    // Can only swap to higher tier
    if (layer->current_tier <= target_tier) {
        return 0;
    }
    
    // Swap in
    if (target_tier == MEMORY_TIER_VRAM) {
        return load_layer_to_vram(engine, layer_idx);
    } else if (target_tier == MEMORY_TIER_RAM) {
        return load_layer_to_ram(engine, layer_idx);
    }
    
    return 0;
}

// ============================================================================
// STATISTICS
// ============================================================================

void get_progressive_stats(ProgressiveEngine* engine, ProgressiveStats* stats) {
    if (!engine || !stats) return;
    
    memset(stats, 0, sizeof(ProgressiveStats));
    
    stats->total_inferences = engine->total_inferences;
    stats->total_layer_loads = engine->total_layer_loads;
    stats->total_layer_swaps = engine->total_layer_swaps;
    stats->total_prefetch_hits = engine->total_prefetch_hits;
    stats->total_prefetch_misses = engine->total_prefetch_misses;
    stats->total_cache_hits = engine->total_cache_hits;
    stats->total_cache_misses = engine->total_cache_misses;
    
    if (engine->total_inferences > 0) {
        stats->avg_inference_time_ns = (float)engine->total_inference_time_ns / engine->total_inferences;
    }
    
    if (engine->total_layer_loads > 0) {
        stats->avg_load_time_ns = (float)engine->total_load_time_ns / engine->total_layer_loads;
    }
    
    if (engine->total_layer_swaps > 0) {
        stats->avg_swap_time_ns = (float)engine->total_swap_time_ns / engine->total_layer_swaps;
    }
    
    uint64_t total_prefetch = engine->total_prefetch_hits + engine->total_prefetch_misses;
    if (total_prefetch > 0) {
        stats->prefetch_hit_rate = (float)engine->total_prefetch_hits / total_prefetch;
    }
    
    uint64_t total_cache = engine->total_cache_hits + engine->total_cache_misses;
    if (total_cache > 0) {
        stats->cache_hit_rate = (float)engine->total_cache_hits / total_cache;
    }
    
    // Count layers in each tier
    for (uint32_t i = 0; i < engine->num_layers; i++) {
        switch (engine->layers[i].current_tier) {
            case MEMORY_TIER_VRAM: stats->layers_in_vram++; break;
            case MEMORY_TIER_RAM: stats->layers_in_ram++; break;
            case MEMORY_TIER_DISK: stats->layers_on_disk++; break;
            default: break;
        }
    }
    
    if (engine->memory) {
        stats->vram_used = engine->memory->vram_used;
        stats->ram_used = engine->memory->ram_used;
        stats->disk_used = engine->memory->disk_used;
    }
}

void reset_progressive_stats(ProgressiveEngine* engine) {
    if (!engine) return;
    
    engine->total_inferences = 0;
    engine->total_layer_loads = 0;
    engine->total_layer_swaps = 0;
    engine->total_prefetch_hits = 0;
    engine->total_prefetch_misses = 0;
    engine->total_cache_hits = 0;
    engine->total_cache_misses = 0;
    engine->total_inference_time_ns = 0;
    engine->total_load_time_ns = 0;
    engine->total_swap_time_ns = 0;
}

void print_progressive_stats(ProgressiveEngine* engine) {
    if (!engine) return;
    
    ProgressiveStats stats;
    get_progressive_stats(engine, &stats);
    
    printf("\n=== Progressive Engine Statistics ===\n");
    printf("Total inferences: %lu\n", (unsigned long)stats.total_inferences);
    printf("Total layer loads: %lu\n", (unsigned long)stats.total_layer_loads);
    printf("Total layer swaps: %lu\n", (unsigned long)stats.total_layer_swaps);
    printf("Prefetch hits: %lu\n", (unsigned long)stats.total_prefetch_hits);
    printf("Prefetch misses: %lu\n", (unsigned long)stats.total_prefetch_misses);
    printf("Cache hits: %lu\n", (unsigned long)stats.total_cache_hits);
    printf("Cache misses: %lu\n", (unsigned long)stats.total_cache_misses);
    printf("\n");
    printf("Avg inference time: %.2f ns\n", stats.avg_inference_time_ns);
    printf("Avg load time: %.2f ns\n", stats.avg_load_time_ns);
    printf("Avg swap time: %.2f ns\n", stats.avg_swap_time_ns);
    printf("\n");
    printf("Prefetch hit rate: %.1f%%\n", stats.prefetch_hit_rate * 100.0f);
    printf("Cache hit rate: %.1f%%\n", stats.cache_hit_rate * 100.0f);
    printf("\n");
    printf("Layers in VRAM: %u\n", stats.layers_in_vram);
    printf("Layers in RAM: %u\n", stats.layers_in_ram);
    printf("Layers on disk: %u\n", stats.layers_on_disk);
    printf("\n");
    printf("VRAM used: %lu MB\n", (unsigned long)(stats.vram_used / (1024 * 1024)));
    printf("RAM used: %lu MB\n", (unsigned long)(stats.ram_used / (1024 * 1024)));
    printf("Disk used: %lu MB\n", (unsigned long)(stats.disk_used / (1024 * 1024)));
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void set_prefetch_depth(ProgressiveEngine* engine, uint32_t depth) {
    if (!engine) return;
    engine->prefetch_depth = depth;
}

void set_max_layers(ProgressiveEngine* engine, uint32_t vram_layers, uint32_t ram_layers) {
    if (!engine) return;
    engine->max_layers_in_vram = vram_layers;
    engine->max_layers_in_ram = ram_layers;
}

void enable_prefetch(ProgressiveEngine* engine, bool enable) {
    if (!engine) return;
    engine->prefetch_enabled = enable;
}

void enable_hotpatch(ProgressiveEngine* engine, bool enable) {
    if (!engine) return;
    engine->hotpatch_enabled = enable;
}

void enable_auto_tier(ProgressiveEngine* engine, bool enable) {
    if (!engine) return;
    engine->auto_tier_enabled = enable;
}

// ============================================================================
// UTILITY
// ============================================================================

float calculate_layer_importance(ProgressiveEngine* engine, uint32_t layer_idx) {
    if (!engine || layer_idx >= engine->num_layers) return 0.0f;
    
    LayerState* layer = &engine->layers[layer_idx];
    
    // Factors:
    // 1. Position (early/late layers more important)
    float position_weight = (layer_idx < 8) ? 1.5f :
                           (layer_idx >= engine->num_layers - 8) ? 1.3f : 1.0f;
    
    // 2. Access frequency
    float access_weight = (layer->access_count > 0) ? 
                          1.0f + (layer->access_count / 100.0f) : 1.0f;
    
    // 3. Recency
    uint64_t now = get_time_ns();
    float recency_weight = 1.0f;
    if (layer->last_access_time > 0) {
        uint64_t time_since_access = now - layer->last_access_time;
        recency_weight = 1.0f / (1.0f + time_since_access / 1e9f);
    }
    
    return position_weight * access_weight * recency_weight;
}

uint64_t estimate_layer_size(ProgressiveEngine* engine, uint32_t layer_idx, QuantType quant) {
    if (!engine || layer_idx >= engine->num_layers) return 0;
    
    // Base size (FP16)
    uint64_t base_size = 10ULL * 1024 * 1024 * 1024;  // 10GB
    
    // Adjust for quantization
    float quant_factor = 1.0f;
    switch (quant) {
        case QINT2: quant_factor = 0.125f; break;
        case QINT3: quant_factor = 0.1875f; break;
        case QINT4: quant_factor = 0.25f; break;
        case QINT8: quant_factor = 0.5f; break;
        case QFP8: quant_factor = 0.5f; break;
        case QFP16: quant_factor = 1.0f; break;
        case QFP32: quant_factor = 2.0f; break;
        default: break;
    }
    
    return (uint64_t)(base_size * quant_factor);
}

uint32_t find_least_important_layer(ProgressiveEngine* engine, MemoryTier tier) {
    if (!engine) return 0;
    
    uint32_t least_important = 0;
    float min_importance = 1e10f;
    
    for (uint32_t i = 0; i < engine->num_layers; i++) {
        if (engine->layers[i].current_tier == tier) {
            float importance = calculate_layer_importance(engine, i);
            if (importance < min_importance) {
                min_importance = importance;
                least_important = i;
            }
        }
    }
    
    return least_important;
}

uint32_t find_most_important_layer(ProgressiveEngine* engine, MemoryTier tier) {
    if (!engine) return 0;
    
    uint32_t most_important = 0;
    float max_importance = 0.0f;
    
    for (uint32_t i = 0; i < engine->num_layers; i++) {
        if (engine->layers[i].current_tier == tier) {
            float importance = calculate_layer_importance(engine, i);
            if (importance > max_importance) {
                max_importance = importance;
                most_important = i;
            }
        }
    }
    
    return most_important;
}

// ============================================================================
// DEMO / TEST
// ============================================================================

#ifdef PROGRESSIVE_ENGINE_DEMO

int main(void) {
    printf("=== Progressive Engine Demo ===\n\n");
    
    // Create auto-config
    AutoConfig config;
    memset(&config, 0, sizeof(config));
    config.hw = detect_hardware();
    
    // Create progressive engine
    ProgressiveEngine* engine = progressive_engine_create(&config);
    
    // Configure
    set_prefetch_depth(engine, 3);
    set_max_layers(engine, 40, 30);
    enable_prefetch(engine, true);
    enable_auto_tier(engine, true);
    
    // Run inference
    printf("Running inference...\n");
    for (int i = 0; i < 100; i++) {
        progressive_inference_with_prefetch(engine, i);
    }
    
    // Print stats
    print_progressive_stats(engine);
    
    // Cleanup
    progressive_engine_destroy(engine);
    
    printf("\n=== Demo Complete ===\n");
    
    return 0;
}

#endif // PROGRESSIVE_ENGINE_DEMO