// auto_configurator.h - Auto-Configuration for Progressive Layer Loading
// Brute-force optimal settings detection with hardware profiling
// Part of RawrXD 14-Day Production-Ready Expansion

#ifndef AUTO_CONFIGURATOR_H
#define AUTO_CONFIGURATOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// QUANTIZATION TYPES
// ============================================================================

typedef enum {
    QINT2,      // 2-bit quantization
    QINT3,      // 3-bit quantization
    QINT4,      // 4-bit quantization
    QINT8,      // 8-bit quantization
    QFP8,       // 8-bit floating point (E4M3, E5M2)
    QFP16,      // 16-bit floating point (BF16, FP16)
    QFP32,      // 32-bit floating point (FP32)
    QMIXED      // Mixed precision per layer
} QuantType;

// Correction modes for quantization error
typedef enum {
    CORR_NONE,      // No correction
    CORR_SPARSE,    // Sparse outlier correction
    CORR_SVD,       // SVD low-rank correction
    CORR_ADAPTIVE,  // Adaptive correction based on layer
    CORR_HOTPATCH   // Live hotpatch with rollback
} CorrectionMode;

// ============================================================================
// HARDWARE PROFILE
// ============================================================================

typedef struct {
    // Memory tiers
    uint64_t vram_bytes;
    uint64_t ram_bytes;
    uint64_t disk_bytes;
    
    // Bandwidth
    float vram_bandwidth_gbps;
    float ram_bandwidth_gbps;
    float disk_read_speed_mbps;
    float disk_write_speed_mbps;
    
    // GPU compute
    uint32_t num_cuda_cores;
    uint32_t num_tensor_cores;
    uint32_t gpu_clock_mhz;
    uint32_t gpu_sm_count;
    uint32_t gpu_warp_size;
    
    // CPU
    uint32_t cpu_cores;
    uint32_t cpu_threads;
    uint32_t cpu_clock_mhz;
    uint32_t numa_nodes;
    uint32_t ram_channels;
    
    // Computed metrics
    float vram_to_ram_ratio;
    float compute_to_memory_ratio;
    float bandwidth_efficiency;
    float theoretical_tflops;
    
    // GPU info
    char gpu_name[256];
    uint32_t gpu_vendor;  // 0=Unknown, 1=NVIDIA, 2=AMD, 3=Intel
    uint32_t compute_capability_major;
    uint32_t compute_capability_minor;
    
} HardwareProfile;

// ============================================================================
// QUANTIZATION CONFIGURATION
// ============================================================================

typedef struct {
    QuantType weight_quant;
    QuantType activation_quant;
    QuantType kv_cache_quant;
    
    CorrectionMode correction_mode;
    uint32_t correction_rank;       // For SVD
    float outlier_percentile;        // For sparse
    float sparsity_threshold;        // For pruning
    
    uint32_t block_size;
    uint32_t group_size;
    
    // Layer distribution
    uint32_t layers_in_vram;
    uint32_t layers_in_ram;
    uint32_t layers_on_disk;
    uint32_t recompute_layers;
    
    // KV cache
    uint32_t kv_cache_layers;
    uint32_t context_window;
    uint32_t cache_compression_level;
    
    // Scoring
    float quality_score;
    float speed_score;
    float memory_score;
    float combined_score;
    
    // Memory requirements
    uint64_t vram_required;
    uint64_t ram_required;
    uint64_t disk_required;
    
} QuantConfig;

// ============================================================================
// BENCHMARK RESULTS
// ============================================================================

typedef struct {
    float perplexity;
    float quality_metric;
    float efficiency_metric;
    
    uint32_t tokens_per_second;
    uint32_t latency_ms;
    uint32_t time_to_first_token_ms;
    
    uint32_t vram_peak_mb;
    uint32_t ram_peak_mb;
    
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t layer_swaps;
    uint64_t prefetch_hits;
    uint64_t prefetch_misses;
    
    uint32_t benchmark_iterations;
    uint32_t benchmark_tokens;
    
} BenchmarkResult;

// ============================================================================
// PROGRESSIVE LOADING METHODS
// ============================================================================

typedef enum {
    PROG_STANDARD,           // Standard layer-by-layer
    PROG_PREFETCH,           // Prefetch next layers
    PROG_PIPELINED,          // Pipeline loading and execution
    PROG_SPECULATIVE,        // Speculative execution
    PROG_RECOMPUTE,          // Recompute instead of cache
    PROG_COMPRESSED_CACHE,   // Compress KV cache
    PROG_ATTENTION_SLIDING,  // Sliding window attention
    PROG_DYNAMIC_QUANT,      // Dynamic quantization levels
    PROG_MIXED_PRECISION,    // Mixed precision per layer
    PROG_ADAPTIVE_BATCH,     // Adaptive batch sizes
    PROG_GRADIENT_CHECKPOINT,// Gradient checkpointing
    PROG_HOTPATCH_PRUNE,     // Live hotpatch pruning
    
    NUM_PROGRESSIVE_METHODS
} ProgressiveMethod;

// ============================================================================
// ADVANCED METHODS (Research)
// ============================================================================

typedef enum {
    UNKNOWN_NONE = 0,
    
    // Compression methods
    UNKNOWN_WEIGHT_PRUNING = 1 << 0,
    UNKNOWN_SPARSE_ATTENTION = 1 << 1,
    UNKNOWN_LOW_RANK_APPROX = 1 << 2,
    UNKNOWN_KNOWLEDGE_DISTILLATION = 1 << 3,
    
    // Memory optimization
    UNKNOWN_FLASH_ATTENTION = 1 << 4,
    UNKNOWN_PAGED_ATTENTION = 1 << 5,
    UNKNOWN_RING_ATTENTION = 1 << 6,
    UNKNOWN_LINEAR_ATTENTION = 1 << 7,
    
    // Parallelism
    UNKNOWN_TENSOR_PARALLEL = 1 << 8,
    UNKNOWN_PIPELINE_PARALLEL = 1 << 9,
    UNKNOWN_SEQUENCE_PARALLEL = 1 << 10,
    UNKNOWN_EXPERT_PARALLEL = 1 << 11,
    
    // Quantization research
    UNKNOWN_DYNAMIC_QUANT = 1 << 12,
    UNKNOWN_MIXED_PRECISION = 1 << 13,
    UNKNOWN_LOGARITHMIC_QUANT = 1 << 14,
    UNKNOWN_VECTOR_QUANT = 1 << 15,
    
    // Inference optimization
    UNKNOWN_SPECULATIVE_DECODING = 1 << 16,
    UNKNOWN_EARLY_EXIT = 1 << 17,
    UNKNOWN_ADAPTIVE_COMPUTE = 1 << 18,
    UNKNOWN_MEGA_CHOICE = 1 << 19,
    
    // Novel methods
    UNKNOWN_CHUNKED_ATTENTION = 1 << 20,
    UNKNOWN_MULTI_QUERY_ATTENTION = 1 << 21,
    UNKNOWN_GROUPED_QUERY_ATTENTION = 1 << 22,
    UNKNOWN_SLIDING_WINDOW = 1 << 23,
    UNKNOWN_LOCAL_ATTENTION = 1 << 24,
    
    // Hotpatch methods
    UNKNOWN_HOTPATCH_PRUNE = 1 << 25,
    UNKNOWN_HOTPATCH_QUANT = 1 << 26,
    UNKNOWN_HOTPATCH_RESTORE = 1 << 27,
    
    ALL_METHODS = 0xFFFFFFFF
    
} UnknownMethod;

// ============================================================================
// HOTPATCH ROLLBACK STATE
// ============================================================================

typedef struct {
    uint32_t layer_index;
    uint32_t original_quant;
    uint32_t new_quant;
    float original_sparsity;
    float new_sparsity;
    
    void* original_weights;      // Backup of original weights
    size_t original_weights_size;
    
    void* pruned_weights;        // Pruned version
    size_t pruned_weights_size;
    
    uint64_t timestamp;
    bool is_applied;
    bool can_rollback;
    
    // Performance metrics
    float quality_before;
    float quality_after;
    float speed_before;
    float speed_after;
    float memory_before;
    float memory_after;
    
} HotpatchState;

// ============================================================================
// COMPLETE AUTO-CONFIGURATION
// ============================================================================

typedef struct {
    HardwareProfile hw;
    QuantConfig quant;
    ProgressiveMethod prog_method;
    UnknownMethod unknown_methods;
    
    // Brute-force parameters
    uint32_t search_iterations;
    uint32_t benchmark_duration_ms;
    uint32_t max_configs_to_test;
    
    // Results
    BenchmarkResult best_result;
    QuantConfig best_config;
    
    // Hotpatch state
    HotpatchState* hotpatch_states;
    uint32_t num_hotpatch_states;
    bool hotpatch_enabled;
    bool auto_rollback_enabled;
    float quality_threshold_for_rollback;
    
} AutoConfig;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Hardware detection
HardwareProfile detect_hardware(void);
void print_hardware_profile(const HardwareProfile* hw);

// Benchmarking
void benchmark_config(QuantConfig* config, BenchmarkResult* result, uint32_t iterations);
void benchmark_progressive_method(ProgressiveMethod method, BenchmarkResult* result);
void benchmark_unknown_method(UnknownMethod method, BenchmarkResult* result);

// Auto-configuration
void auto_configure(AutoConfig* cfg, float quality_weight, float speed_weight, float memory_weight);
void find_optimal_config(AutoConfig* cfg);
void calculate_layer_distribution(QuantConfig* config, HardwareProfile* hw);

// Memory estimation
uint64_t estimate_vram_usage(QuantConfig* config, HardwareProfile* hw);
uint64_t estimate_ram_usage(QuantConfig* config, HardwareProfile* hw);
uint64_t estimate_disk_usage(QuantConfig* config, HardwareProfile* hw);

// Hotpatch with rollback
int hotpatch_apply_pruning(AutoConfig* cfg, uint32_t layer, float target_sparsity);
int hotpatch_apply_quantization(AutoConfig* cfg, uint32_t layer, QuantType new_quant);
int hotpatch_rollback(AutoConfig* cfg, uint32_t layer);
int hotpatch_rollback_all(AutoConfig* cfg);
void hotpatch_auto_adapt(AutoConfig* cfg, float quality_threshold, float memory_target);

// Progressive loading
void test_progressive_method(ProgressiveMethod method, BenchmarkResult* result);
void test_unknown_method(UnknownMethod method, BenchmarkResult* result);

// Reporting
void generate_report(AutoConfig* cfg, const char* output_path);
void generate_json_report(AutoConfig* cfg, const char* output_path);

#ifdef __cplusplus
}
#endif

#endif // AUTO_CONFIGURATOR_H
