/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_NEXUS - Next-Generation Execution Engine
   
   UNRELEASED features:
   - Speculative Decoding (Draft + Verify)
   - Token-Level Parallelism
   - Dynamic Model Switching Mid-Inference
   - Predictive Prefetching
   - Distributed Attention
   - Self-Correction Loop
   - Confidence-Gated Routing
   - Memory-Augmented Generation
   - Adaptive Quantization
   - Cross-Agent Knowledge Transfer
   
   <5k lines, pure C, no dependencies
   ═══════════════════════════════════════════════════════════════════════════ */

#ifndef RAWRXD_NEXUS_H
#define RAWRXD_NEXUS_H

#include "rawrxd_core.h"
#include "rawrxd_swarm.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   CONFIGURATION
   ═══════════════════════════════════════════════════════════════════════════ */

#define RXD_NEXUS_MAX_DRAFT_TOKENS 8
#define RXD_NEXUS_MAX_VERIFY_BATCH 16
#define RXD_NEXUS_MAX_MEMORY_SLOTS 1024
#define RXD_NEXUS_MAX_PREFETCH_QUEUE 32
#define RXD_NEXUS_MAX_ATTENTION_HEADS 64
#define RXD_NEXUS_MAX_CORRECTION_ITERATIONS 5
#define RXD_NEXUS_TOKEN_CACHE_SIZE 65536
#define RXD_NEXUS_MAX_ROUTING_PATHS 16

/* ═══════════════════════════════════════════════════════════════════════════
   SPECULATIVE DECODING (Draft + Verify)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t draft_model_id;
    uint32_t verify_model_id;
    float draft_temperature;
    float verify_threshold;
    uint32_t max_draft_tokens;
    uint32_t accepted_tokens;
    uint32_t rejected_tokens;
    float acceptance_rate;
} RXDSpeculativeConfig;

typedef struct {
    int32_t tokens[RXD_NEXUS_MAX_DRAFT_TOKENS];
    float probabilities[RXD_NEXUS_MAX_DRAFT_TOKENS];
    uint32_t count;
    uint32_t accepted_count;
    uint64_t generate_time_ns;
    uint64_t verify_time_ns;
} RXDSpeculativeResult;

typedef struct {
    RXDSpeculativeConfig config;
    RXDSpeculativeResult last_result;
    uint64_t total_draft_tokens;
    uint64_t total_accepted;
    uint64_t total_rejected;
    float speedup_factor;
    float acceptance_rate;  /* Runtime acceptance rate calculation */
    bool enabled;
} RXDSpeculativeEngine;

/* ═══════════════════════════════════════════════════════════════════════════
   TOKEN-LEVEL PARALLELISM
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int32_t token_id;
    float logprob;
    uint32_t position;
    bool is_speculative;
    bool is_verified;
    uint64_t generated_time_ns;
} RXDTokenSlot;

typedef struct {
    RXDTokenSlot slots[RXD_NEXUS_MAX_VERIFY_BATCH];
    uint32_t active_slots;
    uint32_t completed_slots;
    uint32_t parallel_factor;
    uint64_t batch_start_ns;
    uint64_t batch_end_ns;
    float throughput_multiplier;
} RXDTokenParallel;

/* ═══════════════════════════════════════════════════════════════════════════
   DYNAMIC MODEL SWITCHING (Mid-Inference)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    RXD_MODEL_TINY,    /* <1B, ultra-fast */
    RXD_MODEL_SMALL,   /* 1-3B, fast */
    RXD_MODEL_MEDIUM,  /* 3-7B, balanced */
    RXD_MODEL_LARGE,   /* 7-13B, accurate */
    RXD_MODEL_XL,      /* 13-70B, very accurate */
    RXD_MODEL_XXL      /* 70B+, maximum accuracy */
} RXDModelSize;

typedef struct {
    RXDModelSize size;
    char path[512];
    void* hotswap;  /* RXDHotswap* - opaque pointer */
    bool is_loaded;
    bool is_active;
    float load_time_ms;
    uint64_t vram_usage;
    float tokens_per_sec;
    float accuracy_score;
} RXDModelInstance;

typedef struct {
    RXDModelInstance models[16];  /* RXD_SWARM_MAX_AGENTS equivalent */
    uint32_t model_count;
    uint32_t active_model;
    uint32_t switch_count;
    uint64_t total_switch_time_ns;
    float avg_switch_time_ms;
    bool auto_switch_enabled;
    float complexity_threshold;
    uint32_t complexity_window;
    float* complexity_history;
    uint32_t complexity_idx;
} RXDModelRouter;

/* ═══════════════════════════════════════════════════════════════════════════
   PREDICTIVE PREFETCHING
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char predicted_prompt[4096];  /* RXD_MAX_PROMPT equivalent */
    float confidence;
    uint32_t priority;
    bool is_prefetched;
    uint64_t prefetch_time_ns;
    size_t context_size;
} RXDPrefetchEntry;

typedef struct {
    RXDPrefetchEntry queue[RXD_NEXUS_MAX_PREFETCH_QUEUE];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t hits;
    uint32_t misses;
    float hit_rate;
    uint64_t total_prefetch_time_ns;
    uint64_t total_latency_saved_ns;
    bool enabled;
} RXDPrefetchEngine;

/* ═══════════════════════════════════════════════════════════════════════════
   DISTRIBUTED ATTENTION
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t head_id;
    float* attention_weights;
    uint32_t num_tokens;
    uint32_t assigned_model;
    uint64_t compute_time_ns;
} RXDAttentionHead;

typedef struct {
    RXDAttentionHead heads[RXD_NEXUS_MAX_ATTENTION_HEADS];
    uint32_t active_heads;
    uint32_t distribution_strategy; /* 0=round-robin, 1=load-balance, 2=attention-based */
    uint64_t total_compute_ns;
    float parallel_efficiency;
} RXDDistributedAttention;

/* ═══════════════════════════════════════════════════════════════════════════
   SELF-CORRECTION LOOP
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char original_output[8192];  /* RXD_MAX_AGENT_OUTPUT equivalent */
    char corrected_output[8192];
    char criticism[4096];
    float quality_before;
    float quality_after;
    uint32_t iterations;
    bool is_corrected;
    char errors_found[10][256];
    uint32_t error_count;
} RXDSelfCorrection;

/* ═══════════════════════════════════════════════════════════════════════════
   CONFIDENCE-GATED ROUTING
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    float confidence_threshold_low;
    float confidence_threshold_high;
    uint32_t low_confidence_count;
    uint32_t high_confidence_count;
    uint32_t routed_count;
    float avg_confidence;
    RXDModelSize last_routed_size;
} RXDConfidenceRouter;

typedef struct {
    int32_t token;
    float confidence;
    RXDModelSize routed_to;
    bool was_rerouted;
} RXDRoutedToken;

/* ═══════════════════════════════════════════════════════════════════════════
   MEMORY-AUGMENTED GENERATION
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char key[256];
    char value[4096];
    float relevance_score;
    uint64_t last_accessed;
    uint32_t access_count;
    bool is_locked;
} RXDMemorySlot;

typedef struct {
    RXDMemorySlot slots[RXD_NEXUS_MAX_MEMORY_SLOTS];
    uint32_t used_slots;
    uint32_t capacity;
    float decay_factor;
    uint64_t total_retrievals;
    uint64_t retrieval_hits;
    float retrieval_hit_rate;
} RXDMemoryBank;

/* ═══════════════════════════════════════════════════════════════════════════
   ADAPTIVE QUANTIZATION (Mid-Inference)
   Note: GGMLType is defined in rawrxd_core.h - use that definition
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t layer_idx;
    GGMLType current_type;
    GGMLType target_type;
    float importance_score;
    bool is_switched;
    uint64_t switch_time_ns;
} RXDLayerQuant;

typedef struct {
    RXDLayerQuant layers[128];  /* RXD_MAX_LAYERS equivalent */
    uint32_t layer_count;
    uint32_t switched_layers;
    uint64_t total_switch_time_ns;
    float memory_saved_bytes;
    float quality_loss;
    bool auto_adjust_enabled;
} RXDAdaptiveQuant;

/* ═══════════════════════════════════════════════════════════════════════════
   CROSS-AGENT KNOWLEDGE TRANSFER
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char knowledge_key[256];
    char knowledge_value[4096];
    char source_agent[64];
    char recipient_agents[16][64];  /* Fixed array of agent names */
    uint32_t recipient_count;
    uint64_t transfer_time_ns;
    float transfer_quality;
} RXDKnowledgeTransfer;

typedef struct {
    RXDKnowledgeTransfer transfers[64];
    uint32_t transfer_count;
    uint64_t total_transfers;
    uint64_t successful_transfers;
    float avg_transfer_quality;
    char shared_memory[8192];
} RXDKnowledgeNetwork;

/* ═══════════════════════════════════════════════════════════════════════════
   NEXUS UNIFIED ENGINE
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Core systems */
    RXDSpeculativeEngine speculative;
    RXDTokenParallel token_parallel;
    RXDModelRouter router;
    RXDPrefetchEngine prefetch;
    RXDDistributedAttention attention;
    RXDSelfCorrection correction;
    RXDConfidenceRouter confidence;
    RXDMemoryBank memory;
    RXDAdaptiveQuant quant;
    RXDKnowledgeNetwork knowledge;
    
    /* State */
    bool is_initialized;
    bool is_running;
    uint64_t start_time;
    uint64_t total_tokens;
    float effective_tps;
    float quality_score;
    
    /* Metrics */
    float speedup_over_baseline;
    float memory_efficiency;
    float quality_efficiency;
    float overall_efficiency;
} RXDNexusEngine;

/* Nexus inference result structure - distinct from core RXDInferenceResult */
typedef struct {
    char content[8192];
    uint32_t tokens;
    float tps;
    float first_token_ms;
    double total_ms;
    bool success;
} RXDNexusInferenceResult;

/* Status structure */
typedef struct {
    float speculative_acceptance;
    float speculative_speedup;
    float parallel_multiplier;
    float prefetch_hit_rate;
    float attention_efficiency;
    float memory_hit_rate;
    float correction_improvement;
    float overall_speedup;
    uint32_t active_model;
    uint32_t total_tokens;
    float effective_tps;
} RXDNexusStatus;

/* ═══════════════════════════════════════════════════════════════════════════
   API FUNCTIONS
   ═══════════════════════════════════════════════════════════════════════════ */

/* Initialization and cleanup */
bool rxd_nexus_init(const char* model_paths[], uint32_t model_count);
void rxd_nexus_cleanup(void);

/* Core inference */
RXDNexusInferenceResult rxd_nexus_infer(const char* prompt);

/* Status and reporting */
RXDNexusStatus rxd_nexus_get_status(void);
char* rxd_nexus_report_json(void);

/* Debug snapshots for smoke tests and benchmarks */
RXDSpeculativeEngine* rxd_nexus_debug_speculative(void);
RXDTokenParallel* rxd_nexus_debug_token_parallel(void);
RXDModelRouter* rxd_nexus_debug_router(void);
RXDPrefetchEngine* rxd_nexus_debug_prefetch(void);
RXDDistributedAttention* rxd_nexus_debug_attention(void);
RXDSelfCorrection* rxd_nexus_debug_correction(void);
RXDConfidenceRouter* rxd_nexus_debug_confidence(void);
RXDMemoryBank* rxd_nexus_debug_memory(void);
RXDAdaptiveQuant* rxd_nexus_debug_quant(void);
RXDKnowledgeNetwork* rxd_nexus_debug_knowledge(void);
RXDNexusEngine* rxd_nexus_debug_engine(void);

/* Speculative decoding */
void rxd_spec_init(uint32_t draft_model, uint32_t verify_model, float threshold);
RXDSpeculativeResult rxd_spec_generate_draft(const char* context, uint32_t max_tokens);
RXDSpeculativeResult rxd_spec_verify(RXDSpeculativeResult* draft, const char* context);

/* Token parallelism */
void rxd_token_parallel_init(uint32_t parallel_factor);
int32_t rxd_token_parallel_allocate(int32_t hint_token, float hint_prob);
bool rxd_token_parallel_verify(uint32_t slot_idx);
float rxd_token_parallel_get_multiplier(void);

/* Model routing */
void rxd_router_init(uint32_t window_size);
bool rxd_router_load_model(RXDModelSize size, const char* path);
float rxd_router_measure_complexity(const char* prompt);
RXDModelSize rxd_router_select_model(float complexity);
bool rxd_router_switch_to(RXDModelSize target_size);
void rxd_router_auto_switch(const char* prompt);

/* Prefetching */
void rxd_prefetch_init(void);
RXDPrefetchEntry rxd_prefetch_predict(const char* conversation_history);
bool rxd_prefetch_execute(RXDPrefetchEntry* entry);
bool rxd_prefetch_check(const char* prompt);

/* Distributed attention */
void rxd_dist_attn_init(uint32_t strategy);
void rxd_dist_attn_distribute(uint32_t num_heads, uint32_t num_models);
void rxd_dist_attn_compute_parallel(void);

/* Self-correction */
RXDSelfCorrection rxd_run_self_correction(const char* initial_output, uint32_t max_iterations);

/* Confidence routing */
void rxd_confidence_router_init(float low_thresh, float high_thresh);
RXDRoutedToken rxd_confidence_route(int32_t token, float confidence);

/* Memory augmentation */
void rxd_memory_init(uint32_t capacity);
bool rxd_memory_store(const char* key, const char* value);
const char* rxd_memory_retrieve(const char* key);
const char* rxd_memory_search(const char* query, float* out_relevance);

/* Adaptive quantization */
void rxd_adapt_quant_init(uint32_t layer_count);
float rxd_adapt_quant_analyze_layer(uint32_t layer_idx, const char* layer_name);
bool rxd_adapt_quant_switch_layer(uint32_t layer_idx, GGMLType new_type);
void rxd_adapt_quant_auto_adjust(float vram_pressure);

/* Knowledge transfer */
bool rxd_knowledge_transfer(const char* key, const char* value,
                            const char* source, char** recipients, 
                            uint32_t recipient_count);
const char* rxd_knowledge_query(const char* key);

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_NEXUS_H */
