// inference_playback_bridge.h - Real Inference Hook for Hotpatch Playback System
// Connects PlaybackController to actual GGUF inference engine
// Part of RawrXD Progressive Layer Loading System

#ifndef INFERENCE_PLAYBACK_BRIDGE_H
#define INFERENCE_PLAYBACK_BRIDGE_H

#include "hotpatch_playback.h"
#include "live_hotpatch.h"
#include "omnidirectional_hotpatch.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// GGUF TENSOR ACCESS (Zero-Copy)
// ============================================================================

// GGUF file header
typedef struct GGUFHeader {
    uint32_t magic;           // "GGUF"
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
} GGUFHeader;

// GGUF tensor info
typedef struct GGUFTensorInfo {
    char* name;
    uint32_t n_dims;
    uint64_t* ne;            // Dimensions
    uint32_t type;           // GGML type
    uint64_t offset;         // Offset in data section
    void* data;              // Pointer to tensor data (memory-mapped)
    uint64_t size;           // Tensor size in bytes
} GGUFTensorInfo;

// Memory-mapped GGUF context
typedef struct GGUFContext {
    // File mapping
    void* file_handle;       // HANDLE on Windows
    void* mapping_handle;    // HANDLE on Windows
    void* base_address;      // Mapped address
    uint64_t file_size;
    
    // Header
    GGUFHeader header;
    
    // Tensors
    GGUFTensorInfo* tensors;
    uint64_t tensor_count;
    uint64_t* tensor_offsets;
    
    // Metadata
    char** metadata_keys;
    char** metadata_values;
    uint64_t metadata_count;
    
    // Architecture info
    uint32_t n_layers;
    uint32_t n_heads;
    uint32_t n_embd;
    uint32_t n_ctx;
    uint32_t vocab_size;
    
    // Quantization
    uint32_t default_quantization;  // Q4_K_M, Q5_K_S, etc.
    float quantization_ratio;        // Bits per weight
    
    // State
    bool is_loaded;
    bool is_mapped;
    char filepath[512];
} GGUFContext;

// ============================================================================
// INFERENCE ENGINE HOOK
// ============================================================================

// Inference metrics (real measurements)
typedef struct InferenceMetrics {
    // Throughput
    float tokens_per_second;
    float tokens_per_second_first_token;
    float ms_per_token;
    
    // Latency
    float prefill_latency_ms;
    float decode_latency_ms;
    float total_latency_ms;
    
    // Memory
    uint64_t vram_used_bytes;
    uint64_t ram_used_bytes;
    uint64_t kv_cache_bytes;
    uint64_t activation_bytes;
    
    // Quality
    float perplexity;
    float kl_divergence;
    float entropy;
    
    // GPU utilization
    float gpu_utilization_percent;
    float memory_bandwidth_gbps;
    uint32_t compute_utilization_percent;
    
    // Batch efficiency
    uint32_t batch_size;
    float batch_efficiency;
    
    // Timestamp
    uint64_t timestamp_ns;
    uint64_t inference_id;
} InferenceMetrics;

// Inference context
typedef struct InferenceContext {
    // Model
    GGUFContext* model;
    char model_path[512];
    
    // KV cache
    void* kv_cache;
    uint64_t kv_cache_size;
    uint32_t n_ctx;
    uint32_t n_ctx_used;
    
    // Batch
    int32_t* batch_tokens;
    uint32_t batch_size;
    uint32_t batch_capacity;
    
    // State
    bool is_initialized;
    bool is_running;
    uint64_t inference_count;
    
    // Metrics history
    InferenceMetrics* metrics_history;
    uint64_t metrics_count;
    uint64_t metrics_capacity;
    
    // Callbacks
    void (*on_token_generated)(uint32_t token, float* logits, void* user_data);
    void (*on_inference_complete)(InferenceMetrics* metrics, void* user_data);
    void* user_data;
    
} InferenceContext;

// ============================================================================
// PLAYBACK → INFERENCE BRIDGE
// ============================================================================

// Bridge between playback system and real inference
typedef struct InferencePlaybackBridge {
    // Core systems
    PlaybackController* playback;
    InferenceContext* inference;
    GGUFContext* gguf;
    
    // Hot-swap state
    bool is_hot_swap_enabled;
    uint64_t active_config_id;
    char active_config_name[128];
    
    // Quantization state
    uint32_t current_quantization;
    float current_prune_ratio;
    uint32_t current_kv_compression;
    
    // Measurement state
    bool is_measuring;
    uint64_t measurement_start_ns;
    uint32_t tokens_generated;
    float total_quality_score;
    
    // Determinism tracking
    uint64_t last_state_hash;
    uint32_t determinism_check_count;
    uint32_t determinism_failures;
    
    // Real metrics (not simulated)
    InferenceMetrics current_metrics;
    InferenceMetrics baseline_metrics;
    InferenceMetrics best_metrics;
    
    // Config hot-swap queue
    uint64_t* config_queue;
    uint32_t config_queue_count;
    uint32_t config_queue_capacity;
    
    // Callbacks
    void (*on_config_swapped)(uint64_t old_config, uint64_t new_config, void* user_data);
    void (*on_metrics_updated)(InferenceMetrics* metrics, void* user_data);
    void (*on_determinism_violation)(uint64_t expected_hash, uint64_t actual_hash, void* user_data);
    void* user_data;
    
} InferencePlaybackBridge;

// ============================================================================
// GGUF LOADER API (Memory-Mapped, Zero-Copy)
// ============================================================================

// Create GGUF context
GGUFContext* gguf_create_context(void);

// Load GGUF file (memory-mapped, zero-copy)
bool gguf_load_file(
    GGUFContext* ctx,
    const char* filepath
);

// Map GGUF file into memory (Windows: CreateFileMapping + MapViewOfFile)
bool gguf_map_file(
    GGUFContext* ctx,
    const char* filepath
);

// Unmap GGUF file
void gguf_unmap_file(
    GGUFContext* ctx
);

// Get tensor by name (zero-copy pointer)
GGUFTensorInfo* gguf_get_tensor(
    GGUFContext* ctx,
    const char* name
);

// Get tensor data (direct pointer to mapped memory)
void* gguf_get_tensor_data(
    GGUFContext* ctx,
    const char* name,
    uint64_t* out_size
);

// Get metadata value
const char* gguf_get_metadata(
    GGUFContext* ctx,
    const char* key
);

// Get architecture info
bool gguf_get_arch_info(
    GGUFContext* ctx,
    uint32_t* n_layers,
    uint32_t* n_heads,
    uint32_t* n_embd,
    uint32_t* n_ctx,
    uint32_t* vocab_size
);

// Destroy GGUF context
void gguf_destroy_context(
    GGUFContext* ctx
);

// ============================================================================
// INFERENCE ENGINE API
// ============================================================================

// Create inference context
InferenceContext* inference_create_context(void);

// Initialize inference with GGUF model
bool inference_initialize(
    InferenceContext* ctx,
    GGUFContext* model,
    uint32_t n_ctx,
    uint32_t batch_size
);

// Run inference (real token generation)
bool inference_forward(
    InferenceContext* ctx,
    const int32_t* tokens,
    uint32_t n_tokens,
    InferenceMetrics* out_metrics
);

// Generate tokens (streaming)
bool inference_generate(
    InferenceContext* ctx,
    const char* prompt,
    uint32_t max_tokens,
    float temperature,
    float top_p,
    InferenceMetrics* out_metrics
);

// Get current metrics
InferenceMetrics* inference_get_metrics(
    InferenceContext* ctx
);

// Reset inference state
void inference_reset(
    InferenceContext* ctx
);

// Destroy inference context
void inference_destroy_context(
    InferenceContext* ctx
);

// ============================================================================
// BRIDGE API (Playback → Real Inference)
// ============================================================================

// Create bridge
InferencePlaybackBridge* bridge_create(
    PlaybackController* playback,
    InferenceContext* inference,
    GGUFContext* gguf
);

// Destroy bridge
void bridge_destroy(
    InferencePlaybackBridge* bridge
);

// ============================================================================
// HOT-SWAP API (Runtime Config Changes)
// ============================================================================

// Hot-swap quantization (re-quantize weights on-the-fly)
typedef struct QuantSwapResult {
    bool success;
    uint32_t old_quant;
    uint32_t new_quant;
    uint64_t time_ns;
    float quality_delta;
    float speed_delta;
    uint64_t memory_delta;
    char error_message[256];
} QuantSwapResult;

QuantSwapResult bridge_swap_quantization(
    InferencePlaybackBridge* bridge,
    uint32_t new_quantization  // Q4_K_M, Q5_K_S, Q8_0, etc.
);

// Hot-swap KV cache compression
typedef struct KVSwapResult {
    bool success;
    uint32_t old_compression;
    uint32_t new_compression;
    uint64_t time_ns;
    float quality_delta;
    char error_message[256];
} KVSwapResult;

KVSwapResult bridge_swap_kv_compression(
    InferencePlaybackBridge* bridge,
    uint32_t new_compression
);

// Hot-swap prune ratio
typedef struct PruneSwapResult {
    bool success;
    float old_ratio;
    float new_ratio;
    uint64_t time_ns;
    uint64_t weights_removed;
    uint64_t memory_saved;
    char error_message[256];
} PruneSwapResult;

PruneSwapResult bridge_swap_prune_ratio(
    InferencePlaybackBridge* bridge,
    float new_ratio
);

// ============================================================================
// REAL MEASUREMENT API (Not Simulated)
// ============================================================================

// Measure actual TPS (tokens per second)
typedef struct TPSMeasurement {
    float tps;
    float tps_first_token;
    float ms_per_token;
    uint32_t tokens_generated;
    uint64_t measurement_time_ns;
    float confidence_interval;
} TPSMeasurement;

TPSMeasurement bridge_measure_tps(
    InferencePlaybackBridge* bridge,
    const char* prompt,
    uint32_t max_tokens,
    uint32_t iterations
);

// Measure actual quality (perplexity)
typedef struct QualityMeasurement {
    float perplexity;
    float kl_divergence;
    float entropy;
    uint32_t tokens_evaluated;
    uint64_t measurement_time_ns;
    char error_message[256];
} QualityMeasurement;

QualityMeasurement bridge_measure_quality(
    InferencePlaybackBridge* bridge,
    const char* test_prompt,
    const char* expected_output
);

// Measure actual memory usage
typedef struct MemoryMeasurement {
    uint64_t vram_used;
    uint64_t ram_used;
    uint64_t kv_cache;
    uint64_t activations;
    uint64_t weights;
    uint64_t overhead;
    uint64_t measurement_time_ns;
} MemoryMeasurement;

MemoryMeasurement bridge_measure_memory(
    InferencePlaybackBridge* bridge
);

// ============================================================================
// DETERMINISM VALIDATION API
// ============================================================================

// State hash for determinism checking
typedef struct StateHash {
    uint64_t tensor_hash;      // Hash of all tensor data
    uint64_t config_hash;      // Hash of configuration
    uint64_t metrics_hash;     // Hash of metrics
    uint64_t combined_hash;    // Combined hash
    uint64_t timestamp_ns;
} StateHash;

// Compute state hash
StateHash bridge_compute_state_hash(
    InferencePlaybackBridge* bridge
);

// Validate determinism (run same config twice, compare hashes)
typedef struct DeterminismResult {
    bool is_deterministic;
    uint64_t hash_run1;
    uint64_t hash_run2;
    uint32_t mismatch_count;
    char mismatch_details[512];
} DeterminismResult;

DeterminismResult bridge_validate_determinism(
    InferencePlaybackBridge* bridge,
    const char* test_prompt,
    uint32_t iterations
);

// ============================================================================
// PLAYBACK HOOK API (Connect to Real Inference)
// ============================================================================

// Hook playback step to real inference
typedef struct PlaybackStepResult {
    bool success;
    uint64_t state_id;
    InferenceMetrics metrics;
    StateHash hash;
    char error_message[256];
} PlaybackStepResult;

// Execute one playback step with real inference
PlaybackStepResult bridge_playback_step(
    InferencePlaybackBridge* bridge,
    HotpatchOp operation,
    const char* test_prompt
);

// Run smoke test with real inference
typedef struct RealSmokeTestResult {
    bool passed;
    uint32_t iterations;
    float avg_tps;
    float avg_quality;
    uint64_t avg_memory;
    float tps_variance;
    float quality_variance;
    uint32_t determinism_failures;
    InferenceMetrics* metrics_timeline;
    uint64_t metrics_count;
    char report_path[512];
} RealSmokeTestResult;

RealSmokeTestResult bridge_run_real_smoke_test(
    InferencePlaybackBridge* bridge,
    SmokeTestConfig* config,
    const char* test_prompt
);

// ============================================================================
// LOCKPICKING WITH REAL INFERENCE
// ============================================================================

// Real lockpicking session (measures actual TPS/quality)
typedef struct RealLockpickSession {
    InferencePlaybackBridge* bridge;
    
    // Baseline (before optimization)
    TPSMeasurement baseline_tps;
    QualityMeasurement baseline_quality;
    MemoryMeasurement baseline_memory;
    
    // Current state
    TPSMeasurement current_tps;
    QualityMeasurement current_quality;
    MemoryMeasurement current_memory;
    
    // Best found
    TPSMeasurement best_tps;
    QualityMeasurement best_quality;
    MemoryMeasurement best_memory;
    uint64_t best_config_id;
    
    // Search state
    uint32_t attempts;
    uint32_t improvements;
    uint32_t regressions;
    uint32_t rollbacks;
    
    // Search strategy
    uint32_t search_mode;  // 0=greedy, 1=hill_climb, 2=annealing
    float exploration_rate;
    float temperature;      // For simulated annealing
    
    // Determinism tracking
    uint32_t determinism_checks;
    uint32_t determinism_failures;
    
} RealLockpickSession;

// Start real lockpicking session
RealLockpickSession* bridge_lockpick_start(
    InferencePlaybackBridge* bridge,
    const char* test_prompt,
    uint32_t search_mode
);

// Try configuration with real measurement
typedef struct RealLockpickResult {
    bool success;
    float tps_delta;
    float quality_delta;
    int64_t memory_delta;
    bool is_improvement;
    char recommendation[256];
} RealLockpickResult;

RealLockpickResult bridge_lockpick_try(
    RealLockpickSession* session,
    HotpatchOp operations,
    float prune_ratio,
    uint32_t quantization
);

// Get best configuration found
typedef struct BestConfig {
    uint64_t config_id;
    HotpatchOp operations;
    float prune_ratio;
    uint32_t quantization;
    float tps;
    float quality;
    uint64_t memory;
    char config_name[128];
} BestConfig;

BestConfig bridge_lockpick_get_best(
    RealLockpickSession* session
);

// End lockpicking session
void bridge_lockpick_end(
    RealLockpickSession* session
);

// ============================================================================
// CONFIG HOT-SWAP WITHOUT RESTART
// ============================================================================

// Queue config for hot-swap
bool bridge_queue_config_swap(
    InferencePlaybackBridge* bridge,
    uint64_t config_id,
    HotpatchOp operations,
    float prune_ratio,
    uint32_t quantization
);

// Execute queued config swaps
typedef struct ConfigSwapResult {
    uint32_t swaps_executed;
    uint32_t swaps_failed;
    uint64_t total_time_ns;
    uint64_t* failed_config_ids;
    uint32_t failed_count;
} ConfigSwapResult;

ConfigSwapResult bridge_execute_config_swaps(
    InferencePlaybackBridge* bridge
);

// ============================================================================
// STREAMING HOOK (Real-Time Token Generation)
// ============================================================================

// Token callback for streaming
typedef void (*TokenCallback)(
    uint32_t token,
    const char* token_text,
    float probability,
    InferenceMetrics* metrics,
    void* user_data
);

// Set token callback
void bridge_set_token_callback(
    InferencePlaybackBridge* bridge,
    TokenCallback callback,
    void* user_data
);

// Generate with streaming
bool bridge_generate_streaming(
    InferencePlaybackBridge* bridge,
    const char* prompt,
    uint32_t max_tokens,
    TokenCallback callback,
    void* user_data
);

// ============================================================================
// BACKPRESSURE CONTROL
// ============================================================================

// Backpressure state
typedef struct BackpressureState {
    bool is_paused;
    uint32_t tokens_in_flight;
    uint32_t max_tokens_in_flight;
    uint64_t last_token_time_ns;
    float target_tps;
    float current_tps;
} BackpressureState;

// Enable backpressure
void bridge_enable_backpressure(
    InferencePlaybackBridge* bridge,
    float target_tps
);

// Get backpressure state
BackpressureState bridge_get_backpressure(
    InferencePlaybackBridge* bridge
);

// Pause/resume generation
void bridge_pause_generation(
    InferencePlaybackBridge* bridge
);

void bridge_resume_generation(
    InferencePlaybackBridge* bridge
);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Convert quantization enum to string
const char* quantization_to_string(uint32_t quant);

// Convert string to quantization enum
uint32_t string_to_quantization(const char* str);

// Get quantization bits per weight
float quantization_bits_per_weight(uint32_t quant);

// Estimate memory for quantization
uint64_t estimate_memory_for_quant(
    GGUFContext* ctx,
    uint32_t quantization
);

// Validate GGUF file
bool validate_gguf_file(const char* filepath);

// Get GGUF file info
typedef struct GGUFInfo {
    uint64_t file_size;
    uint32_t tensor_count;
    uint32_t layer_count;
    uint32_t vocab_size;
    uint32_t context_length;
    char architecture[64];
    char name[128];
} GGUFInfo;

bool get_gguf_info(const char* filepath, GGUFInfo* out_info);

// ============================================================================
// DEMO / TEST
// ============================================================================

#ifdef INFERENCE_PLAYBACK_BRIDGE_DEMO

int main(void);

#endif

#ifdef __cplusplus
}
#endif

#endif // INFERENCE_PLAYBACK_BRIDGE_H
