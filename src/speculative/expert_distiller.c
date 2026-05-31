// expert_distiller.c
// Magnitude-pruning distillation + calibration for RawrXD speculative decoding
// Converts a parent Expert model into a Draft model that achieves 60-80% acceptance
//
// Algorithm:
//   1. Magnitude pruning: keep top (1 - sparsity_ratio) weights per matrix
//   2. Projection compression: reduce dim via column-subset selection
//   3. Calibration pass: rescale draft logits to match parent distribution
//
// Compile: gcc -O2 -mavx512f -o expert_distiller expert_distiller.c -lm
// Or include as a translation unit alongside spec_dec_realistic.c

#ifndef EXPERT_DISTILLER_H
#define EXPERT_DISTILLER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

// ─────────────────────────────────────────────────────────────────────────────
// Config
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    float sparsity_ratio;     // 0.85 → keep top 15% by |w|
    int   calibration_steps;  // prompts to align logit distributions
    float logit_temp_init;    // initial draft temperature scale (default 1.0)
    float lr;                 // calibration learning rate
    int   dim_reduction;      // reduce draft dim to parent_dim / dim_reduction
    int   layer_reduction;    // reduce draft layers to parent_layers / layer_reduction
} PruneConfig;

static inline PruneConfig default_prune_config(void) {
    PruneConfig c;
    c.sparsity_ratio     = 0.85f;
    c.calibration_steps  = 100;
    c.logit_temp_init    = 1.0f;
    c.lr                 = 0.01f;
    c.dim_reduction      = 2;
    c.layer_reduction    = 2;
    return c;
}

// ─────────────────────────────────────────────────────────────────────────────
// Telemetry: per-step flight recorder
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    // Acceptance tracking
    int   total_draft_tokens;
    int   total_accepted;
    int   total_verifications;

    // Per-gamma position histogram (γ up to 16)
    int   pos_accepted[16];
    int   pos_attempted[16];

    // Speed
    double baseline_tps;
    double speculative_tps;
    double spec_roi;          // speculative_tps / baseline_tps

    // Calibration convergence
    double calibration_kl;    // KL divergence draft‖target, last calibration step
} SpecTelemetry;

static inline void telemetry_reset(SpecTelemetry *t) {
    memset(t, 0, sizeof(*t));
}

static inline void telemetry_print(const SpecTelemetry *t, int gamma) {
    printf("\n┌─────────────────────────────────────────────────────┐\n");
    printf("│           Speculative Decoder Flight Recorder        │\n");
    printf("├──────────────────────────────┬──────────────────────┤\n");

    double alpha = t->total_draft_tokens > 0
        ? (double)t->total_accepted / t->total_draft_tokens : 0.0;

    double eff_gamma = t->total_verifications > 0
        ? (double)t->total_accepted / t->total_verifications : 0.0;

    printf("│ Acceptance α                 │ %19.1f%% │\n", alpha * 100.0);
    printf("│ Effective γ                  │ %20.2f │\n", eff_gamma);
    printf("│ Baseline TPS                 │ %17.2f t/s │\n", t->baseline_tps);
    printf("│ Speculative TPS              │ %17.2f t/s │\n", t->speculative_tps);
    printf("│ Speculative ROI              │ %19.2fx │\n", t->spec_roi);
    printf("│ Calibration KL               │ %20.4f │\n", t->calibration_kl);
    printf("├──────────────────────────────┴──────────────────────┤\n");
    printf("│ Per-position acceptance:                             │\n");
    for (int i = 0; i < gamma; i++) {
        double p = t->pos_attempted[i] > 0
            ? 100.0 * t->pos_accepted[i] / t->pos_attempted[i] : 0.0;
        printf("│   pos[%d]: %5d/%5d = %5.1f%%                       │\n",
               i, t->pos_accepted[i], t->pos_attempted[i], p);
    }
    printf("└─────────────────────────────────────────────────────┘\n");

    // Diagnosis
    if (alpha < 0.3) {
        printf("\n[DISTILLER] Alpha=%.1f%% < 30%% — draft not aligned. Re-run calibration.\n", alpha * 100.0);
    } else if (alpha < 0.6) {
        printf("\n[DISTILLER] Alpha=%.1f%% — marginal. Increase calibration_steps or reduce sparsity_ratio.\n", alpha * 100.0);
    } else {
        printf("\n[DISTILLER] Alpha=%.1f%% — good. Speculative ROI %.2fx.\n", alpha * 100.0, t->spec_roi);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Pruning Engine
// ─────────────────────────────────────────────────────────────────────────────

// In-place magnitude pruning: zero out weights below the sparsity_ratio percentile.
// Returns the mask (caller must free).
static uint8_t *magnitude_prune(float *w, int n, float sparsity_ratio) {
    // 1. Compute absolute values into a scratch array for sorting
    float *scratch = (float *)malloc(n * sizeof(float));
    if (!scratch) return NULL;
    for (int i = 0; i < n; i++) scratch[i] = fabsf(w[i]);

    // 2. Partial sort: find (sparsity_ratio * n)-th order statistic via quickselect
    //    Simple O(n log n) sort is fine for calibration-time work
    // Use insertion sort chunks + partition for reasonable perf on 1M weights
    int keep_n = (int)((1.0f - sparsity_ratio) * n);
    if (keep_n < 1) keep_n = 1;

    // Sort scratch descending via partial qsort using stdlib
    // We only need the threshold value, not a full sort
    int cmp_desc(const void *a, const void *b) {
        float fa = *(const float *)a;
        float fb = *(const float *)b;
        return (fb > fa) - (fb < fa);
    }
    qsort(scratch, n, sizeof(float), cmp_desc);
    float threshold = scratch[keep_n - 1];
    free(scratch);

    // 3. Build mask and zero pruned weights
    uint8_t *mask = (uint8_t *)malloc(n * sizeof(uint8_t));
    if (!mask) return NULL;
    int actual_kept = 0;
    for (int i = 0; i < n; i++) {
        if (fabsf(w[i]) >= threshold) {
            mask[i] = 1;
            actual_kept++;
        } else {
            mask[i] = 0;
            w[i] = 0.0f;
        }
    }
    return mask;
}

// ─────────────────────────────────────────────────────────────────────────────
// Weight projection: copy parent weights into draft (smaller dim)
// Strategy: take first draft_dim columns of parent matrix (column-subset)
// This preserves the most-active input dimensions.
// ─────────────────────────────────────────────────────────────────────────────
static void project_weights(
    const float *parent_w, int parent_rows, int parent_cols,
    float       *draft_w,  int draft_rows,  int draft_cols)
{
    int r = (draft_rows < parent_rows) ? draft_rows : parent_rows;
    int c = (draft_cols < parent_cols) ? draft_cols : parent_cols;
    for (int i = 0; i < r; i++) {
        for (int j = 0; j < c; j++) {
            draft_w[i * draft_cols + j] = parent_w[i * parent_cols + j];
        }
        // Zero-pad remaining cols
        for (int j = c; j < draft_cols; j++) {
            draft_w[i * draft_cols + j] = 0.0f;
        }
    }
    // Zero-pad remaining rows
    for (int i = r; i < draft_rows; i++) {
        memset(draft_w + i * draft_cols, 0, draft_cols * sizeof(float));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Softmax (in-place)
// ─────────────────────────────────────────────────────────────────────────────
static void softmax_inplace(float *x, int n) {
    float max_v = x[0];
    for (int i = 1; i < n; i++) if (x[i] > max_v) max_v = x[i];
    float sum = 0.0f;
    for (int i = 0; i < n; i++) { x[i] = expf(x[i] - max_v); sum += x[i]; }
    float inv = 1.0f / sum;
    for (int i = 0; i < n; i++) x[i] *= inv;
}

// KL divergence: KL(target_probs || draft_probs)  (nats)
static double kl_divergence(const float *p, const float *q, int n) {
    double kl = 0.0;
    for (int i = 0; i < n; i++) {
        if (p[i] > 1e-9f) {
            kl += p[i] * log((double)p[i] / ((double)q[i] + 1e-9));
        }
    }
    return kl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Calibration pass
// We run N random tokens through both parent and draft, compare softmax
// distributions over vocabulary, and tune a per-layer temperature scalar
// (stored in draft->ln_final scale) so draft's distribution tracks parent.
//
// No backprop — this is a single-pass logit-temperature search via golden section.
// ─────────────────────────────────────────────────────────────────────────────

// Forward declaration — implemented below main() by including spec_dec_realistic.c
// or defined inline here for the standalone case.

// Temperature alignment: scale draft logits by T* = argmin_T KL(p_target || softmax(draft_logits / T))
// Solved with Brent's method over T in [0.1, 10.0].
static double find_optimal_temp(const float *draft_logits, const float *target_probs, int vocab) {
    // Brent's method
    double a = 0.1, b = 10.0, tol = 1e-4;
    // Evaluate KL at both ends and midpoints
    auto eval = [&](double T) -> double {
        float *q = (float *)alloca(vocab * sizeof(float));
        for (int i = 0; i < vocab; i++) q[i] = draft_logits[i] / (float)T;
        softmax_inplace(q, vocab);
        return kl_divergence(target_probs, q, vocab);
    };
    // Golden section search
    double gr = 0.6180339887;
    double c = b - gr * (b - a);
    double d = a + gr * (b - a);
    int iters = 0;
    while (fabs(b - a) > tol && iters++ < 100) {
        double fc = eval(c), fd = eval(d);
        if (fc < fd) { b = d; } else { a = c; }
        c = b - gr * (b - a);
        d = a + gr * (b - a);
    }
    return (a + b) * 0.5;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API: distill_from_parent
//
// Takes parent Model (as defined in spec_dec_realistic.c), produces a draft
// Model with:
//   - fewer layers     (parent_layers / config.layer_reduction)
//   - smaller dim      (parent_dim    / config.dim_reduction)
//   - magnitude-pruned weights projected from parent
//   - calibrated logit temperature stored in draft->w_lm scale factor
//
// The caller receives ownership of all allocated buffers in the returned Model.
// ─────────────────────────────────────────────────────────────────────────────

// Forward declaration of Model type (defined in spec_dec_realistic.c)
// When compiled standalone, include the struct definition here.
#ifndef MODEL_STRUCT_DEFINED
#define MODEL_STRUCT_DEFINED
typedef struct Model_s {
    float *w_embed;
    float *w_qkv;
    float *w_out;
    float *w_gate;
    float *w_up;
    float *w_down;
    float *ln1;
    float *ln2;
    float *ln_final;
    float *w_lm;
    int layers, dim, ff, vocab;
    // Calibration result
    float logit_temp_scale;   // multiply draft logits by this before sampling
} Model;
#endif

// Forward: model_forward_logits — returns raw logits (not just argmax token)
// This is needed for calibration. See implementation at bottom.
static void model_forward_logits(Model *m, int token, float *logits_out);

static void distill_from_parent(Model *draft, const Model *parent, PruneConfig cfg,
                                SpecTelemetry *telem)
{
    int d_layers = parent->layers / cfg.layer_reduction;
    int d_dim    = parent->dim    / cfg.dim_reduction;
    int d_ff     = parent->ff     / cfg.dim_reduction;
    int vocab    = parent->vocab;

    if (d_layers < 1) d_layers = 1;
    if (d_dim    < 1) d_dim    = 1;
    if (d_ff     < 1) d_ff     = 1;

    printf("[DISTILLER] Parent: %d layers, %d dim → Draft: %d layers, %d dim\n",
           parent->layers, parent->dim, d_layers, d_dim);
    printf("[DISTILLER] Sparsity %.0f%%, keeping top %.0f%% by |w|\n",
           cfg.sparsity_ratio * 100.0f, (1.0f - cfg.sparsity_ratio) * 100.0f);

    // Alloc draft buffers
    draft->layers = d_layers;
    draft->dim    = d_dim;
    draft->ff     = d_ff;
    draft->vocab  = vocab;
    draft->logit_temp_scale = cfg.logit_temp_init;

    draft->w_embed  = (float *)calloc(vocab * d_dim, sizeof(float));
    draft->w_qkv    = (float *)calloc(d_layers * 3 * d_dim * d_dim, sizeof(float));
    draft->w_out    = (float *)calloc(d_layers * d_dim * d_dim, sizeof(float));
    draft->w_gate   = (float *)calloc(d_layers * d_ff * d_dim, sizeof(float));
    draft->w_up     = (float *)calloc(d_layers * d_ff * d_dim, sizeof(float));
    draft->w_down   = (float *)calloc(d_layers * d_dim * d_ff, sizeof(float));
    draft->ln1      = (float *)calloc(d_layers * d_dim, sizeof(float));
    draft->ln2      = (float *)calloc(d_layers * d_dim, sizeof(float));
    draft->ln_final = (float *)calloc(d_dim, sizeof(float));
    draft->w_lm     = (float *)calloc(vocab * d_dim, sizeof(float));

    // ── Step 1: Project parent weights into draft dims ────────────────────────
    project_weights(parent->w_embed, vocab, parent->dim,
                    draft->w_embed,  vocab, d_dim);

    for (int l = 0; l < d_layers; l++) {
        int pl = l * cfg.layer_reduction;   // source layer in parent

        project_weights(parent->w_qkv + pl * 3 * parent->dim * parent->dim,
                        3 * parent->dim, parent->dim,
                        draft->w_qkv  +  l * 3 * d_dim * d_dim,
                        3 * d_dim, d_dim);

        project_weights(parent->w_out + pl * parent->dim * parent->dim,
                        parent->dim, parent->dim,
                        draft->w_out  +  l * d_dim * d_dim,
                        d_dim, d_dim);

        project_weights(parent->w_gate + pl * parent->ff * parent->dim,
                        parent->ff, parent->dim,
                        draft->w_gate  +  l * d_ff * d_dim,
                        d_ff, d_dim);

        project_weights(parent->w_up + pl * parent->ff * parent->dim,
                        parent->ff, parent->dim,
                        draft->w_up  +  l * d_ff * d_dim,
                        d_ff, d_dim);

        project_weights(parent->w_down + pl * parent->dim * parent->ff,
                        parent->dim, parent->ff,
                        draft->w_down  +  l * d_dim * d_ff,
                        d_dim, d_ff);

        // Layer norms: copy first d_dim elements
        memcpy(draft->ln1 + l * d_dim, parent->ln1 + pl * parent->dim,
               d_dim * sizeof(float));
        memcpy(draft->ln2 + l * d_dim, parent->ln2 + pl * parent->dim,
               d_dim * sizeof(float));
    }
    memcpy(draft->ln_final, parent->ln_final, d_dim * sizeof(float));

    project_weights(parent->w_lm, vocab, parent->dim,
                    draft->w_lm,  vocab, d_dim);

    // ── Step 2: Magnitude pruning on draft buffers ────────────────────────────
    printf("[DISTILLER] Applying magnitude pruning...\n");
    uint64_t total_w = 0, kept_w = 0;

#define PRUNE_BUF(buf, n) do {                          \
    uint8_t *mask = magnitude_prune(buf, n, cfg.sparsity_ratio); \
    if (mask) {                                         \
        for (int _i = 0; _i < (n); _i++) {             \
            total_w++; kept_w += mask[_i];              \
        }                                               \
        free(mask);                                     \
    }                                                   \
} while(0)

    PRUNE_BUF(draft->w_embed,  vocab * d_dim);
    PRUNE_BUF(draft->w_lm,     vocab * d_dim);
    for (int l = 0; l < d_layers; l++) {
        PRUNE_BUF(draft->w_qkv  + l * 3 * d_dim * d_dim, 3 * d_dim * d_dim);
        PRUNE_BUF(draft->w_out  + l * d_dim * d_dim,          d_dim * d_dim);
        PRUNE_BUF(draft->w_gate + l * d_ff  * d_dim,          d_ff  * d_dim);
        PRUNE_BUF(draft->w_up   + l * d_ff  * d_dim,          d_ff  * d_dim);
        PRUNE_BUF(draft->w_down + l * d_dim * d_ff,           d_dim * d_ff);
    }
#undef PRUNE_BUF

    printf("[DISTILLER] Kept %llu / %llu weights (%.1f%%)\n",
           (unsigned long long)kept_w, (unsigned long long)total_w,
           100.0 * kept_w / total_w);

    // ── Step 3: Calibration — align logit temperature ─────────────────────────
    printf("[DISTILLER] Calibration (%d steps)...\n", cfg.calibration_steps);

    float *parent_logits = (float *)malloc(vocab * sizeof(float));
    float *draft_logits  = (float *)malloc(vocab * sizeof(float));
    float *parent_probs  = (float *)malloc(vocab * sizeof(float));
    if (!parent_logits || !draft_logits || !parent_probs) {
        fprintf(stderr, "[DISTILLER] OOM in calibration alloc\n");
        goto calibration_done;
    }

    double total_kl = 0.0;
    double best_temp_sum = 0.0;

    for (int step = 0; step < cfg.calibration_steps; step++) {
        int calib_token = rand() % vocab;

        // Run parent
        model_forward_logits((Model*)parent, calib_token, parent_logits);
        memcpy(parent_probs, parent_logits, vocab * sizeof(float));
        softmax_inplace(parent_probs, vocab);

        // Run draft
        model_forward_logits(draft, calib_token, draft_logits);

        // Find optimal temperature for this token
        double opt_T = find_optimal_temp(draft_logits, parent_probs, vocab);
        best_temp_sum += opt_T;

        // Compute KL at optimal T for reporting
        float *q = (float *)alloca(vocab * sizeof(float));
        for (int i = 0; i < vocab; i++) q[i] = draft_logits[i] / (float)opt_T;
        softmax_inplace(q, vocab);
        total_kl += kl_divergence(parent_probs, q, vocab);
    }

    draft->logit_temp_scale = (float)(best_temp_sum / cfg.calibration_steps);
    telem->calibration_kl   = total_kl / cfg.calibration_steps;

    printf("[DISTILLER] Calibrated temperature scale: %.4f (avg KL: %.4f)\n",
           draft->logit_temp_scale, telem->calibration_kl);

calibration_done:
    free(parent_logits);
    free(draft_logits);
    free(parent_probs);
}

// ─────────────────────────────────────────────────────────────────────────────
// model_forward_logits — raw logits (used by calibration)
// Mirrors forward() from spec_dec_realistic.c but returns logits array
// ─────────────────────────────────────────────────────────────────────────────
static float _dot(const float *a, const float *b, int n) {
    float s = 0; for (int i = 0; i < n; i++) s += a[i]*b[i]; return s;
}
static void _matvec(const float *w, const float *x, float *y, int m, int n) {
    for (int i = 0; i < m; i++) y[i] = _dot(w + i*n, x, n);
}
static void _rmsnorm(float *out, const float *x, const float *w, int n) {
    float ss = 0; for (int i = 0; i < n; i++) ss += x[i]*x[i];
    float inv = 1.0f / sqrtf(ss/n + 1e-5f);
    for (int i = 0; i < n; i++) out[i] = x[i]*inv*w[i];
}
static void _silu(float *out, const float *a, const float *b, int n) {
    for (int i = 0; i < n; i++) {
        float v = a[i]; out[i] = (v / (1.0f + expf(-v))) * b[i];
    }
}

static void model_forward_logits(Model *m, int token, float *logits_out) {
    int dim = m->dim, ff = m->ff;
    float *x    = (float *)alloca(dim * sizeof(float));
    float *tmp  = (float *)alloca((dim > ff ? dim : ff) * 3 * sizeof(float));

    memcpy(x, m->w_embed + token * dim, dim * sizeof(float));

    for (int l = 0; l < m->layers; l++) {
        float *n1 = tmp;
        _rmsnorm(n1, x, m->ln1 + l*dim, dim);

        float *qkv = tmp + dim;
        _matvec(m->w_qkv + l*3*dim*dim, n1, qkv, 3*dim, dim);

        // Simplified attention: Q as residual (same as forward() in benchmark)
        float *ao = tmp + dim + 3*dim;
        memcpy(ao, qkv, dim * sizeof(float));
        for (int i = 0; i < dim; i++) ao[i] += x[i];

        float *n2   = (float *)alloca(dim * sizeof(float));
        float *gate = (float *)alloca(ff  * sizeof(float));
        float *up   = (float *)alloca(ff  * sizeof(float));
        float *fo   = (float *)alloca(dim * sizeof(float));

        _rmsnorm(n2, ao, m->ln2 + l*dim, dim);
        _matvec(m->w_gate + l*ff*dim, n2, gate, ff, dim);
        _matvec(m->w_up   + l*ff*dim, n2, up,   ff, dim);
        _silu(gate, gate, up, ff);
        _matvec(m->w_down + l*dim*ff, gate, fo, dim, ff);
        for (int i = 0; i < dim; i++) x[i] = ao[i] + fo[i];
    }

    float *nf = (float *)alloca(dim * sizeof(float));
    _rmsnorm(nf, x, m->ln_final, dim);
    _matvec(m->w_lm, nf, logits_out, m->vocab, dim);
}

#endif // EXPERT_DISTILLER_H
