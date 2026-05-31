// ============================================================================
// MEMORY_MANAGER - Implementation
// ============================================================================

#define MEMORY_MANAGER_IMPLEMENTATION
#include "memory_manager.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// ============================================================================
// Memory Creation
// ============================================================================
UnifiedMemory* umemory_create(uint64_t vram_size, uint64_t ram_size) {
    UnifiedMemory* mem = calloc(1, sizeof(UnifiedMemory));
    if (!mem) return NULL;
    
    mem->vram_capacity = vram_size;
    mem->ram_capacity = ram_size;
    mem->disk_capacity = 0;
    
    mem->vram_pools = calloc(16, sizeof(MemoryPool));
    mem->ram_pools = calloc(64, sizeof(MemoryPool));
    mem->disk_pools = calloc(256, sizeof(MemoryPool));
    
    mem->max_layers_in_vram = 3;
    mem->prefetch_layers = 2;
    mem->offload_threshold = 90;
    
    return mem;
}

void umemory_destroy(UnifiedMemory* mem) {
    if (!mem) return;
    
    for (uint32_t i = 0; i < mem->num_vram_pools; i++) {
        if (mem->vram_pools[i].data) free(mem->vram_pools[i].data);
    }
    for (uint32_t i = 0; i < mem->num_ram_pools; i++) {
        if (mem->ram_pools[i].data) free(mem->ram_pools[i].data);
    }
    
    free(mem->vram_pools);
    free(mem->ram_pools);
    free(mem->disk_pools);
    free(mem->layer_status);
    free(mem);
}

// ============================================================================
// Memory Allocation
// ============================================================================
void* umemory_alloc(UnifiedMemory* mem, size_t size, MemoryTier tier, const char* tag) {
    MemoryPool* pool = NULL;
    
    if (tier == MEM_VRAM) {
        for (uint32_t i = 0; i < mem->num_vram_pools; i++) {
            if (mem->vram_pools[i].used + size <= mem->vram_pools[i].size) {
                pool = &mem->vram_pools[i];
                break;
            }
        }
        
        if (!pool && mem->num_vram_pools < 16) {
            pool = &mem->vram_pools[mem->num_vram_pools++];
            pool->size = size * 2;
            pool->data = malloc(pool->size);
            pool->tier = MEM_VRAM;
        }
    }
    else if (tier == MEM_RAM) {
        for (uint32_t i = 0; i < mem->num_ram_pools; i++) {
            if (mem->ram_pools[i].used + size <= mem->ram_pools[i].size) {
                pool = &mem->ram_pools[i];
                break;
            }
        }
        
        if (!pool && mem->num_ram_pools < 64) {
            pool = &mem->ram_pools[mem->num_ram_pools++];
            pool->size = size * 2;
            pool->data = malloc(pool->size);
            pool->tier = MEM_RAM;
        }
    }
    
    if (!pool) return NULL;
    
    void* ptr = (char*)pool->data + pool->used;
    pool->used += size;
    
    if (tag) strncpy(pool->tag, tag, sizeof(pool->tag) - 1);
    
    if (tier == MEM_VRAM) mem->vram_used += size;
    else mem->ram_used += size;
    
    return ptr;
}

void umemory_free(UnifiedMemory* mem, void* ptr) {
    for (uint32_t i = 0; i < mem->num_vram_pools; i++) {
        if (mem->vram_pools[i].data <= ptr && 
            ptr < (char*)mem->vram_pools[i].data + mem->vram_pools[i].size) {
            mem->vram_used -= mem->vram_pools[i].used;
            mem->vram_pools[i].used = 0;
            return;
        }
    }
    
    for (uint32_t i = 0; i < mem->num_ram_pools; i++) {
        if (mem->ram_pools[i].data <= ptr &&
            ptr < (char*)mem->ram_pools[i].data + mem->ram_pools[i].size) {
            mem->ram_used -= mem->ram_pools[i].used;
            mem->ram_pools[i].used = 0;
            return;
        }
    }
}

// ============================================================================
// Layer Management
// ============================================================================
int umemory_load_layer(UnifiedMemory* mem, MMapModel* model, uint32_t layer_idx) {
    if (layer_idx >= model->num_layers) return -1;
    if (model->layer_loaded[layer_idx]) {
        mem->cache_hits++;
        return 0;
    }
    
    mem->cache_misses++;
    
    if (mem->vram_used > mem->vram_capacity * mem->offload_threshold / 100) {
        umemory_make_space(mem, model->layer_sizes[layer_idx], MEM_VRAM);
    }
    
    uint64_t offset = model->layer_offsets[layer_idx];
    uint32_t size = model->layer_sizes[layer_idx];
    
    void* layer_data = umemory_alloc(mem, size, MEM_VRAM, "layer");
    if (!layer_data) {
        layer_data = umemory_alloc(mem, size, MEM_RAM, "layer");
        if (!layer_data) return -1;
    }
    
    memcpy(layer_data, (char*)model->base_ptr + offset, size);
    
    model->layer_ptrs[layer_idx] = layer_data;
    model->layer_loaded[layer_idx] = 1;
    
    return 0;
}

int umemory_unload_layer(UnifiedMemory* mem, MMapModel* model, uint32_t layer_idx) {
    if (!model->layer_loaded[layer_idx]) return 0;
    
    umemory_free(mem, model->layer_ptrs[layer_idx]);
    model->layer_ptrs[layer_idx] = NULL;
    model->layer_loaded[layer_idx] = 0;
    
    return 0;
}

int umemory_offload_layer(UnifiedMemory* mem, MMapModel* model, uint32_t layer_idx, MemoryTier target_tier) {
    if (!model->layer_loaded[layer_idx]) return -1;
    
    if (target_tier == MEM_DISK) {
        umemory_unload_layer(mem, model, layer_idx);
        return 0;
    }
    
    void* ram_ptr = umemory_alloc(mem, model->layer_sizes[layer_idx], MEM_RAM, "offloaded");
    if (!ram_ptr) return -1;
    
    memcpy(ram_ptr, model->layer_ptrs[layer_idx], model->layer_sizes[layer_idx]);
    umemory_free(mem, model->layer_ptrs[layer_idx]);
    model->layer_ptrs[layer_idx] = ram_ptr;
    
    mem->total_vram_swaps++;
    return 0;
}

// ============================================================================
// Space Management
// ============================================================================
void umemory_make_space(UnifiedMemory* mem, size_t required, MemoryTier tier) {
    while (mem->vram_used + required > mem->vram_capacity) {
        uint32_t evict = umemory_evict_lru(mem, tier);
        if (evict == UINT32_MAX) break;
    }
}

uint32_t umemory_evict_lru(UnifiedMemory* mem, MemoryTier tier) {
    uint64_t oldest_time = UINT64_MAX;
    uint32_t oldest_idx = UINT32_MAX;
    
    for (uint32_t i = 0; i < mem->num_layers; i++) {
        if (mem->layer_status[i].current_tier == tier) {
            if (mem->layer_status[i].last_access < oldest_time) {
                oldest_time = mem->layer_status[i].last_access;
                oldest_idx = i;
            }
        }
    }
    
    if (oldest_idx != UINT32_MAX) {
        mem->total_vram_swaps++;
    }
    
    return oldest_idx;
}

void umemory_prefetch_next(UnifiedMemory* mem, MMapModel* model, uint32_t current_layer) {
    for (uint32_t i = 1; i <= mem->prefetch_layers; i++) {
        uint32_t next_layer = current_layer + i;
        if (next_layer < model->num_layers && !model->layer_loaded[next_layer]) {
            umemory_load_layer(mem, model, next_layer);
        }
    }
}

void umemory_update_importance(UnifiedMemory* mem, uint32_t layer_idx, float score) {
    if (layer_idx < mem->num_layers) {
        mem->layer_status[layer_idx].importance_score = score;
    }
}

// ============================================================================
// Statistics
// ============================================================================
void umemory_print_stats(UnifiedMemory* mem) {
    printf("=== Memory Statistics ===\n");
    printf("VRAM: %lu/%lu MB (%.1f%% used)\n",
           (unsigned long)(mem->vram_used / (1024 * 1024)),
           (unsigned long)(mem->vram_capacity / (1024 * 1024)),
           100.0 * mem->vram_used / mem->vram_capacity);
    printf("RAM: %lu/%lu MB (%.1f%% used)\n",
           (unsigned long)(mem->ram_used / (1024 * 1024)),
           (unsigned long)(mem->ram_capacity / (1024 * 1024)),
           100.0 * mem->ram_used / mem->ram_capacity);
    printf("Cache hits: %lu\n", (unsigned long)mem->cache_hits);
    printf("Cache misses: %lu\n", (unsigned long)mem->cache_misses);
    printf("Hit rate: %.1f%%\n", 100.0 * mem->cache_hits / (mem->cache_hits + mem->cache_misses + 1));
    printf("VRAM swaps: %lu\n", (unsigned long)mem->total_vram_swaps);
}

float umemory_get_hit_rate(UnifiedMemory* mem) {
    uint64_t total = mem->cache_hits + mem->cache_misses;
    return total > 0 ? (float)mem->cache_hits / total : 0.0f;
}

// ============================================================================
// Memory-Mapped Model
// ============================================================================
MMapModel* mmap_model_load(const char* path) {
    MMapModel* model = calloc(1, sizeof(MMapModel));
    if (!model) return NULL;
    
#ifdef _WIN32
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        free(model);
        return NULL;
    }
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    model->file_size = fileSize.QuadPart;
    
    HANDLE hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) {
        CloseHandle(hFile);
        free(model);
        return NULL;
    }
    
    model->base_ptr = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(hMap);
    CloseHandle(hFile);
    
    if (!model->base_ptr) {
        free(model);
        return NULL;
    }
#else
    model->fd = open(path, O_RDONLY);
    if (model->fd < 0) {
        free(model);
        return NULL;
    }
    
    struct stat st;
    fstat(model->fd, &st);
    model->file_size = st.st_size;
    
    model->base_ptr = mmap(NULL, model->file_size, PROT_READ, MAP_PRIVATE, model->fd, 0);
    if (model->base_ptr == MAP_FAILED) {
        close(model->fd);
        free(model);
        return NULL;
    }
#endif
    
    // Parse header (simplified)
    model->num_layers = 80; // Would read from file
    model->layer_offsets = calloc(model->num_layers, sizeof(uint64_t));
    model->layer_sizes = calloc(model->num_layers, sizeof(uint32_t));
    model->layer_loaded = calloc(model->num_layers, sizeof(uint8_t));
    model->layer_ptrs = calloc(model->num_layers, sizeof(void*));
    
    return model;
}

void mmap_model_unload(MMapModel* model) {
    if (!model) return;
    
#ifdef _WIN32
    if (model->base_ptr) UnmapViewOfFile(model->base_ptr);
#else
    if (model->base_ptr) munmap(model->base_ptr, model->file_size);
    if (model->fd >= 0) close(model->fd);
#endif
    
    free(model->layer_offsets);
    free(model->layer_sizes);
    free(model->layer_loaded);
    free(model->layer_ptrs);
    free(model);
}

// ============================================================================
// Strategy for 16GB
// ============================================================================
MemoryStrategy16GB get_recommended_strategy_16gb(void) {
    MemoryStrategy16GB strategy;
    
    // GLM-5: 80 layers, ~1.25GB per layer in INT4
    strategy.layers_in_vram = 3;       // ~3.75GB for active layers
    strategy.layers_in_ram = 12;       // ~15GB in system RAM
    strategy.layers_on_disk = 65;       // Rest on SSD
    
    strategy.embed_in_vram = 1;
    strategy.embed_cache_size = 100 * 1024 * 1024; // 100MB
    
    strategy.kv_cache_layers = 32;
    strategy.kv_cache_size = 2 * 1024 * 1024; // 2MB per layer
    
    strategy.recompute_layers = 35;
    
    return strategy;
}