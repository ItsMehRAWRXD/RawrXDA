// progressive_system.h - Unified Progressive Layer Loading System
// Complete integration of auto-configuration, hotpatch, and progressive loading
// Part of RawrXD 14-Day Production-Ready Expansion

#ifndef PROGRESSIVE_SYSTEM_H
#define PROGRESSIVE_SYSTEM_H

#include "auto_configurator.h"
#include "live_hotpatch.h"
#include "progressive_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================

typedef struct {
    // Quality/Speed/Memory tradeoff weights
    float quality_weight;
    float speed_weight;
    float memory_weight;
    
    // Hotpatch settings
    bool enable_hotpatch;
    bool enable_auto_rollback;
    float quality_threshold;
    float memory_pressure_threshold;
    
    // Progressive loading settings
    bool enable_prefetch;
    bool enable_auto_tier;
    uint32_t prefetch_depth;
    
    // Model settings
    uint32_t num_layers;
    uint32_t num_heads_per_layer;
    uint32_t context_window;
    
    // Paths
    char model_path[512];
    char checkpoint_path[512];
    char report_path[512];
    
} ProgressiveSystemConfig;

// ============================================================================
// COMPLETE SYSTEM
// ============================================================================

typedef struct {
    // Core systems
    AutoConfig config;
    ProgressiveEngine* engine;
    LiveHotpatch* hotpatch;
    UnifiedMemory* memory;
    
    // Model state
    void* model_weights;
    size_t model_size;
    uint32_t num_parameters;
    
    // Inference state
    uint32_t* token_history;
    uint32_t context_length;
    uint32_t max_context_length;
    
    // Statistics
    uint64_t total_tokens_generated;
    uint64_t total_time_ns;
    float average_tokens_per_second;
    
    // Configuration
    ProgressiveSystemConfig sys_config;
    
} ProgressiveSystem;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Lifecycle
ProgressiveSystem* progressive_system_init(void);
ProgressiveSystem* progressive_system_init_with_config(const ProgressiveSystemConfig* config);
void progressive_system_shutdown(ProgressiveSystem* sys);

// Model loading
int progressive_load_model(ProgressiveSystem* sys, const char* model_path);
int progressive_load_model_from_memory(ProgressiveSystem* sys, void* weights, size_t size);
void progressive_unload_model(ProgressiveSystem* sys);

// Inference
int progressive_generate(ProgressiveSystem* sys, uint32_t prompt_tokens[], uint32_t prompt_len, 
                        uint32_t max_tokens, uint32_t output_tokens[]);
int progressive_generate_streaming(ProgressiveSystem* sys, uint32_t prompt_tokens[], uint32_t prompt_len,
                                   uint32_t max_tokens, 
                                   void (*callback)(uint32_t token, void* context),
                                   void* context);

// Configuration
void progressive_set_quality_weight(ProgressiveSystem* sys, float weight);
void progressive_set_speed_weight(ProgressiveSystem* sys, float weight);
void progressive_set_memory_weight(ProgressiveSystem* sys, float weight);
void progressive_enable_hotpatch(ProgressiveSystem* sys, bool enable);
void progressive_enable_prefetch(ProgressiveSystem* sys, bool enable);
void progressive_set_prefetch_depth(ProgressiveSystem* sys, uint32_t depth);

// Hotpatch
int progressive_hotpatch_for_memory(ProgressiveSystem* sys, uint64_t target_memory_bytes);
int progressive_hotpatch_for_speed(ProgressiveSystem* sys, float target_speedup);
int progressive_hotpatch_for_hardware(ProgressiveSystem* sys);
int progressive_rollback(ProgressiveSystem* sys);

// Statistics
void progressive_get_stats(ProgressiveSystem* sys, ProgressiveStats* stats);
void progressive_get_hotpatch_stats(ProgressiveSystem* sys, HotpatchStats* stats);
void progressive_print_stats(ProgressiveSystem* sys);

// Reporting
void progressive_generate_report(ProgressiveSystem* sys, const char* output_path);
void progressive_generate_json_report(ProgressiveSystem* sys, const char* output_path);

// Utility
uint64_t progressive_estimate_memory(ProgressiveSystem* sys, uint32_t num_layers, QuantType quant);
float progressive_estimate_quality(ProgressiveSystem* sys, HotpatchOp ops);
float progressive_estimate_speedup(ProgressiveSystem* sys, HotpatchOp ops);

#ifdef __cplusplus
}
#endif

#endif // PROGRESSIVE_SYSTEM_H
