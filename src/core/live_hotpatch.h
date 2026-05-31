// live_hotpatch.h - Live Hotpatch Pruning System with Rollback
// Automatic model adaptation with safety guarantees
// Part of RawrXD 14-Day Production-Ready Expansion

#ifndef LIVE_HOTPATCH_H
#define LIVE_HOTPATCH_H

#include "auto_configurator.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// HOTPATCH OPERATION TYPES
// ============================================================================

typedef enum {
    HOTPATCH_NONE = 0,
    HOTPATCH_PRUNE_WEIGHTS = 1 << 0,      // Prune low-magnitude weights
    HOTPATCH_QUANTIZE = 1 << 1,           // Quantize to lower precision
    HOTPATCH_COMPRESS_KV = 1 << 2,        // Compress KV cache
    HOTPATCH_FUSE_LAYERS = 1 << 3,        // Fuse consecutive layers
    HOTPATCH_PRUNE_HEADS = 1 << 4,        // Prune attention heads
    HOTPATCH_PRUNE_EXPERTS = 1 << 5,      // Prune MoE experts
    HOTPATCH_MERGE_EMBEDDINGS = 1 << 6,   // Merge embedding layers
    HOTPATCH_OPTIMIZE_ATTENTION = 1 << 7, // Optimize attention patterns
    HOTPATCH_ALL = 0xFF
} HotpatchOp;

// ============================================================================
// IMPORTANCE SCORING METHODS
// ============================================================================

typedef enum {
    IMPORTANCE_MAGNITUDE = 0,       // Weight magnitude
    IMPORTANCE_GRADIENT = 1,        // Gradient-based
    IMPORTANCE_ACTIVATION = 2,      // Activation magnitude
    IMPORTANCE_TAYLOR = 3,          // Taylor expansion
    IMPORTANCE_LOBPCG = 4,          // Eigenvalue-based
    IMPORTANCE_HESSIAN = 5,         // Hessian-based
    IMPORTANCE_SNIP = 6,            // SNIP method
    IMPORTANCE_SYNAPSE = 7,         // Synapse importance
    IMPORTANCE_MIXED = 8            // Combination of methods
} ImportanceMethod;

// ============================================================================
// CHECKPOINT STRUCTURE
// ============================================================================

typedef struct {
    uint64_t checkpoint_id;
    uint64_t timestamp;
    uint64_t model_size_original;
    uint64_t model_size_current;
    uint32_t operation_count;
    
    // Memory state
    uint64_t vram_used;
    uint64_t ram_used;
    
    // Performance state
    float quality_score;
    float speed_score;
    float memory_score;
    
    // Compressed diff for rollback
    void* compressed_diff;
    size_t compressed_size;
    
    // Metadata
    char description[256];
    HotpatchOp operations;
    bool is_valid;
    bool can_restore;
    
} HotpatchCheckpoint;

// ============================================================================
// PRUNING CONFIGURATION
// ============================================================================

typedef struct {
    float sparsity_target;          // Target sparsity (0.0-1.0)
    float magnitude_threshold;      // Weight magnitude threshold
    uint32_t min_layers_to_keep;    // Minimum layers to preserve
    uint32_t min_heads_to_keep;     // Minimum attention heads per layer
    uint32_t min_experts_to_keep;   // Minimum MoE experts
    
    // Importance scoring
    ImportanceMethod importance_method;
    float gradient_importance;      // Weight for gradient-based importance
    float activation_importance;    // Weight for activation-based importance
    
    // Structured pruning
    bool prune_entire_heads;        // Prune complete attention heads
    bool prune_entire_layers;       // Prune complete layers
    bool prune_mlp_first;           // Prune MLP before attention
    
    // Gradual pruning
    uint32_t prune_iterations;      // Number of pruning iterations
    float initial_sparsity;         // Starting sparsity
    float final_sparsity;           // Final sparsity
    uint32_t fine_tune_steps;       // Steps between pruning iterations
    
    // Safety thresholds
    float max_quality_drop;         // Maximum acceptable quality drop
    float min_speed_gain;          // Minimum speed gain to justify
    
} PruningConfig;

// ============================================================================
// HOTPATCH STATISTICS
// ============================================================================

typedef struct {
    uint32_t total_hotpatches;
    uint32_t successful_hotpatches;
    uint32_t failed_hotpatches;
    uint32_t total_rollbacks;
    uint32_t successful_rollbacks;
    uint32_t failed_rollbacks;
    
    uint64_t avg_hotpatch_time_ns;
    uint64_t avg_rollback_time_ns;
    uint64_t total_time_saved_ns;
    
    uint32_t current_checkpoint;
    uint32_t checkpoint_count;
    bool is_pristine;
    
    float quality_impact_total;
    float speed_improvement_total;
    uint64_t memory_saved_total;
    
} HotpatchStats;

// ============================================================================
// LIVE HOTPATCH CONTEXT
// ============================================================================

typedef struct {
    AutoConfig* config;
    
    // Checkpoint history
    HotpatchCheckpoint* checkpoints;
    uint32_t checkpoint_count;
    uint32_t checkpoint_capacity;
    uint64_t next_checkpoint_id;
    
    // Current state
    HotpatchOp current_operations;
    uint32_t current_checkpoint_index;
    bool is_pristine;              // No modifications yet
    
    // Pruning state
    PruningConfig pruning;
    float* weight_importance;      // Per-weight importance scores
    float* layer_importance;       // Per-layer importance scores
    float* head_importance;        // Per-head importance scores
    
    // Monitoring
    uint64_t hotpatch_time_ns;
    uint64_t rollback_time_ns;
    uint32_t total_hotpatches;
    uint32_t total_rollbacks;
    uint32_t successful_hotpatches;
    uint32_t failed_hotpatches;
    
    // Thresholds for automatic hotpatch
    float quality_drop_threshold;   // Max acceptable quality drop
    float memory_pressure_threshold; // Trigger hotpatch at this memory pressure
    float speed_gain_minimum;       // Minimum speed gain to justify hotpatch
    
    // Model state
    void* model_weights;
    size_t model_weights_size;
    uint32_t num_layers;
    uint32_t num_heads_per_layer;
    
} LiveHotpatch;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Lifecycle
LiveHotpatch* live_hotpatch_create(AutoConfig* config);
void live_hotpatch_destroy(LiveHotpatch* ctx);

// Checkpoint management
uint64_t create_checkpoint(LiveHotpatch* ctx, const char* description);
bool rollback_to_checkpoint(LiveHotpatch* ctx, uint64_t checkpoint_id);
bool rollback_last(LiveHotpatch* ctx);
bool verify_checkpoint_integrity(LiveHotpatch* ctx, uint64_t checkpoint_id);
void prune_old_checkpoints(LiveHotpatch* ctx, uint32_t keep_count);

// Core hotpatch operations
bool apply_hotpatch(LiveHotpatch* ctx, HotpatchOp ops, void* model_weights, size_t num_weights);
bool hotpatch_prune_weights(LiveHotpatch* ctx, void* weights, size_t num_weights, float target_sparsity);
bool hotpatch_quantize_layer(LiveHotpatch* ctx, uint32_t layer_idx, QuantType target_quant);
bool hotpatch_compress_kv_cache(LiveHotpatch* ctx, uint32_t compression_level);
bool hotpatch_prune_heads(LiveHotpatch* ctx, uint32_t layer_idx, float prune_ratio);
bool hotpatch_fuse_layers(LiveHotpatch* ctx, uint32_t layer_start, uint32_t layer_end);

// Automatic hotpatching
bool auto_hotpatch_for_memory(LiveHotpatch* ctx, uint64_t target_memory_bytes);
bool auto_hotpatch_for_speed(LiveHotpatch* ctx, float target_speedup);
bool auto_hotpatch_for_hardware(LiveHotpatch* ctx);
bool evaluate_and_hotpatch(LiveHotpatch* ctx);

// Importance scoring
void compute_weight_importance(LiveHotpatch* ctx, void* weights, size_t num_weights);
void compute_layer_importance(LiveHotpatch* ctx);
void compute_head_importance(LiveHotpatch* ctx, uint32_t layer_idx);
float score_pruning_importance(LiveHotpatch* ctx, void* weights, size_t num_weights, 
                               uint32_t start_idx, uint32_t count);

// Estimation
float estimate_quality_impact(LiveHotpatch* ctx, HotpatchOp ops);
uint64_t estimate_memory_savings(LiveHotpatch* ctx, HotpatchOp ops);
float estimate_speedup(LiveHotpatch* ctx);

// Statistics
void get_hotpatch_stats(LiveHotpatch* ctx, HotpatchStats* stats);
void reset_hotpatch_stats(LiveHotpatch* ctx);

// Testing
bool test_rollback_feasibility(LiveHotpatch* ctx);
bool benchmark_hotpatch(LiveHotpatch* ctx, HotpatchOp ops, BenchmarkResult* result);

// Utility
uint64_t get_time_ns(void);
float find_nth_element(float* arr, size_t n, size_t k);

#ifdef __cplusplus
}
#endif

#endif // LIVE_HOTPATCH_H
