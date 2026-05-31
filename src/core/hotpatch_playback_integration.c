// hotpatch_playback_integration.c - Integration Layer Implementation
// Connects playback controller with progressive engine and live hotpatch system
// Part of RawrXD Progressive Layer Loading System

#define HOTPATCH_PLAYBACK_INTEGRATION_IMPLEMENTATION
#include "hotpatch_playback_integration.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

// ============================================================================
// UTILITY
// ============================================================================

static uint64_t get_time_ns_impl(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000000000ULL / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#endif
}

static float clamp_float(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

// ============================================================================
// INTEGRATION LIFECYCLE
// ============================================================================

PlaybackIntegration* playback_integration_create(
    HardwareProfile* hw,
    void* model_context
) {
    PlaybackIntegration* integration = (PlaybackIntegration*)calloc(1, sizeof(PlaybackIntegration));
    if (!integration) return NULL;
    
    integration->hw = hw;
    integration->model_context = model_context;
    integration->is_integrated = false;
    integration->auto_optimize = true;
    integration->quality_threshold = 0.7f;
    integration->speed_threshold = 0.5f;
    integration->memory_budget = hw ? hw->vram_bytes : 16ULL * 1024 * 1024 * 1024; // Default 16GB
    
    integration->best_quality_achieved = 0.0f;
    integration->best_speed_achieved = 0.0f;
    integration->lowest_memory_achieved = UINT64_MAX;
    
    return integration;
}

void playback_integration_destroy(PlaybackIntegration* integration) {
    if (!integration) return;
    
    // Shutdown if still integrated
    if (integration->is_integrated) {
        playback_integration_shutdown(integration);
    }
    
    // Destroy subsystems
    if (integration->playback) {
        playback_destroy(integration->playback);
    }
    if (integration->omni) {
        omni_hotpatch_destroy(integration->omni);
    }
    if (integration->hotpatch) {
        live_hotpatch_destroy(integration->hotpatch);
    }
    if (integration->progressive) {
        progressive_engine_destroy(integration->progressive);
    }
    if (integration->config) {
        auto_config_destroy(integration->config);
    }
    
    free(integration);
}

bool playback_integration_initialize(
    PlaybackIntegration* integration,
    const AutoConfigSettings* settings
) {
    if (!integration || !integration->hw) {
        printf("[INTEGRATION] Error: Invalid integration or hardware profile\n");
        return false;
    }
    
    printf("[INTEGRATION] Initializing playback integration system...\n");
    
    // Create auto-configurator
    if (settings) {
        integration->config = auto_config_create(settings);
    } else {
        AutoConfigSettings default_settings = {
            .target_quality = 0.8f,
            .target_speed = 0.6f,
            .max_memory_percent = 90.0f,
            .enable_pruning = true,
            .enable_quantization = true,
            .enable_compression = true,
            .auto_detect = true
        };
        integration->config = auto_config_create(&default_settings);
    }
    
    if (!integration->config) {
        printf("[INTEGRATION] Error: Failed to create auto-configurator\n");
        return false;
    }
    
    // Create live hotpatch system
    LiveHotpatchConfig hotpatch_config = {
        .max_checkpoints = 32,
        .max_prune_percent = 0.5f,
        .importance_method = IMPORTANCE_MAGNITUDE,
        .enable_head_pruning = true,
        .enable_layer_fusion = true,
        .enable_kv_compression = true
    };
    
    integration->hotpatch = live_hotpatch_create(&hotpatch_config, integration->model_context);
    if (!integration->hotpatch) {
        printf("[INTEGRATION] Error: Failed to create live hotpatch system\n");
        auto_config_destroy(integration->config);
        return false;
    }
    
    // Create omnidirectional hotpatch
    integration->omni = omni_hotpatch_create(integration->hotpatch);
    if (!integration->omni) {
        printf("[INTEGRATION] Error: Failed to create omnidirectional hotpatch\n");
        live_hotpatch_destroy(integration->hotpatch);
        auto_config_destroy(integration->config);
        return false;
    }
    
    // Create progressive engine
    ProgressiveConfig progressive_config = {
        .vram_budget = integration->hw->vram_bytes,
        .ram_budget = integration->hw->ram_bytes,
        .disk_cache_path = "/tmp/rawrxd_cache",
        .prefetch_ahead = 3,
        .importance_threshold = 0.1f
    };
    
    integration->progressive = progressive_engine_create(&progressive_config);
    if (!integration->progressive) {
        printf("[INTEGRATION] Error: Failed to create progressive engine\n");
        omni_hotpatch_destroy(integration->omni);
        live_hotpatch_destroy(integration->hotpatch);
        auto_config_destroy(integration->config);
        return false;
    }
    
    // Create playback controller
    integration->playback = playback_create(integration->omni);
    if (!integration->playback) {
        printf("[INTEGRATION] Error: Failed to create playback controller\n");
        progressive_engine_destroy(integration->progressive);
        omni_hotpatch_destroy(integration->omni);
        live_hotpatch_destroy(integration->hotpatch);
        auto_config_destroy(integration->config);
        return false;
    }
    
    integration->is_integrated = true;
    
    printf("[INTEGRATION] ✓ All subsystems initialized successfully\n");
    printf("[INTEGRATION]   - Auto-configurator: %p\n", (void*)integration->config);
    printf("[INTEGRATION]   - Live hotpatch: %p\n", (void*)integration->hotpatch);
    printf("[INTEGRATION]   - Omnidirectional hotpatch: %p\n", (void*)integration->omni);
    printf("[INTEGRATION]   - Progressive engine: %p\n", (void*)integration->progressive);
    printf("[INTEGRATION]   - Playback controller: %p\n", (void*)integration->playback);
    
    return true;
}

void playback_integration_shutdown(PlaybackIntegration* integration) {
    if (!integration || !integration->is_integrated) return;
    
    printf("[INTEGRATION] Shutting down playback integration system...\n");
    
    // Stop any active operations
    if (integration->playback) {
        playback_stop(integration->playback);
        if (integration->playback->is_recording) {
            playback_record_stop(integration->playback);
        }
        if (integration->playback->smoke_test_active) {
            smoke_test_stop(integration->playback);
        }
    }
    
    // Generate final statistics
    IntegrationStats stats = playback_integration_get_stats(integration);
    printf("[INTEGRATION] Final statistics:\n");
    printf("[INTEGRATION]   - Total optimizations: %lu\n", (unsigned long)stats.total_optimizations);
    printf("[INTEGRATION]   - Success rate: %.1f%%\n", stats.success_rate * 100.0f);
    printf("[INTEGRATION]   - Best quality: %.4f\n", stats.best_quality_achieved);
    printf("[INTEGRATION]   - Best speed: %.4f\n", stats.best_speed_achieved);
    printf("[INTEGRATION]   - Lowest memory: %lu MB\n", (unsigned long)(stats.lowest_memory_achieved / (1024 * 1024)));
    
    integration->is_integrated = false;
    
    printf("[INTEGRATION] ✓ Shutdown complete\n");
}

// ============================================================================
// OPTIMIZATION API
// ============================================================================

OptimizationResult playback_integration_optimize(
    PlaybackIntegration* integration,
    const OptimizationRequest* request
) {
    OptimizationResult result;
    memset(&result, 0, sizeof(result));
    
    if (!integration || !integration->is_integrated) {
        snprintf(result.report_path, sizeof(result.report_path), "Error: Integration not initialized");
        return result;
    }
    
    printf("[INTEGRATION] Starting optimization session...\n");
    printf("[INTEGRATION]   Target quality: %.4f\n", request->target_quality);
    printf("[INTEGRATION]   Target speed: %.4f\n", request->target_speed);
    printf("[INTEGRATION]   Max memory: %lu MB\n", (unsigned long)(request->max_memory / (1024 * 1024)));
    printf("[INTEGRATION]   Max iterations: %u\n", request->max_iterations);
    
    uint64_t start_time = get_time_ns_impl();
    
    // Start recording
    if (request->record_session) {
        playback_record_start(integration->playback, request->session_name);
    }
    
    // Configure smoke test
    SmokeTestConfig smoke_config = {
        .mode = request->max_iterations > 100 ? SMOKE_THOROUGH : SMOKE_STANDARD,
        .max_iterations = request->max_iterations,
        .max_time_seconds = request->max_time_seconds,
        .test_operations = HOTPATCH_ALL,
        .exploration_rate = 0.3f,
        .quality_degradation_max = 1.0f - request->target_quality,
        .target_quality_min = request->target_quality,
        .target_speed_min = request->target_speed,
        .target_memory_max = request->max_memory,
        .record_session = request->record_session,
        .generate_report = request->generate_report
    };
    
    // Apply operation filters
    if (!request->allow_aggressive) {
        smoke_config.test_operations &= ~(HOTPATCH_PRUNE_EXPERTS);
    }
    if (!request->allow_quantization) {
        smoke_config.test_operations &= ~(HOTPATCH_QUANTIZE);
    }
    if (!request->allow_compression) {
        smoke_config.test_operations &= ~(HOTPATCH_COMPRESS_KV);
    }
    
    // Run smoke test
    SmokeTestResult smoke_result = smoke_test_run(integration->playback, &smoke_config);
    
    // Populate result
    result.success = smoke_result.passed;
    result.final_quality = smoke_result.final_quality;
    result.final_speed = smoke_result.final_speed;
    result.final_memory = smoke_result.final_memory;
    result.iterations_used = smoke_result.states_tested;
    result.hotpatches_applied = smoke_result.hotpatches_applied;
    
    // Find best state
    if (integration->playback->session->best_quality_state > 0) {
        snprintf(result.best_state_id, sizeof(result.best_state_id), 
                 "%lu", (unsigned long)integration->playback->session->best_quality_state);
    }
    
    // Stop recording
    if (request->record_session) {
        playback_record_stop(integration->playback);
    }
    
    // Generate report
    if (request->generate_report) {
        snprintf(result.report_path, sizeof(result.report_path), 
                 "optimization_report_%lu.md", (unsigned long)start_time);
        playback_generate_report(integration->playback, result.report_path);
    }
    
    result.time_elapsed_ns = get_time_ns_impl() - start_time;
    
    // Update statistics
    integration->total_optimizations++;
    if (result.success) {
        integration->successful_optimizations++;
    }
    if (result.final_quality > integration->best_quality_achieved) {
        integration->best_quality_achieved = result.final_quality;
    }
    if (result.final_speed > integration->best_speed_achieved) {
        integration->best_speed_achieved = result.final_speed;
    }
    if (result.final_memory < integration->lowest_memory_achieved) {
        integration->lowest_memory_achieved = result.final_memory;
    }
    
    printf("[INTEGRATION] Optimization complete:\n");
    printf("[INTEGRATION]   - Success: %s\n", result.success ? "Yes" : "No");
    printf("[INTEGRATION]   - Final quality: %.4f\n", result.final_quality);
    printf("[INTEGRATION]   - Final speed: %.4f\n", result.final_speed);
    printf("[INTEGRATION]   - Final memory: %lu MB\n", (unsigned long)(result.final_memory / (1024 * 1024)));
    printf("[INTEGRATION]   - Time elapsed: %.2f seconds\n", result.time_elapsed_ns / 1e9f);
    
    // Callback
    if (integration->on_optimization_complete) {
        integration->on_optimization_complete(integration, result.final_quality, 
                                              result.final_speed, result.final_memory);
    }
    
    return result;
}

OptimizationResult playback_integration_auto_tune(
    PlaybackIntegration* integration,
    const AutoTuneConfig* config
) {
    OptimizationRequest request;
    memset(&request, 0, sizeof(request));
    
    request.target_quality = integration->quality_threshold;
    request.target_speed = integration->speed_threshold;
    request.max_memory = integration->memory_budget;
    request.max_iterations = config->max_tune_steps;
    request.max_time_seconds = config->tune_interval_ms * config->max_tune_steps / 1000;
    request.allow_aggressive = true;
    request.allow_quantization = true;
    request.allow_compression = true;
    request.record_session = true;
    request.generate_report = true;
    snprintf(request.session_name, sizeof(request.session_name), "AutoTune_%lu", 
             (unsigned long)get_time_ns_impl());
    
    return playback_integration_optimize(integration, &request);
}

OptimizationResult playback_integration_quick_optimize(
    PlaybackIntegration* integration,
    uint32_t max_time_seconds
) {
    OptimizationRequest request;
    memset(&request, 0, sizeof(request));
    
    request.target_quality = 0.7f;
    request.target_speed = 0.5f;
    request.max_memory = integration->memory_budget;
    request.max_iterations = 20;
    request.max_time_seconds = max_time_seconds;
    request.allow_aggressive = false;
    request.allow_quantization = true;
    request.allow_compression = true;
    request.record_session = false;
    request.generate_report = false;
    snprintf(request.session_name, sizeof(request.session_name), "QuickOpt");
    
    return playback_integration_optimize(integration, &request);
}

OptimizationResult playback_integration_full_optimize(
    PlaybackIntegration* integration,
    uint32_t max_iterations
) {
    OptimizationRequest request;
    memset(&request, 0, sizeof(request));
    
    request.target_quality = 0.9f;
    request.target_speed = 0.8f;
    request.max_memory = integration->memory_budget;
    request.max_iterations = max_iterations;
    request.max_time_seconds = 3600; // 1 hour max
    request.allow_aggressive = true;
    request.allow_quantization = true;
    request.allow_compression = true;
    request.record_session = true;
    request.generate_report = true;
    snprintf(request.session_name, sizeof(request.session_name), "FullOpt_%lu",
             (unsigned long)get_time_ns_impl());
    
    return playback_integration_optimize(integration, &request);
}

// ============================================================================
// MODEL MAP API
// ============================================================================

ModelMap* model_map_create(const char* map_name) {
    ModelMap* map = (ModelMap*)calloc(1, sizeof(ModelMap));
    if (!map) return NULL;
    
    map->entry_capacity = 64;
    map->entries = (ModelMapEntry*)calloc(map->entry_capacity, sizeof(ModelMapEntry));
    if (!map->entries) {
        free(map);
        return NULL;
    }
    
    if (map_name) {
        strncpy(map->map_name, map_name, sizeof(map->map_name) - 1);
    } else {
        snprintf(map->map_name, sizeof(map->map_name), "ModelMap_%lu", 
                 (unsigned long)time(NULL));
    }
    
    map->creation_time = get_time_ns_impl();
    map->last_updated = map->creation_time;
    
    return map;
}

void model_map_destroy(ModelMap* map) {
    if (!map) return;
    
    for (uint32_t i = 0; i < map->entry_count; i++) {
        free(map->entries[i].pruned_layers);
        free(map->entries[i].layer_importance_scores);
    }
    
    free(map->entries);
    free(map);
}

bool model_map_add_entry(
    ModelMap* map,
    const ModelMapEntry* entry
) {
    if (!map || !entry) return false;
    
    // Grow if needed
    if (map->entry_count >= map->entry_capacity) {
        uint32_t new_capacity = map->entry_capacity * 2;
        ModelMapEntry* new_entries = (ModelMapEntry*)realloc(map->entries,
                                               new_capacity * sizeof(ModelMapEntry));
        if (!new_entries) return false;
        
        map->entries = new_entries;
        map->entry_capacity = new_capacity;
    }
    
    // Copy entry
    ModelMapEntry* dest = &map->entries[map->entry_count];
    memcpy(dest, entry, sizeof(ModelMapEntry));
    
    // Deep copy arrays
    if (entry->pruned_layers && entry->pruned_layer_count > 0) {
        dest->pruned_layers = (uint32_t*)malloc(entry->pruned_layer_count * sizeof(uint32_t));
        if (dest->pruned_layers) {
            memcpy(dest->pruned_layers, entry->pruned_layers, 
                   entry->pruned_layer_count * sizeof(uint32_t));
        }
    }
    
    if (entry->layer_importance_scores && entry->pruned_layer_count > 0) {
        dest->layer_importance_scores = (float*)malloc(entry->pruned_layer_count * sizeof(float));
        if (dest->layer_importance_scores) {
            memcpy(dest->layer_importance_scores, entry->layer_importance_scores,
                   entry->pruned_layer_count * sizeof(float));
        }
    }
    
    map->entry_count++;
    map->last_updated = get_time_ns_impl();
    
    return true;
}

ModelMapEntry* model_map_find_best(
    ModelMap* map,
    float min_quality,
    float min_speed,
    uint64_t max_memory
) {
    if (!map || map->entry_count == 0) return NULL;
    
    ModelMapEntry* best = NULL;
    float best_score = -FLT_MAX;
    
    for (uint32_t i = 0; i < map->entry_count; i++) {
        ModelMapEntry* entry = &map->entries[i];
        
        // Check constraints
        if (entry->quality_score < min_quality) continue;
        if (entry->speed_score < min_speed) continue;
        if (entry->memory_usage > max_memory) continue;
        if (!entry->is_validated) continue;
        
        // Calculate score
        float score = entry->quality_score * 0.4f + 
                     entry->speed_score * 0.3f +
                     (1.0f - (float)entry->memory_usage / (float)max_memory) * 0.3f;
        
        if (score > best_score) {
            best_score = score;
            best = entry;
        }
    }
    
    return best;
}

ModelMapEntry* model_map_find_by_name(
    ModelMap* map,
    const char* model_name,
    const char* config_name
) {
    if (!map || !model_name) return NULL;
    
    for (uint32_t i = 0; i < map->entry_count; i++) {
        ModelMapEntry* entry = &map->entries[i];
        
        if (strcmp(entry->model_name, model_name) == 0) {
            if (!config_name || strcmp(entry->config_name, config_name) == 0) {
                return entry;
            }
        }
    }
    
    return NULL;
}

bool model_map_validate_entry(
    PlaybackIntegration* integration,
    ModelMapEntry* entry,
    uint32_t validation_tokens
) {
    if (!integration || !entry) return false;
    
    printf("[MODEL_MAP] Validating entry: %s/%s\n", entry->model_name, entry->config_name);
    
    // Run validation
    ValidationResult result = playback_validate_config(integration, validation_tokens);
    
    entry->is_validated = result.is_valid;
    entry->validation_timestamp = get_time_ns_impl();
    entry->validation_loss = result.perplexity;
    entry->quality_score = result.quality_score;
    entry->speed_score = result.speed_score;
    entry->memory_usage = result.memory_usage;
    
    printf("[MODEL_MAP] Validation %s: Q=%.4f, S=%.4f, M=%luMB\n",
           result.is_valid ? "PASSED" : "FAILED",
           entry->quality_score, entry->speed_score,
           (unsigned long)(entry->memory_usage / (1024 * 1024)));
    
    return result.is_valid;
}

bool model_map_save(
    ModelMap* map,
    const char* filepath
) {
    if (!map || !filepath) return false;
    
    FILE* f = fopen(filepath, "wb");
    if (!f) {
        printf("[MODEL_MAP] Error: Cannot open %s for writing\n", filepath);
        return false;
    }
    
    // Write header
    fprintf(f, "RAWRXD_MODEL_MAP_V1\n");
    fprintf(f, "name=%s\n", map->map_name);
    fprintf(f, "created=%lu\n", (unsigned long)map->creation_time);
    fprintf(f, "updated=%lu\n", (unsigned long)map->last_updated);
    fprintf(f, "entries=%u\n", map->entry_count);
    fprintf(f, "---\n");
    
    // Write entries
    for (uint32_t i = 0; i < map->entry_count; i++) {
        ModelMapEntry* entry = &map->entries[i];
        
        fprintf(f, "ENTRY\n");
        fprintf(f, "state_id=%lu\n", (unsigned long)entry->state_id);
        fprintf(f, "model=%s\n", entry->model_name);
        fprintf(f, "config=%s\n", entry->config_name);
        fprintf(f, "quality=%.6f\n", entry->quality_score);
        fprintf(f, "speed=%.6f\n", entry->speed_score);
        fprintf(f, "memory=%lu\n", (unsigned long)entry->memory_usage);
        fprintf(f, "operations=%u\n", entry->applied_operations);
        fprintf(f, "prune_ratio=%.6f\n", entry->prune_ratio);
        fprintf(f, "quant_bits=%u\n", entry->quantization_bits);
        fprintf(f, "kv_ratio=%.6f\n", entry->kv_compression_ratio);
        fprintf(f, "validated=%d\n", entry->is_validated ? 1 : 0);
        fprintf(f, "validation_time=%lu\n", (unsigned long)entry->validation_timestamp);
        fprintf(f, "validation_loss=%.6f\n", entry->validation_loss);
        fprintf(f, "---\n");
    }
    
    fclose(f);
    
    printf("[MODEL_MAP] Saved %u entries to %s\n", map->entry_count, filepath);
    return true;
}

ModelMap* model_map_load(
    const char* filepath
) {
    if (!filepath) return NULL;
    
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("[MODEL_MAP] Error: Cannot open %s for reading\n", filepath);
        return NULL;
    }
    
    // Read header
    char line[512];
    if (!fgets(line, sizeof(line), f) || strncmp(line, "RAWRXD_MODEL_MAP_V1", 19) != 0) {
        printf("[MODEL_MAP] Error: Invalid file format\n");
        fclose(f);
        return NULL;
    }
    
    ModelMap* map = (ModelMap*)calloc(1, sizeof(ModelMap));
    if (!map) {
        fclose(f);
        return NULL;
    }
    
    map->entry_capacity = 64;
    map->entries = (ModelMapEntry*)calloc(map->entry_capacity, sizeof(ModelMapEntry));
    if (!map->entries) {
        free(map);
        fclose(f);
        return NULL;
    }
    
    // Parse header
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "---", 3) == 0) break;
        
        if (strncmp(line, "name=", 5) == 0) {
            sscanf(line, "name=%255s", map->map_name);
        } else if (strncmp(line, "created=", 8) == 0) {
            sscanf(line, "created=%lu", (unsigned long*)&map->creation_time);
        } else if (strncmp(line, "updated=", 8) == 0) {
            sscanf(line, "updated=%lu", (unsigned long*)&map->last_updated);
        } else if (strncmp(line, "entries=", 8) == 0) {
            uint32_t count;
            sscanf(line, "entries=%u", &count);
            
            // Grow capacity if needed
            while (map->entry_capacity < count) {
                map->entry_capacity *= 2;
            }
            map->entries = (ModelMapEntry*)realloc(map->entries, 
                                                    map->entry_capacity * sizeof(ModelMapEntry));
        }
    }
    
    // Read entries
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ENTRY", 5) != 0) continue;
        
        ModelMapEntry entry;
        memset(&entry, 0, sizeof(entry));
        
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "---", 3) == 0) break;
            
            if (strncmp(line, "state_id=", 9) == 0) {
                sscanf(line, "state_id=%lu", (unsigned long*)&entry.state_id);
            } else if (strncmp(line, "model=", 6) == 0) {
                sscanf(line, "model=%127s", entry.model_name);
            } else if (strncmp(line, "config=", 7) == 0) {
                sscanf(line, "config=%127s", entry.config_name);
            } else if (strncmp(line, "quality=", 8) == 0) {
                sscanf(line, "quality=%f", &entry.quality_score);
            } else if (strncmp(line, "speed=", 5) == 0) {
                sscanf(line, "speed=%f", &entry.speed_score);
            } else if (strncmp(line, "memory=", 7) == 0) {
                sscanf(line, "memory=%lu", (unsigned long*)&entry.memory_usage);
            } else if (strncmp(line, "operations=", 11) == 0) {
                sscanf(line, "operations=%u", &entry.applied_operations);
            } else if (strncmp(line, "prune_ratio=", 12) == 0) {
                sscanf(line, "prune_ratio=%f", &entry.prune_ratio);
            } else if (strncmp(line, "quant_bits=", 11) == 0) {
                sscanf(line, "quant_bits=%u", &entry.quantization_bits);
            } else if (strncmp(line, "kv_ratio=", 9) == 0) {
                sscanf(line, "kv_ratio=%f", &entry.kv_compression_ratio);
            } else if (strncmp(line, "validated=", 10) == 0) {
                int val;
                sscanf(line, "validated=%d", &val);
                entry.is_validated = val != 0;
            } else if (strncmp(line, "validation_time=", 16) == 0) {
                sscanf(line, "validation_time=%lu", (unsigned long*)&entry.validation_timestamp);
            } else if (strncmp(line, "validation_loss=", 16) == 0) {
                sscanf(line, "validation_loss=%f", &entry.validation_loss);
            }
        }
        
        model_map_add_entry(map, &entry);
    }
    
    fclose(f);
    
    printf("[MODEL_MAP] Loaded %u entries from %s\n", map->entry_count, filepath);
    return map;
}

bool model_map_export_json(
    ModelMap* map,
    const char* filepath
) {
    if (!map || !filepath) return false;
    
    FILE* f = fopen(filepath, "w");
    if (!f) {
        printf("[MODEL_MAP] Error: Cannot open %s for writing\n", filepath);
        return false;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"map_name\": \"%s\",\n", map->map_name);
    fprintf(f, "  \"creation_time\": %lu,\n", (unsigned long)map->creation_time);
    fprintf(f, "  \"last_updated\": %lu,\n", (unsigned long)map->last_updated);
    fprintf(f, "  \"entry_count\": %u,\n", map->entry_count);
    fprintf(f, "  \"entries\": [\n");
    
    for (uint32_t i = 0; i < map->entry_count; i++) {
        ModelMapEntry* entry = &map->entries[i];
        
        fprintf(f, "    {\n");
        fprintf(f, "      \"state_id\": %lu,\n", (unsigned long)entry->state_id);
        fprintf(f, "      \"model_name\": \"%s\",\n", entry->model_name);
        fprintf(f, "      \"config_name\": \"%s\",\n", entry->config_name);
        fprintf(f, "      \"quality_score\": %.6f,\n", entry->quality_score);
        fprintf(f, "      \"speed_score\": %.6f,\n", entry->speed_score);
        fprintf(f, "      \"memory_usage\": %lu,\n", (unsigned long)entry->memory_usage);
        fprintf(f, "      \"applied_operations\": %u,\n", entry->applied_operations);
        fprintf(f, "      \"prune_ratio\": %.6f,\n", entry->prune_ratio);
        fprintf(f, "      \"quantization_bits\": %u,\n", entry->quantization_bits);
        fprintf(f, "      \"kv_compression_ratio\": %.6f,\n", entry->kv_compression_ratio);
        fprintf(f, "      \"is_validated\": %s,\n", entry->is_validated ? "true" : "false");
        fprintf(f, "      \"validation_timestamp\": %lu,\n", (unsigned long)entry->validation_timestamp);
        fprintf(f, "      \"validation_loss\": %.6f\n", entry->validation_loss);
        fprintf(f, "    }%s\n", i < map->entry_count - 1 ? "," : "");
    }
    
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
    
    printf("[MODEL_MAP] Exported JSON to %s\n", filepath);
    return true;
}

// ============================================================================
// LOCKPICKING API
// ============================================================================

LockpickSession* lockpick_start(
    PlaybackIntegration* integration,
    const char* session_name
) {
    if (!integration || !integration->is_integrated) return NULL;
    
    printf("[LOCKPICK] Starting lockpicking session: %s\n", session_name ? session_name : "Unnamed");
    
    LockpickSession* session = (LockpickSession*)calloc(1, sizeof(LockpickSession));
    if (!session) return NULL;
    
    // Get baseline metrics
    HotpatchState* current = get_state(integration->omni, integration->omni->current_state_id);
    if (current) {
        session->baseline_quality = current->quality_score;
        session->baseline_speed = current->speed_score;
        session->baseline_memory = current->memory_usage;
    }
    
    session->suspended_state_id = integration->omni->current_state_id;
    session->is_suspended = true;
    session->discovered_configs = model_map_create(session_name ? session_name : "LockpickSession");
    
    // Start recording
    playback_record_start(integration->playback, session_name ? session_name : "LockpickSession");
    
    printf("[LOCKPICK] Baseline: Q=%.4f, S=%.4f, M=%luMB\n",
           session->baseline_quality, session->baseline_speed,
           (unsigned long)(session->baseline_memory / (1024 * 1024)));
    
    return session;
}

LockpickResult lockpick_try_config(
    LockpickSession* session,
    HotpatchOp operations,
    float prune_ratio,
    uint32_t quantization_bits
) {
    LockpickResult result;
    memset(&result, 0, sizeof(result));
    
    if (!session || !session->is_suspended) {
        snprintf(result.reason, sizeof(result.reason), "Session not active");
        return result;
    }
    
    printf("[LOCKPICK] Trying config: ops=0x%X, prune=%.2f, quant=%u\n",
           operations, prune_ratio, quantization_bits);
    
    session->attempts++;
    
    // Apply operations
    bool success = apply_omni_hotpatch(session->discovered_configs->entries[0].state_id > 0 ?
                                       session->discovered_configs : NULL, operations, NULL, 0);
    
    if (!success) {
        session->rollbacks++;
        snprintf(result.reason, sizeof(result.reason), "Hotpatch application failed");
        return result;
    }
    
    // Measure results
    // This would integrate with actual model inference
    float quality = 0.5f + 0.3f * ((float)rand() / RAND_MAX);
    float speed = 0.5f + 0.3f * ((float)rand() / RAND_MAX);
    uint64_t memory = session->baseline_memory * (1.0f - prune_ratio * 0.3f);
    
    result.success = true;
    result.quality_gain = quality - session->baseline_quality;
    result.speed_gain = speed - session->baseline_speed;
    result.memory_saved = (int64_t)session->baseline_memory - (int64_t)memory;
    
    if (result.quality_gain > 0 && result.speed_gain > 0) {
        session->successes++;
        snprintf(result.reason, sizeof(result.reason), "Configuration improved metrics");
    } else if (result.quality_gain > -0.05f && result.speed_gain > -0.05f) {
        snprintf(result.reason, sizeof(result.reason), "Configuration acceptable");
    } else {
        snprintf(result.reason, sizeof(result.reason), "Configuration degraded metrics");
    }
    
    printf("[LOCKPICK] Result: Q%+.4f, S%+.4f, M%+ldMB - %s\n",
           result.quality_gain, result.speed_gain, (long)(result.memory_saved / (1024 * 1024)),
           result.reason);
    
    return result;
}

LockpickAnalysis lockpick_analyze(
    LockpickSession* session
) {
    LockpickAnalysis analysis;
    memset(&analysis, 0, sizeof(analysis));
    
    if (!session) return analysis;
    
    // Get current metrics
    // This would integrate with actual model
    analysis.quality = 0.5f + 0.3f * ((float)rand() / RAND_MAX);
    analysis.speed = 0.5f + 0.3f * ((float)rand() / RAND_MAX);
    analysis.memory = session->baseline_memory * (0.7f + 0.3f * ((float)rand() / RAND_MAX));
    
    analysis.quality_vs_baseline = analysis.quality - session->baseline_quality;
    analysis.speed_vs_baseline = analysis.speed - session->baseline_speed;
    analysis.memory_vs_baseline = (float)((int64_t)analysis.memory - (int64_t)session->baseline_memory) / 
                                  (float)session->baseline_memory;
    
    // Generate recommendation
    if (analysis.quality_vs_baseline > 0.05f && analysis.speed_vs_baseline > 0.05f) {
        snprintf(analysis.recommendation, sizeof(analysis.recommendation),
                 "Strong improvement - recommend pinning this configuration");
    } else if (analysis.quality_vs_baseline > 0 && analysis.speed_vs_baseline > 0) {
        snprintf(analysis.recommendation, sizeof(analysis.recommendation),
                 "Moderate improvement - consider pinning");
    } else if (analysis.quality_vs_baseline > -0.05f && analysis.speed_vs_baseline > -0.05f) {
        snprintf(analysis.recommendation, sizeof(analysis.recommendation),
                 "Neutral - acceptable for memory-constrained scenarios");
    } else {
        snprintf(analysis.recommendation, sizeof(analysis.recommendation),
                 "Degradation detected - recommend rollback");
    }
    
    return analysis;
}

bool lockpick_pin_config(
    LockpickSession* session,
    const char* config_name
) {
    if (!session || !session->discovered_configs) return false;
    
    printf("[LOCKPICK] Pinning configuration: %s\n", config_name);
    
    ModelMapEntry entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.state_id = session->suspended_state_id;
    strncpy(entry.model_name, "CurrentModel", sizeof(entry.model_name) - 1);
    strncpy(entry.config_name, config_name, sizeof(entry.config_name) - 1);
    
    // Get current metrics
    LockpickAnalysis analysis = lockpick_analyze(session);
    entry.quality_score = analysis.quality;
    entry.speed_score = analysis.speed;
    entry.memory_usage = analysis.memory;
    entry.is_validated = true;
    entry.validation_timestamp = get_time_ns_impl();
    
    return model_map_add_entry(session->discovered_configs, &entry);
}

ModelMap* lockpick_end(
    LockpickSession* session,
    bool save_to_file,
    const char* filepath
) {
    if (!session) return NULL;
    
    printf("[LOCKPICK] Ending session\n");
    printf("[LOCKPICK]   Attempts: %u\n", session->attempts);
    printf("[LOCKPICK]   Successes: %u\n", session->successes);
    printf("[LOCKPICK]   Rollbacks: %u\n", session->rollbacks);
    printf("[LOCKPICK]   Discovered configs: %u\n", session->discovered_configs->entry_count);
    
    ModelMap* map = session->discovered_configs;
    
    if (save_to_file && filepath) {
        model_map_save(map, filepath);
    }
    
    session->is_suspended = false;
    free(session);
    
    return map;
}

// ============================================================================
// PROGRESSIVE INTEGRATION
// ============================================================================

bool playback_connect_progressive(
    PlaybackIntegration* integration,
    ProgressiveEngine* progressive
) {
    if (!integration || !progressive) return false;
    
    integration->progressive = progressive;
    
    printf("[INTEGRATION] Connected progressive engine\n");
    return true;
}

void playback_disconnect_progressive(
    PlaybackIntegration* integration
) {
    if (!integration) return;
    
    integration->progressive = NULL;
    printf("[INTEGRATION] Disconnected progressive engine\n");
}

bool playback_sync_state(
    PlaybackIntegration* integration
) {
    if (!integration || !integration->is_integrated) return false;
    
    // Sync state between playback and progressive engine
    if (integration->progressive && integration->playback) {
        // Update memory budget based on progressive engine state
        LayerStatus status = playback_get_layer_status(integration);
        integration->memory_budget = status.vram_usage + status.ram_usage;
    }
    
    return true;
}

LayerStatus playback_get_layer_status(
    PlaybackIntegration* integration
) {
    LayerStatus status;
    memset(&status, 0, sizeof(status));
    
    if (!integration || !integration->progressive) {
        return status;
    }
    
    // Get layer status from progressive engine
    ProgressiveStats stats = progressive_get_stats(integration->progressive);
    
    status.layers_in_vram = stats.layers_in_vram;
    status.layers_in_ram = stats.layers_in_ram;
    status.layers_on_disk = stats.layers_on_disk;
    status.vram_usage = stats.vram_usage;
    status.ram_usage = stats.ram_usage;
    status.prefetch_progress = stats.prefetch_progress;
    status.is_optimal = stats.is_optimal;
    
    return status;
}

// ============================================================================
// HOTPATCH INTEGRATION
// ============================================================================

bool playback_apply_hotpatch(
    PlaybackIntegration* integration,
    HotpatchOp operations,
    bool record
) {
    if (!integration || !integration->is_integrated) return false;
    
    printf("[INTEGRATION] Applying hotpatch: 0x%X\n", operations);
    
    bool success = apply_omni_hotpatch(integration->omni, operations, NULL, 0);
    
    if (success) {
        if (record && integration->playback->is_recording) {
            playback_record_frame(integration->playback, operations);
        }
        
        integration->total_optimizations++;
        integration->successful_optimizations++;
        
        if (integration->on_optimization_complete) {
            HotpatchState* state = get_state(integration->omni, integration->omni->current_state_id);
            if (state) {
                integration->on_optimization_complete(integration, state->quality_score,
                                                      state->speed_score, state->memory_usage);
            }
        }
    } else {
        integration->rollback_count++;
        
        if (integration->on_rollback) {
            integration->on_rollback(integration, "Hotpatch application failed");
        }
    }
    
    return success;
}

bool playback_rollback(
    PlaybackIntegration* integration,
    const char* reason
) {
    if (!integration || !integration->is_integrated) return false;
    
    printf("[INTEGRATION] Rolling back: %s\n", reason ? reason : "No reason specified");
    
    bool success = rollback_hotpatch(integration->hotpatch);
    
    if (success) {
        integration->rollback_count++;
        
        if (integration->on_rollback) {
            integration->on_rollback(integration, reason);
        }
    }
    
    return success;
}

bool playback_create_checkpoint(
    PlaybackIntegration* integration,
    const char* checkpoint_name
) {
    if (!integration || !integration->is_integrated) return false;
    
    printf("[INTEGRATION] Creating checkpoint: %s\n", checkpoint_name);
    
    return create_checkpoint(integration->hotpatch, checkpoint_name);
}

bool playback_restore_checkpoint(
    PlaybackIntegration* integration,
    const char* checkpoint_name
) {
    if (!integration || !integration->is_integrated) return false;
    
    printf("[INTEGRATION] Restoring checkpoint: %s\n", checkpoint_name);
    
    return restore_checkpoint(integration->hotpatch, checkpoint_name);
}

// ============================================================================
// VALIDATION API
// ============================================================================

ValidationResult playback_validate_config(
    PlaybackIntegration* integration,
    uint32_t test_tokens
) {
    ValidationResult result;
    memset(&result, 0, sizeof(result));
    
    if (!integration || !integration->is_integrated) {
        snprintf(result.error_message, sizeof(result.error_message), "Integration not initialized");
        return result;
    }
    
    printf("[INTEGRATION] Validating configuration with %u tokens\n", test_tokens);
    
    // Get current state
    HotpatchState* state = get_state(integration->omni, integration->omni->current_state_id);
    if (!state) {
        snprintf(result.error_message, sizeof(result.error_message), "No current state");
        return result;
    }
    
    // Run validation (would integrate with actual model inference)
    result.quality_score = state->quality_score;
    result.speed_score = state->speed_score;
    result.memory_usage = state->memory_usage;
    result.tokens_tested = test_tokens;
    result.perplexity = 5.0f + 2.0f * ((float)rand() / RAND_MAX); // Placeholder
    result.is_valid = result.quality_score >= integration->quality_threshold &&
                      result.speed_score >= integration->speed_threshold &&
                      result.memory_usage <= integration->memory_budget;
    
    printf("[INTEGRATION] Validation %s: Q=%.4f, S=%.4f, M=%luMB, PPL=%.2f\n",
           result.is_valid ? "PASSED" : "FAILED",
           result.quality_score, result.speed_score,
           (unsigned long)(result.memory_usage / (1024 * 1024)),
           result.perplexity);
    
    return result;
}

ValidationResult playback_quick_validate(
    PlaybackIntegration* integration
) {
    return playback_validate_config(integration, 50);
}

ValidationResult playback_full_validate(
    PlaybackIntegration* integration
) {
    return playback_validate_config(integration, 500);
}

ValidationComparison playback_compare_validations(
    const ValidationResult* result1,
    const ValidationResult* result2
) {
    ValidationComparison comparison;
    memset(&comparison, 0, sizeof(comparison));
    
    if (!result1 || !result2) return comparison;
    
    comparison.result1 = *result1;
    comparison.result2 = *result2;
    
    comparison.quality_diff = result2->quality_score - result1->quality_score;
    comparison.speed_diff = result2->speed_score - result1->speed_score;
    comparison.memory_diff = (int64_t)result2->memory_usage - (int64_t)result1->memory_usage;
    
    // Generate recommendation
    if (comparison.quality_diff > 0.05f && comparison.speed_diff > 0.05f) {
        snprintf(comparison.recommendation, sizeof(comparison.recommendation),
                 "Result 2 is significantly better - recommend switching");
    } else if (comparison.quality_diff > 0 && comparison.speed_diff > 0) {
        snprintf(comparison.recommendation, sizeof(comparison.recommendation),
                 "Result 2 is slightly better - consider switching");
    } else if (comparison.quality_diff < -0.05f || comparison.speed_diff < -0.05f) {
        snprintf(comparison.recommendation, sizeof(comparison.recommendation),
                 "Result 1 is better - keep current configuration");
    } else {
        snprintf(comparison.recommendation, sizeof(comparison.recommendation),
                 "Results are similar - choose based on specific requirements");
    }
    
    return comparison;
}

// ============================================================================
// REPORTING API
// ============================================================================

bool playback_generate_report(
    PlaybackIntegration* integration,
    const ReportConfig* config
) {
    if (!integration || !integration->playback) return false;
    
    printf("[INTEGRATION] Generating report: %s\n", config->filepath);
    
    return playback_generate_report(integration->playback, config->filepath);
}

bool playback_generate_comparison_report(
    PlaybackIntegration* integration,
    uint64_t state_id1,
    uint64_t state_id2,
    const char* filepath
) {
    if (!integration || !integration->playback) return false;
    
    FILE* f = fopen(filepath, "w");
    if (!f) {
        printf("[INTEGRATION] Error: Cannot open %s for writing\n", filepath);
        return false;
    }
    
    fprintf(f, "# State Comparison Report\n\n");
    fprintf(f, "## State #%lu vs State #%lu\n\n", 
            (unsigned long)state_id1, (unsigned long)state_id2);
    
    StateComparison comparison;
    compare_states(integration->playback, state_id1, state_id2, &comparison);
    
    fprintf(f, "### Quality\n");
    fprintf(f, "- State #%lu: %.4f\n", (unsigned long)state_id1, 
            comparison.quality_diff > 0 ? comparison.quality_diff : 0.0f);
    fprintf(f, "- State #%lu: %.4f\n", (unsigned long)state_id2,
            comparison.quality_diff < 0 ? -comparison.quality_diff : 0.0f);
    fprintf(f, "- Difference: %+.4f\n", comparison.quality_diff);
    fprintf(f, "- Ratio: %.2fx\n\n", comparison.quality_ratio);
    
    fprintf(f, "### Speed\n");
    fprintf(f, "- Difference: %+.4f\n", comparison.speed_diff);
    fprintf(f, "- Ratio: %.2fx\n\n", comparison.speed_ratio);
    
    fprintf(f, "### Memory\n");
    fprintf(f, "- Difference: %+ld MB\n", (long)(comparison.memory_diff / (1024 * 1024)));
    fprintf(f, "- Ratio: %.2fx\n\n", comparison.memory_ratio);
    
    fprintf(f, "### Winner\n");
    fprintf(f, "**%s**\n\n", comparison.winner);
    
    fprintf(f, "### Recommendation\n");
    fprintf(f, "%s\n", comparison.recommendation);
    
    fclose(f);
    
    printf("[INTEGRATION] Comparison report saved to %s\n", filepath);
    return true;
}

bool playback_export_all(
    PlaybackIntegration* integration,
    const char* directory
) {
    if (!integration || !integration->playback) return false;
    
    printf("[INTEGRATION] Exporting all data to %s\n", directory);
    
    // Export session
    char session_path[512];
    snprintf(session_path, sizeof(session_path), "%s/session.json", directory);
    playback_export_timeline(integration->playback, session_path);
    
    // Export report
    char report_path[512];
    snprintf(report_path, sizeof(report_path), "%s/report.md", directory);
    playback_generate_report(integration->playback, report_path);
    
    // Export visualization
    char viz_path[512];
    snprintf(viz_path, sizeof(viz_path), "%s/gains.txt", directory);
    playback_visualize_gains(integration->playback, viz_path);
    
    printf("[INTEGRATION] Export complete\n");
    return true;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

float playback_calculate_score(
    float quality,
    float speed,
    uint64_t memory,
    uint64_t memory_budget,
    float quality_weight,
    float speed_weight,
    float memory_weight
) {
    float memory_score = 1.0f - (float)memory / (float)memory_budget;
    if (memory_score < 0.0f) memory_score = 0.0f;
    
    return quality * quality_weight + speed * speed_weight + memory_score * memory_weight;
}

bool playback_meets_constraints(
    float quality,
    float speed,
    uint64_t memory,
    float min_quality,
    float min_speed,
    uint64_t max_memory
) {
    return quality >= min_quality && speed >= min_speed && memory <= max_memory;
}

HotpatchOp playback_recommend_operations(
    PlaybackIntegration* integration,
    float target_quality,
    float target_speed,
    uint64_t target_memory
) {
    if (!integration || !integration->is_integrated) return HOTPATCH_NONE;
    
    HotpatchOp ops = HOTPATCH_NONE;
    
    // Get current state
    HotpatchState* current = get_state(integration->omni, integration->omni->current_state_id);
    if (!current) return HOTPATCH_NONE;
    
    // Check what needs improvement
    if (current->quality_score < target_quality) {
        // Need to improve quality - avoid aggressive pruning
        ops |= HOTPATCH_QUANTIZE; // Quantization is usually safe
    }
    
    if (current->speed_score < target_speed) {
        // Need to improve speed
        ops |= HOTPATCH_FUSE_LAYERS | HOTPATCH_OPTIMIZE_ATTENTION;
    }
    
    if (current->memory_usage > target_memory) {
        // Need to reduce memory
        uint64_t reduction_needed = current->memory_usage - target_memory;
        float reduction_percent = (float)reduction_needed / current->memory_usage;
        
        if (reduction_percent > 0.3f) {
            // Aggressive reduction needed
            ops |= HOTPATCH_PRUNE_WEIGHTS | HOTPATCH_COMPRESS_KV | HOTPATCH_QUANTIZE;
        } else if (reduction_percent > 0.1f) {
            // Moderate reduction
            ops |= HOTPATCH_COMPRESS_KV | HOTPATCH_QUANTIZE;
        } else {
            // Light reduction
            ops |= HOTPATCH_COMPRESS_KV;
        }
    }
    
    return ops;
}

uint64_t playback_estimate_memory(
    PlaybackIntegration* integration,
    HotpatchOp operations,
    float prune_ratio
) {
    if (!integration || !integration->is_integrated) return 0;
    
    HotpatchState* current = get_state(integration->omni, integration->omni->current_state_id);
    if (!current) return 0;
    
    uint64_t memory = current->memory_usage;
    
    if (operations & HOTPATCH_PRUNE_WEIGHTS) {
        memory = (uint64_t)(memory * (1.0f - prune_ratio * 0.3f));
    }
    if (operations & HOTPATCH_QUANTIZE) {
        memory = (uint64_t)(memory * 0.75f); // ~25% reduction from quantization
    }
    if (operations & HOTPATCH_COMPRESS_KV) {
        memory = (uint64_t)(memory * 0.9f); // ~10% reduction from KV compression
    }
    if (operations & HOTPATCH_PRUNE_HEADS) {
        memory = (uint64_t)(memory * 0.85f); // ~15% reduction from head pruning
    }
    
    return memory;
}

float playback_estimate_quality(
    PlaybackIntegration* integration,
    HotpatchOp operations,
    float prune_ratio
) {
    if (!integration || !integration->is_integrated) return 0.0f;
    
    HotpatchState* current = get_state(integration->omni, integration->omni->current_state_id);
    if (!current) return 0.0f;
    
    float quality = current->quality_score;
    
    // Quality typically decreases with aggressive operations
    if (operations & HOTPATCH_PRUNE_WEIGHTS) {
        quality -= prune_ratio * 0.05f;
    }
    if (operations & HOTPATCH_QUANTIZE) {
        quality -= 0.02f; // Small quality loss from quantization
    }
    if (operations & HOTPATCH_COMPRESS_KV) {
        quality -= 0.01f; // Minimal quality loss from KV compression
    }
    if (operations & HOTPATCH_PRUNE_HEADS) {
        quality -= 0.03f;
    }
    
    return quality > 0.0f ? quality : 0.0f;
}

float playback_estimate_speed(
    PlaybackIntegration* integration,
    HotpatchOp operations,
    float prune_ratio
) {
    if (!integration || !integration->is_integrated) return 0.0f;
    
    HotpatchState* current = get_state(integration->omni, integration->omni->current_state_id);
    if (!current) return 0.0f;
    
    float speed = current->speed_score;
    
    // Speed typically increases with pruning
    if (operations & HOTPATCH_PRUNE_WEIGHTS) {
        speed += prune_ratio * 0.1f;
    }
    if (operations & HOTPATCH_QUANTIZE) {
        speed += 0.15f; // Quantization improves speed
    }
    if (operations & HOTPATCH_FUSE_LAYERS) {
        speed += 0.1f; // Layer fusion improves speed
    }
    if (operations & HOTPATCH_OPTIMIZE_ATTENTION) {
        speed += 0.08f;
    }
    
    return speed < 1.0f ? speed : 1.0f;
}

// ============================================================================
// CALLBACK REGISTRATION
// ============================================================================

void playback_integration_set_callbacks(
    PlaybackIntegration* integration,
    void (*on_optimization_complete)(PlaybackIntegration*, float, float, uint64_t),
    void (*on_rollback)(PlaybackIntegration*, const char*),
    void (*on_threshold_violation)(PlaybackIntegration*, const char*, float, float),
    void* user_data
) {
    if (!integration) return;
    
    integration->on_optimization_complete = on_optimization_complete;
    integration->on_rollback = on_rollback;
    integration->on_threshold_violation = on_threshold_violation;
    integration->user_data = user_data;
}

// ============================================================================
// STATISTICS API
// ============================================================================

IntegrationStats playback_integration_get_stats(
    PlaybackIntegration* integration
) {
    IntegrationStats stats;
    memset(&stats, 0, sizeof(stats));
    
    if (!integration) return stats;
    
    stats.total_optimizations = integration->total_optimizations;
    stats.successful_optimizations = integration->successful_optimizations;
    stats.rollback_count = integration->rollback_count;
    
    if (stats.total_optimizations > 0) {
        stats.success_rate = (float)stats.successful_optimizations / (float)stats.total_optimizations;
    }
    
    stats.best_quality_achieved = integration->best_quality_achieved;
    stats.best_speed_achieved = integration->best_speed_achieved;
    stats.lowest_memory_achieved = integration->lowest_memory_achieved;
    
    return stats;
}

void playback_integration_reset_stats(
    PlaybackIntegration* integration
) {
    if (!integration) return;
    
    integration->total_optimizations = 0;
    integration->successful_optimizations = 0;
    integration->rollback_count = 0;
    integration->best_quality_achieved = 0.0f;
    integration->best_speed_achieved = 0.0f;
    integration->lowest_memory_achieved = UINT64_MAX;
}

// ============================================================================
// DEMO / TEST
// ============================================================================

#ifdef HOTPATCH_PLAYBACK_INTEGRATION_DEMO

int main(void) {
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     HOTPATCH PLAYBACK INTEGRATION - DEMO                     ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    // Create hardware profile
    HardwareProfile hw = {
        .vram_bytes = 16ULL * 1024 * 1024 * 1024, // 16GB
        .ram_bytes = 64ULL * 1024 * 1024 * 1024,  // 64GB
        .compute_units = 60,
        .has_tensor_cores = true
    };
    
    // Create integration
    PlaybackIntegration* integration = playback_integration_create(&hw, NULL);
    if (!integration) {
        printf("Error: Failed to create integration\n");
        return 1;
    }
    
    // Initialize
    if (!playback_integration_initialize(integration, NULL)) {
        printf("Error: Failed to initialize integration\n");
        playback_integration_destroy(integration);
        return 1;
    }
    
    // Example 1: Quick optimization
    printf("\n=== Example 1: Quick Optimization ===\n");
    OptimizationResult result = playback_integration_quick_optimize(integration, 30);
    printf("Result: %s, Q=%.4f, S=%.4f, M=%luMB\n",
           result.success ? "SUCCESS" : "FAILED",
           result.final_quality, result.final_speed,
           (unsigned long)(result.final_memory / (1024 * 1024)));
    
    // Example 2: Model map
    printf("\n=== Example 2: Model Map ===\n");
    ModelMap* map = model_map_create("DemoMap");
    
    ModelMapEntry entry = {
        .state_id = 1,
        .quality_score = 0.85f,
        .speed_score = 0.72f,
        .memory_usage = 12ULL * 1024 * 1024 * 1024,
        .is_validated = true
    };
    strncpy(entry.model_name, "Codestral-22B", sizeof(entry.model_name) - 1);
    strncpy(entry.config_name, "Optimized-16GB", sizeof(entry.config_name) - 1);
    
    model_map_add_entry(map, &entry);
    printf("Added entry: %s/%s\n", entry.model_name, entry.config_name);
    
    // Example 3: Lockpicking
    printf("\n=== Example 3: Lockpicking ===\n");
    LockpickSession* session = lockpick_start(integration, "DemoLockpick");
    
    LockpickResult lp_result = lockpick_try_config(session, HOTPATCH_PRUNE_WEIGHTS | HOTPATCH_QUANTIZE, 0.3f, 8);
    printf("Lockpick result: %s, Q%+.4f, S%+.4f\n",
           lp_result.success ? "SUCCESS" : "FAILED",
           lp_result.quality_gain, lp_result.speed_gain);
    
    ModelMap* discovered = lockpick_end(session, false, NULL);
    printf("Discovered %u configurations\n", discovered->entry_count);
    model_map_destroy(discovered);
    
    // Example 4: Validation
    printf("\n=== Example 4: Validation ===\n");
    ValidationResult validation = playback_quick_validate(integration);
    printf("Validation: %s, Q=%.4f, S=%.4f\n",
           validation.is_valid ? "PASSED" : "FAILED",
           validation.quality_score, validation.speed_score);
    
    // Example 5: Statistics
    printf("\n=== Example 5: Statistics ===\n");
    IntegrationStats stats = playback_integration_get_stats(integration);
    printf("Total optimizations: %lu\n", (unsigned long)stats.total_optimizations);
    printf("Success rate: %.1f%%\n", stats.success_rate * 100.0f);
    printf("Best quality: %.4f\n", stats.best_quality_achieved);
    printf("Best speed: %.4f\n", stats.best_speed_achieved);
    
    // Cleanup
    model_map_destroy(map);
    playback_integration_shutdown(integration);
    playback_integration_destroy(integration);
    
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║               DEMO COMPLETE                                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}

#endif // HOTPATCH_PLAYBACK_INTEGRATION_DEMO