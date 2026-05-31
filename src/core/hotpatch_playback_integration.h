// hotpatch_playback_integration.h - Integration Layer for Hotpatch Playback System
// Connects playback controller with progressive engine and live hotpatch system
// Part of RawrXD Progressive Layer Loading System

#ifndef HOTPATCH_PLAYBACK_INTEGRATION_H
#define HOTPATCH_PLAYBACK_INTEGRATION_H

#include "hotpatch_playback.h"
#include "progressive_engine.h"
#include "live_hotpatch.h"
#include "omnidirectional_hotpatch.h"
#include "auto_configurator.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// INTEGRATION TYPES
// ============================================================================

// Integration context that combines all systems
typedef struct PlaybackIntegration {
    // Core systems
    PlaybackController* playback;
    ProgressiveEngine* progressive;
    LiveHotpatch* hotpatch;
    OmnidirectionalHotpatch* omni;
    AutoConfigurator* config;
    
    // Hardware context
    HardwareProfile* hw;
    
    // Model context
    void* model_context;  // Opaque model pointer
    
    // Integration state
    bool is_integrated;
    bool auto_optimize;
    float quality_threshold;
    float speed_threshold;
    uint64_t memory_budget;
    
    // Statistics
    uint64_t total_optimizations;
    uint64_t successful_optimizations;
    uint64_t rollback_count;
    float best_quality_achieved;
    float best_speed_achieved;
    uint64_t lowest_memory_achieved;
    
    // Callbacks
    void (*on_optimization_complete)(struct PlaybackIntegration*, float quality, float speed, uint64_t memory);
    void (*on_rollback)(struct PlaybackIntegration*, const char* reason);
    void (*on_threshold_violation)(struct PlaybackIntegration*, const char* metric, float value, float threshold);
    void* user_data;
    
} PlaybackIntegration;

// Optimization request
typedef struct {
    float target_quality;       // Minimum acceptable quality (0-1)
    float target_speed;         // Minimum acceptable speed (0-1)
    uint64_t max_memory;        // Maximum memory budget in bytes
    uint32_t max_iterations;    // Maximum optimization iterations
    uint32_t max_time_seconds;  // Maximum optimization time
    bool allow_aggressive;      // Allow aggressive pruning
    bool allow_quantization;    // Allow quantization
    bool allow_compression;     // Allow KV compression
    bool record_session;        // Record optimization session
    bool generate_report;       // Generate optimization report
    char session_name[256];      // Session name for recording
} OptimizationRequest;

// Optimization result
typedef struct {
    bool success;
    float final_quality;
    float final_speed;
    uint64_t final_memory;
    uint32_t iterations_used;
    uint64_t time_elapsed_ns;
    uint32_t hotpatches_applied;
    uint32_t rollbacks_performed;
    char best_state_id[64];
    char report_path[512];
} OptimizationResult;

// Auto-tuning configuration
typedef struct {
    bool enable_auto_tune;
    float quality_weight;       // Weight for quality in scoring (0-1)
    float speed_weight;         // Weight for speed in scoring (0-1)
    float memory_weight;        // Weight for memory in scoring (0-1)
    float exploration_rate;     // Rate of random exploration (0-1)
    uint32_t tune_interval_ms;  // Interval between tuning steps
    uint32_t max_tune_steps;    // Maximum tuning steps
    bool adaptive_thresholds;   // Adjust thresholds based on results
} AutoTuneConfig;

// Model map entry - pins perfect tensor/pruning configurations
typedef struct {
    uint64_t state_id;          // State identifier
    char model_name[128];       // Model name
    char config_name[128];      // Configuration name
    
    // Optimal settings
    float quality_score;
    float speed_score;
    uint64_t memory_usage;
    
    // Hotpatch configuration
    HotpatchOp applied_operations;
    float prune_ratio;
    uint32_t quantization_bits;
    float kv_compression_ratio;
    
    // Layer-specific settings
    uint32_t* pruned_layers;
    uint32_t pruned_layer_count;
    float* layer_importance_scores;
    
    // Validation
    bool is_validated;
    uint64_t validation_timestamp;
    float validation_loss;
    
} ModelMapEntry;

// Model map - collection of validated configurations
typedef struct {
    ModelMapEntry* entries;
    uint32_t entry_count;
    uint32_t entry_capacity;
    char map_name[256];
    uint64_t creation_time;
    uint64_t last_updated;
} ModelMap;

// ============================================================================
// INTEGRATION LIFECYCLE
// ============================================================================

// Create integration context
PlaybackIntegration* playback_integration_create(
    HardwareProfile* hw,
    void* model_context
);

// Destroy integration context
void playback_integration_destroy(PlaybackIntegration* integration);

// Initialize all subsystems
bool playback_integration_initialize(
    PlaybackIntegration* integration,
    const AutoConfigSettings* settings
);

// Shutdown all subsystems
void playback_integration_shutdown(PlaybackIntegration* integration);

// ============================================================================
// OPTIMIZATION API
// ============================================================================

// Run optimization session
OptimizationResult playback_integration_optimize(
    PlaybackIntegration* integration,
    const OptimizationRequest* request
);

// Run auto-tuning session
OptimizationResult playback_integration_auto_tune(
    PlaybackIntegration* integration,
    const AutoTuneConfig* config
);

// Quick optimization for immediate use
OptimizationResult playback_integration_quick_optimize(
    PlaybackIntegration* integration,
    uint32_t max_time_seconds
);

// Full optimization for best results
OptimizationResult playback_integration_full_optimize(
    PlaybackIntegration* integration,
    uint32_t max_iterations
);

// ============================================================================
// MODEL MAP API
// ============================================================================

// Create model map
ModelMap* model_map_create(const char* map_name);

// Destroy model map
void model_map_destroy(ModelMap* map);

// Add entry to model map
bool model_map_add_entry(
    ModelMap* map,
    const ModelMapEntry* entry
);

// Find best entry for constraints
ModelMapEntry* model_map_find_best(
    ModelMap* map,
    float min_quality,
    float min_speed,
    uint64_t max_memory
);

// Find entry by name
ModelMapEntry* model_map_find_by_name(
    ModelMap* map,
    const char* model_name,
    const char* config_name
);

// Validate entry
bool model_map_validate_entry(
    PlaybackIntegration* integration,
    ModelMapEntry* entry,
    uint32_t validation_tokens
);

// Save model map to file
bool model_map_save(
    ModelMap* map,
    const char* filepath
);

// Load model map from file
ModelMap* model_map_load(
    const char* filepath
);

// Export model map as JSON
bool model_map_export_json(
    ModelMap* map,
    const char* filepath
);

// ============================================================================
// LOCKPICKING API
// ============================================================================

// "Lockpicking" - suspend model and analyze gains/losses
// This is the core feature for finding optimal configurations

typedef struct {
    uint64_t suspended_state_id;
    bool is_suspended;
    float baseline_quality;
    float baseline_speed;
    uint64_t baseline_memory;
    uint32_t attempts;
    uint32_t successes;
    uint32_t rollbacks;
    ModelMap* discovered_configs;
} LockpickSession;

// Start lockpicking session
LockpickSession* lockpick_start(
    PlaybackIntegration* integration,
    const char* session_name
);

// Try a configuration
typedef struct {
    bool success;
    float quality_gain;
    float speed_gain;
    int64_t memory_saved;
    char reason[256];
} LockpickResult;

LockpickResult lockpick_try_config(
    LockpickSession* session,
    HotpatchOp operations,
    float prune_ratio,
    uint32_t quantization_bits
);

// Analyze current state
typedef struct {
    float quality;
    float speed;
    uint64_t memory;
    float quality_vs_baseline;
    float speed_vs_baseline;
    float memory_vs_baseline;
    char recommendation[512];
} LockpickAnalysis;

LockpickAnalysis lockpick_analyze(
    LockpickSession* session
);

// Pin configuration to model map
bool lockpick_pin_config(
    LockpickSession* session,
    const char* config_name
);

// End lockpicking session
ModelMap* lockpick_end(
    LockpickSession* session,
    bool save_to_file,
    const char* filepath
);

// ============================================================================
// PROGRESSIVE INTEGRATION
// ============================================================================

// Connect playback with progressive engine
bool playback_connect_progressive(
    PlaybackIntegration* integration,
    ProgressiveEngine* progressive
);

// Disconnect from progressive engine
void playback_disconnect_progressive(
    PlaybackIntegration* integration
);

// Sync state between systems
bool playback_sync_state(
    PlaybackIntegration* integration
);

// Get current layer status from progressive engine
typedef struct {
    uint32_t layers_in_vram;
    uint32_t layers_in_ram;
    uint32_t layers_on_disk;
    uint64_t vram_usage;
    uint64_t ram_usage;
    float prefetch_progress;
    bool is_optimal;
} LayerStatus;

LayerStatus playback_get_layer_status(
    PlaybackIntegration* integration
);

// ============================================================================
// HOTPATCH INTEGRATION
// ============================================================================

// Apply hotpatch with playback recording
bool playback_apply_hotpatch(
    PlaybackIntegration* integration,
    HotpatchOp operations,
    bool record
);

// Rollback with playback recording
bool playback_rollback(
    PlaybackIntegration* integration,
    const char* reason
);

// Create checkpoint with playback recording
bool playback_create_checkpoint(
    PlaybackIntegration* integration,
    const char* checkpoint_name
);

// Restore checkpoint with playback recording
bool playback_restore_checkpoint(
    PlaybackIntegration* integration,
    const char* checkpoint_name
);

// ============================================================================
// VALIDATION API
// ============================================================================

// Validate current configuration
typedef struct {
    bool is_valid;
    float quality_score;
    float speed_score;
    uint64_t memory_usage;
    uint32_t tokens_tested;
    float perplexity;
    char error_message[256];
} ValidationResult;

ValidationResult playback_validate_config(
    PlaybackIntegration* integration,
    uint32_t test_tokens
);

// Quick validation (fewer tokens)
ValidationResult playback_quick_validate(
    PlaybackIntegration* integration
);

// Full validation (more tokens)
ValidationResult playback_full_validate(
    PlaybackIntegration* integration
);

// Compare validation results
typedef struct {
    ValidationResult result1;
    ValidationResult result2;
    float quality_diff;
    float speed_diff;
    int64_t memory_diff;
    char recommendation[512];
} ValidationComparison;

ValidationComparison playback_compare_validations(
    const ValidationResult* result1,
    const ValidationResult* result2
);

// ============================================================================
// REPORTING API
// ============================================================================

// Generate comprehensive report
typedef struct {
    char filepath[512];
    uint64_t generation_time_ns;
    uint32_t sections_included;
    bool include_timeline;
    bool include_analytics;
    bool include_model_map;
    bool include_recommendations;
} ReportConfig;

bool playback_generate_report(
    PlaybackIntegration* integration,
    const ReportConfig* config
);

// Generate comparison report
bool playback_generate_comparison_report(
    PlaybackIntegration* integration,
    uint64_t state_id1,
    uint64_t state_id2,
    const char* filepath
);

// Export all data
bool playback_export_all(
    PlaybackIntegration* integration,
    const char* directory
);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Calculate combined score
float playback_calculate_score(
    float quality,
    float speed,
    uint64_t memory,
    uint64_t memory_budget,
    float quality_weight,
    float speed_weight,
    float memory_weight
);

// Check if configuration meets constraints
bool playback_meets_constraints(
    float quality,
    float speed,
    uint64_t memory,
    float min_quality,
    float min_speed,
    uint64_t max_memory
);

// Get recommended operations for target
HotpatchOp playback_recommend_operations(
    PlaybackIntegration* integration,
    float target_quality,
    float target_speed,
    uint64_t target_memory
);

// Estimate memory after operations
uint64_t playback_estimate_memory(
    PlaybackIntegration* integration,
    HotpatchOp operations,
    float prune_ratio
);

// Estimate quality after operations
float playback_estimate_quality(
    PlaybackIntegration* integration,
    HotpatchOp operations,
    float prune_ratio
);

// Estimate speed after operations
float playback_estimate_speed(
    PlaybackIntegration* integration,
    HotpatchOp operations,
    float prune_ratio
);

// ============================================================================
// CALLBACK REGISTRATION
// ============================================================================

void playback_integration_set_callbacks(
    PlaybackIntegration* integration,
    void (*on_optimization_complete)(PlaybackIntegration*, float, float, uint64_t),
    void (*on_rollback)(PlaybackIntegration*, const char*),
    void (*on_threshold_violation)(PlaybackIntegration*, const char*, float, float),
    void* user_data
);

// ============================================================================
// STATISTICS API
// ============================================================================

typedef struct {
    uint64_t total_optimizations;
    uint64_t successful_optimizations;
    uint64_t rollback_count;
    float success_rate;
    float avg_quality_improvement;
    float avg_speed_improvement;
    float avg_memory_saved_mb;
    float best_quality_achieved;
    float best_speed_achieved;
    uint64_t lowest_memory_achieved;
    uint64_t total_time_spent_ns;
    float avg_time_per_optimization_ns;
} IntegrationStats;

IntegrationStats playback_integration_get_stats(
    PlaybackIntegration* integration
);

void playback_integration_reset_stats(
    PlaybackIntegration* integration
);

// ============================================================================
// DEMO / TEST
// ============================================================================

#ifdef HOTPATCH_PLAYBACK_INTEGRATION_DEMO

int main(void);

#endif

#ifdef __cplusplus
}
#endif

#endif // HOTPATCH_PLAYBACK_INTEGRATION_H
