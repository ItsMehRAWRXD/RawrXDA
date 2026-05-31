// spec_dec_telemetry.c
// Speculative Decoder with Integrated Telemetry Suite
// Shows real-time acceptance tracking, ROI analysis, and expert performance
// Compile: gcc -O2 -o spec_dec_telemetry spec_dec_telemetry.c -lm

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>

#include "spec_telemetry.h"

#if defined(_WIN32)
#include <windows.h>
static inline double time_ms_hr(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / f.QuadPart;
}
#else
#include <time.h>
static inline double time_ms_hr(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
#endif

// Realistic small models for benchmarking
// Draft: 4 layers, 256 dim (~60M params) - fits in L3
// Target: 8 layers, 512 dim (~250M params) - realistic baseline
#define DRAFT_LAYERS    4
#define DRAFT_DIM       256
#define DRAFT_FF        1024
#define TARGET_LAYERS   8
#define TARGET_DIM      512
#define TARGET_FF       2048
#define VOCAB           1000  // Small vocab for speed
#define GAMMA           4

// Simple synthetic model
typedef struct {
    float *w_embed;
    float *w_qkv;      // [layers, 3*dim, dim]
    float *w_out;      // [layers, dim, dim]
    float *w_gate;     // [layers, ff, dim]
    float *w_up;       // [layers, ff, dim]
    float *w_down;     // [layers, dim, ff]
    float *ln1;        // [layers, dim]
    float *ln2;        // [layers, dim]
    float *ln_final;
    float *w_lm;       // [vocab, dim]
    int layers, dim, ff, vocab;
} Model;

static void model_init(Model *m, int layers, int dim, int ff, int vocab) {
    m->layers = layers;
    m->dim = dim;
    m->ff = ff;
    m->vocab = vocab;
    
    m->w_embed = calloc(vocab * dim, sizeof(float));
    m->w_qkv = calloc(layers * 3 * dim * dim, sizeof(float));
    m->w_out = calloc(layers * dim * dim, sizeof(float));
    m->w_gate = calloc(layers * ff * dim, sizeof(float));
    m->w_up = calloc(layers * ff * dim, sizeof(float));
    m->w_down = calloc(layers * dim * ff, sizeof(float));
    m->ln1 = calloc(layers * dim, sizeof(float));
    m->ln2 = calloc(layers * dim, sizeof(float));
    m->ln_final = calloc(dim, sizeof(float));
    m->w_lm = calloc(vocab * dim, sizeof(float));
    
    // Initialize with small random values
    for (int i = 0; i < vocab * dim; i++) m->w_embed[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    for (int i = 0; i < layers * 3 * dim * dim; i++) m->w_qkv[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    for (int i = 0; i < layers * dim * dim; i++) m->w_out[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    for (int i = 0; i < layers * ff * dim; i++) m->w_gate[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    for (int i = 0; i < layers * ff * dim; i++) m->w_up[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    for (int i = 0; i < layers * dim * ff; i++) m->w_down[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
    for (int i = 0; i < layers * dim; i++) m->ln1[i] = 1.0f;
    for (int i = 0; i < layers * dim; i++) m->ln2[i] = 1.0f;
    for (int i = 0; i < dim; i++) m->ln_final[i] = 1.0f;
    for (int i = 0; i < vocab * dim; i++) m->w_lm[i] = ((float)rand() / RAND_MAX - 0.5f) * 0.1f;
}

static float dot(const float *a, const float *b, int n) {
    float s = 0;
    for (int i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

static void mat_vec(const float *w, const float *x, float *y, int m, int n) {
    for (int i = 0; i < m; i++) {
        y[i] = dot(w + i * n, x, n);
    }
}

static void rmsnorm(float *out, const float *x, const float *weight, int n) {
    float ss = 0;
    for (int i = 0; i < n; i++) ss += x[i] * x[i];
    float norm = 1.0f / sqrtf(ss / n + 1e-5f);
    for (int i = 0; i < n; i++) out[i] = x[i] * norm * weight[i];
}

static void silu(float *out, const float *a, const float *b, int n) {
    for (int i = 0; i < n; i++) {
        float x = a[i];
        out[i] = (x / (1.0f + expf(-x))) * b[i];
    }
}

static int sample(const float *logits, int n, float temp) {
    float probs[1000];  // max vocab
    float max_l = logits[0];
    for (int i = 1; i < n; i++) if (logits[i] > max_l) max_l = logits[i];
    
    float sum = 0;
    for (int i = 0; i < n; i++) {
        probs[i] = expf((logits[i] - max_l) / temp);
        sum += probs[i];
    }
    for (int i = 0; i < n; i++) probs[i] /= sum;
    
    // Greedy for determinism
    int best = 0;
    for (int i = 1; i < n; i++) if (probs[i] > probs[best]) best = i;
    return best;
}

static int forward(Model *m, int token, float *kv_cache, int pos, float temp) {
    int dim = m->dim;
    float *x = alloca(dim * sizeof(float));
    memcpy(x, m->w_embed + token * dim, dim * sizeof(float));
    
    for (int l = 0; l < m->layers; l++) {
        // Norm
        float *norm1 = alloca(dim * sizeof(float));
        rmsnorm(norm1, x, m->ln1 + l * dim, dim);
        
        // QKV
        float *qkv = alloca(3 * dim * sizeof(float));
        mat_vec(m->w_qkv + l * 3 * dim * dim, norm1, qkv, 3 * dim, dim);
        
        // Simple attention (causal)
        float *attn_out = alloca(dim * sizeof(float));
        memcpy(attn_out, qkv, dim * sizeof(float));
        
        // Residual
        for (int i = 0; i < dim; i++) attn_out[i] += x[i];
        
        // FFN
        float *norm2 = alloca(dim * sizeof(float));
        rmsnorm(norm2, attn_out, m->ln2 + l * dim, dim);
        
        float *gate = alloca(m->ff * sizeof(float));
        float *up = alloca(m->ff * sizeof(float));
        mat_vec(m->w_gate + l * m->ff * dim, norm2, gate, m->ff, dim);
        mat_vec(m->w_up + l * m->ff * dim, norm2, up, m->ff, dim);
        silu(gate, gate, up, m->ff);
        
        float *ffn_out = alloca(dim * sizeof(float));
        mat_vec(m->w_down + l * dim * m->ff, gate, ffn_out, dim, m->ff);
        
        for (int i = 0; i < dim; i++) x[i] = attn_out[i] + ffn_out[i];
    }
    
    float *norm = alloca(dim * sizeof(float));
    rmsnorm(norm, x, m->ln_final, dim);
    
    float *logits = alloca(m->vocab * sizeof(float));
    mat_vec(m->w_lm, norm, logits, m->vocab, dim);
    
    return sample(logits, m->vocab, temp);
}

// Sample from distribution (greedy for determinism)
static int sample_greedy(const float *logits, int n) {
    int best = 0;
    for (int i = 1; i < n; i++) if (logits[i] > logits[best]) best = i;
    return best;
}

int main() {
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║  Speculative Decoder with Telemetry Suite                        ║\n");
    printf("║  Measures acceptance, ROI, and expert performance               ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    srand(42);
    
    // Initialize models
    printf("Initializing draft model (%d layers, %d dim)...\n", DRAFT_LAYERS, DRAFT_DIM);
    Model draft;
    model_init(&draft, DRAFT_LAYERS, DRAFT_DIM, DRAFT_FF, VOCAB);
    
    printf("Initializing target model (%d layers, %d dim)...\n", TARGET_LAYERS, TARGET_DIM);
    Model target;
    model_init(&target, TARGET_LAYERS, TARGET_DIM, TARGET_FF, VOCAB);
    
    printf("\nAligning draft to target (simulating distilled draft)...\n");
    printf("NOTE: In production, draft weights would be distilled from target.\n");
    printf("      Here we use random weights to show the 'before' state.\n\n");
    
    float *draft_kv = calloc(DRAFT_LAYERS * 2048 * DRAFT_DIM, sizeof(float));
    float *target_kv = calloc(TARGET_LAYERS * 2048 * TARGET_DIM, sizeof(float));
    
    // Warmup
    printf("Warming up...\n");
    for (int i = 0; i < 10; i++) {
        forward(&draft, 100, draft_kv, i, 0.8f);
        forward(&target, 100, target_kv, i, 0.8f);
    }
    
    // Benchmark: Draft speed
    printf("\n--- Draft Model Speed ---\n");
    double t0 = time_ms_hr();
    int tok = 100;
    for (int i = 0; i < 100; i++) {
        tok = forward(&draft, tok, draft_kv, i, 0.8f);
    }
    double t1 = time_ms_hr();
    double draft_tps = 100.0 * 1000.0 / (t1 - t0);
    printf("Draft: %.2f tok/s\n", draft_tps);
    
    // Benchmark: Target speed (baseline)
    printf("\n--- Target Model Speed (Baseline) ---\n");
    t0 = time_ms_hr();
    tok = 100;
    for (int i = 0; i < 20; i++) {
        tok = forward(&target, tok, target_kv, i, 0.8f);
    }
    t1 = time_ms_hr();
    double target_tps = 20.0 * 1000.0 / (t1 - t0);
    printf("Target: %.2f tok/s\n", target_tps);
    
    // Initialize telemetry
    telemetry_init(target_tps, draft_tps);
    
    // Speculative decode with telemetry
    printf("\n--- Speculative Decode with Telemetry ---\n");
    memset(draft_kv, 0, DRAFT_LAYERS * 2048 * DRAFT_DIM * sizeof(float));
    memset(target_kv, 0, TARGET_LAYERS * 2048 * TARGET_DIM * sizeof(float));
    
    int output[100];
    int generated = 0;
    int current = 100;
    int pos = 0;
    
    while (generated < 50) {
        // Generate draft tokens
        double draft_start = time_ms_hr();
        int draft_tokens[GAMMA];
        int draft_cur = current;
        for (int i = 0; i < GAMMA && generated + i < 50; i++) {
            draft_tokens[i] = forward(&draft, draft_cur, draft_kv, pos + i, 0.8f);
            draft_cur = draft_tokens[i];
        }
        double draft_end = time_ms_hr();
        double draft_ms = draft_end - draft_start;
        
        // Verify with target
        double verify_start = time_ms_hr();
        int accepted = 0;
        for (int i = 0; i < GAMMA && generated < 50; i++) {
            int target_next = forward(&target, current, target_kv, pos + i, 0.8f);
            if (target_next == draft_tokens[i]) {
                accepted++;
                output[generated++] = draft_tokens[i];
                current = draft_tokens[i];
            } else {
                output[generated++] = target_next;
                current = target_next;
                break;
            }
        }
        double verify_end = time_ms_hr();
        double verify_ms = verify_end - verify_start;
        
        // Record telemetry frame
        int tokens_produced = (accepted == GAMMA) ? GAMMA : accepted + 1;
        telemetry_record_frame(
            GAMMA,                    // draft_tokens
            accepted,                 // accepted_tokens
            tokens_produced,          // tokens_produced
            draft_ms,                 // draft_latency_ms
            verify_ms,                // verify_latency_ms
            -1,                       // expert_id (no MoE yet)
            pos                       // position
        );
        
        pos += (accepted == GAMMA) ? GAMMA + 1 : accepted + 1;
    }
    
    // Print dashboard
    telemetry_print_dashboard();
    
    // Export JSON for IDE integration
    telemetry_export_json("spec_telemetry.json");
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("NEXT STEPS:\n");
    printf("  1. Implement distillation (expert_distiller.c)\n");
    printf("  2. Wire telemetry into Win32IDE status bar\n");
    printf("  3. Add MoE expert tracking\n");
    printf("  4. Run gamma sweep (1, 2, 4, 8, 16)\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    free(draft.w_embed); free(draft.w_qkv); free(draft.w_out);
    free(draft.w_gate); free(draft.w_up); free(draft.w_down);
    free(draft.ln1); free(draft.ln2); free(draft.ln_final); free(draft.w_lm);
    free(target.w_embed); free(target.w_qkv); free(target.w_out);
    free(target.w_gate); free(target.w_up); free(target.w_down);
    free(target.ln1); free(target.ln2); free(target.ln_final); free(target.w_lm);
    free(draft_kv);
    free(target_kv);
    
    return 0;
}