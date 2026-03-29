#pragma once
#define LLAMA_STUB_H_INCLUDED  // Guard to prevent forward declaration conflicts

// llama_stub.h - Stub definitions for llama.cpp API
// Used when llama.h is not available in build include paths

#include <cstddef>
#include <cstdint>

struct llama_context {};

struct llama_token_data {
    int32_t id;
    float logit;
    float p;
};

struct llama_token_data_array {
    llama_token_data* data;
    size_t size;
    bool sorted;
};

struct llama_sampler {};

struct llama_sampler_chain_params {
    int n_prev;
    int n_probs;
};

// Stub functions for missing llama API
static inline llama_sampler_chain_params llama_sampler_chain_default_params() {
    return {32, 0};
}

static inline llama_sampler* llama_sampler_chain_init(llama_sampler_chain_params) {
    return nullptr;
}

static inline void llama_sampler_chain_add(llama_sampler*, llama_sampler*) {}

static inline llama_sampler* llama_sampler_init_temp(float) {
    return nullptr;
}

static inline llama_sampler* llama_sampler_init_top_k(int) {
    return nullptr;
}

static inline llama_sampler* llama_sampler_init_top_p(float, int) {
    return nullptr;
}

static inline llama_sampler* llama_sampler_init_dist(int) {
    return nullptr;
}

static inline void llama_sampler_apply(llama_sampler*, llama_token_data_array*) {}

static inline int32_t llama_sampler_sample(llama_sampler*, llama_context*, int) {
    return -1;
}

static inline void llama_sampler_free(llama_sampler*) {}

static inline int32_t llama_get_kv_cache_token_count(llama_context*) {
    return 0;
}

static inline void llama_kv_cache_seq_rm(llama_context*, int, int, int) {}
