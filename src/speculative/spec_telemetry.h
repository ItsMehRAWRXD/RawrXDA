// spec_telemetry.h
// Telemetry Suite for Speculative Decoding - "Flight Recorder" Dashboard
// Tracks acceptance rate, effective gamma, speculative ROI, and per-expert metrics
//
// Integration: Include this header and call telemetry_record_frame() after each
// speculative decode step. Use telemetry_print_dashboard() for console output.
//
// Compile: gcc -O2 -o spec_dec spec_dec.c -lm (includes this header)

#ifndef SPEC_TELEMETRY_H
#define SPEC_TELEMETRY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
static inline double telemetry_time_ms(void) {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / f.QuadPart;
}
#else
#include <time.h>
static inline double telemetry_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// DiagnosticFrame: Single-step telemetry snapshot
// ═══════════════════════════════════════════════════════════════════════════════

typedef struct {
    // Acceptance metrics
    float acceptance_rate;      // α: percentage of draft tokens kept (0.0 - 1.0)
    int   tokens_produced;      // Total tokens in this burst
    int   draft_tokens;         // Number of draft tokens generated
    int   accepted_tokens;      // Number accepted by target
    
    // Timing metrics
    double draft_latency_ms;    // Time spent on draft model
    double verify_latency_ms;   // Time spent on target verification
    double total_ms;            // End-to-end time for this step
    
    // Effective utilization
    float effective_gamma;      // Actual tokens gained per verification
    float speculative_roi;      // (spec_tps / baseline_tps) - 1.0
    
    // Expert tracking (for MoE integration)
    int   expert_id;            // Which expert was used (-1 if none)
    float expert_confidence;    // Expert's confidence score
    
    // Position tracking
    int   position;             // Token position in sequence
    int   step_id;              // Step counter
    
    // Verification details
    int   verifications;        // Number of target forward passes
    int   fallbacks;           // Number of rejected drafts (fallback to target)
} DiagnosticFrame;

// ═══════════════════════════════════════════════════════════════════════════════
// TelemetryDashboard: Aggregated metrics and history
// ═══════════════════════════════════════════════════════════════════════════════

#define TELEMETRY_MAX_FRAMES 4096
#define TELEMETRY_MAX_EXPERTS 32

typedef struct {
    // Frame history (ring buffer)
    DiagnosticFrame frames[TELEMETRY_MAX_FRAMES];
    int frame_count;
    int frame_head;
    
    // Aggregated statistics
    double total_draft_ms;
    double total_verify_ms;
    double total_ms;
    int total_tokens;
    int total_draft_tokens;
    int total_accepted_tokens;
    int total_verifications;
    int total_fallbacks;
    
    // Per-expert tracking
    int expert_trials[TELEMETRY_MAX_EXPERTS];
    int expert_acceptances[TELEMETRY_MAX_EXPERTS];
    double expert_total_ms[TELEMETRY_MAX_EXPERTS];
    float expert_ema[TELEMETRY_MAX_EXPERTS];  // Exponential moving average
    
    // Baseline metrics (for ROI calculation)
    double baseline_tps;        // Target-only throughput
    double draft_tps;           // Draft-only throughput
    
    // Real-time metrics
    float current_acceptance_rate;
    float current_effective_gamma;
    float current_roi;
    
    // Performance tracking
    double best_acceptance_rate;
    double worst_acceptance_rate;
    int best_step;
    int worst_step;
    
    // Histogram data
    int acceptance_histogram[10];  // Buckets: 0-10%, 10-20%, ..., 90-100%
    
} TelemetryDashboard;

// Global dashboard instance
static TelemetryDashboard g_telemetry = {0};

// ═══════════════════════════════════════════════════════════════════════════════
// Initialization
// ═══════════════════════════════════════════════════════════════════════════════

static inline void telemetry_init(double baseline_tps, double draft_tps) {
    memset(&g_telemetry, 0, sizeof(TelemetryDashboard));
    g_telemetry.baseline_tps = baseline_tps;
    g_telemetry.draft_tps = draft_tps;
    g_telemetry.best_acceptance_rate = -1.0;
    g_telemetry.worst_acceptance_rate = 2.0;
    
    // Initialize EMA to 0.5 (neutral)
    for (int i = 0; i < TELEMETRY_MAX_EXPERTS; i++) {
        g_telemetry.expert_ema[i] = 0.5f;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Frame Recording
// ═══════════════════════════════════════════════════════════════════════════════

static inline void telemetry_record_frame(
    int draft_tokens,
    int accepted_tokens,
    int tokens_produced,
    double draft_ms,
    double verify_ms,
    int expert_id,
    int position
) {
    TelemetryDashboard *d = &g_telemetry;
    
    // Calculate frame metrics
    DiagnosticFrame frame = {0};
    frame.draft_tokens = draft_tokens;
    frame.accepted_tokens = accepted_tokens;
    frame.tokens_produced = tokens_produced;
    frame.draft_latency_ms = draft_ms;
    frame.verify_latency_ms = verify_ms;
    frame.total_ms = draft_ms + verify_ms;
    frame.expert_id = expert_id;
    frame.position = position;
    frame.step_id = d->frame_count;
    frame.verifications = 1;
    frame.fallbacks = (accepted_tokens < draft_tokens) ? 1 : 0;
    
    // Acceptance rate
    frame.acceptance_rate = (draft_tokens > 0) 
        ? (float)accepted_tokens / (float)draft_tokens 
        : 0.0f;
    
    // Effective gamma: tokens gained per verification
    // Formula: γ_eff = (accepted + fallback) / verifications
    frame.effective_gamma = (float)tokens_produced;
    
    // Speculative ROI: (spec_tps / baseline_tps) - 1.0
    // Positive = speedup, Negative = regression (the "tax")
    double spec_tps = (tokens_produced > 0 && frame.total_ms > 0)
        ? (double)tokens_produced * 1000.0 / frame.total_ms
        : 0.0;
    frame.speculative_roi = (d->baseline_tps > 0)
        ? (float)(spec_tps / d->baseline_tps)
        : 0.0f;
    
    // Add to ring buffer
    d->frames[d->frame_head] = frame;
    d->frame_head = (d->frame_head + 1) % TELEMETRY_MAX_FRAMES;
    if (d->frame_count < TELEMETRY_MAX_FRAMES) d->frame_count++;
    
    // Update aggregates
    d->total_draft_ms += draft_ms;
    d->total_verify_ms += verify_ms;
    d->total_ms += frame.total_ms;
    d->total_tokens += tokens_produced;
    d->total_draft_tokens += draft_tokens;
    d->total_accepted_tokens += accepted_tokens;
    d->total_verifications += 1;
    d->total_fallbacks += frame.fallbacks;
    
    // Update per-expert tracking
    if (expert_id >= 0 && expert_id < TELEMETRY_MAX_EXPERTS) {
        d->expert_trials[expert_id]++;
        d->expert_acceptances[expert_id] += accepted_tokens;
        d->expert_total_ms[expert_id] += frame.total_ms;
        
        // EMA update: new_ema = 0.9 * old_ema + 0.1 * new_value
        d->expert_ema[expert_id] = 0.9f * d->expert_ema[expert_id] 
            + 0.1f * frame.acceptance_rate;
    }
    
    // Update best/worst
    if (frame.acceptance_rate > d->best_acceptance_rate) {
        d->best_acceptance_rate = frame.acceptance_rate;
        d->best_step = frame.step_id;
    }
    if (frame.acceptance_rate < d->worst_acceptance_rate) {
        d->worst_acceptance_rate = frame.acceptance_rate;
        d->worst_step = frame.step_id;
    }
    
    // Update histogram
    int bucket = (int)(frame.acceptance_rate * 10.0f);
    if (bucket >= 0 && bucket < 10) {
        d->acceptance_histogram[bucket]++;
    }
    
    // Update real-time metrics
    d->current_acceptance_rate = frame.acceptance_rate;
    d->current_effective_gamma = frame.effective_gamma;
    d->current_roi = frame.speculative_roi;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Dashboard Output
// ═══════════════════════════════════════════════════════════════════════════════

static inline void telemetry_print_dashboard(void) {
    TelemetryDashboard *d = &g_telemetry;
    
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║              SPECULATIVE DECODING TELEMETRY DASHBOARD           ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    
    // Overall metrics
    printf("\n┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ OVERALL PERFORMANCE                                              │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    
    double avg_acceptance = (d->total_draft_tokens > 0)
        ? 100.0 * (double)d->total_accepted_tokens / (double)d->total_draft_tokens
        : 0.0;
    
    double avg_spec_tps = (d->total_tokens > 0 && d->total_ms > 0)
        ? (double)d->total_tokens * 1000.0 / d->total_ms
        : 0.0;
    
    double avg_roi = (d->baseline_tps > 0)
        ? avg_spec_tps / d->baseline_tps
        : 0.0;
    
    printf("│ Total tokens generated:     %8d                          │\n", d->total_tokens);
    printf("│ Total draft tokens:          %8d                          │\n", d->total_draft_tokens);
    printf("│ Total accepted:              %8d (%.1f%%)                   │\n", 
           d->total_accepted_tokens, avg_acceptance);
    printf("│ Total verifications:         %8d                          │\n", d->total_verifications);
    printf("│ Total fallbacks:            %8d                          │\n", d->total_fallbacks);
    printf("│                                                              │\n");
    printf("│ Baseline TPS:               %8.2f                          │\n", d->baseline_tps);
    printf("│ Draft TPS:                  %8.2f                          │\n", d->draft_tps);
    printf("│ Speculative TPS:           %8.2f                          │\n", avg_spec_tps);
    printf("│ Speedup (ROI):             %8.2fx                         │\n", avg_roi);
    printf("└─────────────────────────────────────────────────────────────────┘\n");
    
    // Acceptance analysis
    printf("\n┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ ACCEPTANCE ANALYSIS                                              │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    printf("│ Average acceptance:          %5.1f%%                         │\n", avg_acceptance);
    printf("│ Best acceptance:             %5.1f%% (step %d)                 │\n", 
           d->best_acceptance_rate * 100.0, d->best_step);
    printf("│ Worst acceptance:           %5.1f%% (step %d)                 │\n", 
           d->worst_acceptance_rate * 100.0, d->worst_step);
    printf("│                                                              │\n");
    printf("│ Acceptance Distribution:                                       │\n");
    printf("│   0-10%%:  %4d    10-20%%: %4d    20-30%%: %4d    30-40%%: %4d   │\n",
           d->acceptance_histogram[0], d->acceptance_histogram[1],
           d->acceptance_histogram[2], d->acceptance_histogram[3]);
    printf("│   40-50%%: %4d    50-60%%: %4d    60-70%%: %4d    70-80%%: %4d   │\n",
           d->acceptance_histogram[4], d->acceptance_histogram[5],
           d->acceptance_histogram[6], d->acceptance_histogram[7]);
    printf("│   80-90%%: %4d    90-100%%: %4d                               │\n",
           d->acceptance_histogram[8], d->acceptance_histogram[9]);
    printf("└─────────────────────────────────────────────────────────────────┘\n");
    
    // Timing breakdown
    printf("\n┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ TIMING BREAKDOWN                                                 │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    double draft_pct = (d->total_ms > 0) ? 100.0 * d->total_draft_ms / d->total_ms : 0.0;
    double verify_pct = (d->total_ms > 0) ? 100.0 * d->total_verify_ms / d->total_ms : 0.0;
    printf("│ Total time:          %8.2f ms                              │\n", d->total_ms);
    printf("│ Draft time:          %8.2f ms (%.1f%%)                       │\n", 
           d->total_draft_ms, draft_pct);
    printf("│ Verify time:         %8.2f ms (%.1f%%)                       │\n", 
           d->total_verify_ms, verify_pct);
    printf("│ Avg draft per step:  %8.2f ms                              │\n",
           d->total_verifications > 0 ? d->total_draft_ms / d->total_verifications : 0.0);
    printf("│ Avg verify per step: %8.2f ms                              │\n",
           d->total_verifications > 0 ? d->total_verify_ms / d->total_verifications : 0.0);
    printf("└─────────────────────────────────────────────────────────────────┘\n");
    
    // Expert tracking (if any)
    int experts_used = 0;
    for (int i = 0; i < TELEMETRY_MAX_EXPERTS; i++) {
        if (d->expert_trials[i] > 0) experts_used++;
    }
    
    if (experts_used > 0) {
        printf("\n┌─────────────────────────────────────────────────────────────────┐\n");
        printf("│ EXPERT PERFORMANCE (MoE Routing)                                │\n");
        printf("├─────────────────────────────────────────────────────────────────┤\n");
        printf("│ Expert │ Trials │ Accept │ Rate    │ EMA     │ Avg ms         │\n");
        printf("│────────┼────────┼────────┼─────────┼─────────┼────────────────│\n");
        
        for (int i = 0; i < TELEMETRY_MAX_EXPERTS; i++) {
            if (d->expert_trials[i] > 0) {
                double rate = 100.0 * (double)d->expert_acceptances[i] / 
                              (double)(d->expert_trials[i] * 4);  // Assuming gamma=4
                double avg_ms = d->expert_total_ms[i] / d->expert_trials[i];
                printf("│ %6d │ %6d │ %6d │ %6.1f%% │ %6.3f  │ %8.2f       │\n",
                       i, d->expert_trials[i], d->expert_acceptances[i],
                       rate, d->expert_ema[i], avg_ms);
            }
        }
        printf("└─────────────────────────────────────────────────────────────────┘\n");
    }
    
    // ROI Analysis
    printf("\n┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ SPECULATIVE ROI ANALYSIS                                         │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    
    if (avg_roi < 1.0) {
        printf("│ Status: REGRESSION (Speculative Tax)                           │\n");
        printf("│         You are LOSING %.1f%% throughput due to low acceptance.  │\n", 
               (1.0 - avg_roi) * 100.0);
        printf("│                                                              │\n");
        printf("│ Recommendation:                                                │\n");
        printf("│   - Draft model must be distilled from target                  │\n");
        printf("│   - Target acceptance rate: 60-80%% required for speedup        │\n");
        printf("│   - Current acceptance: %.1f%% (need +%.1f%%)                   │\n",
               avg_acceptance, 60.0 - avg_acceptance);
    } else {
        printf("│ Status: SPEEDUP (Speculative Dividend)                         │\n");
        printf("│         You are GAINING %.1fx throughput from speculation.      │\n", avg_roi);
        printf("│                                                              │\n");
        printf("│ Performance:                                                   │\n");
        printf("│   - Acceptance rate: %.1f%% (healthy)                          │\n", avg_acceptance);
        printf("│   - Effective gamma: %.1f tokens per verification              │\n",
               (double)d->total_tokens / d->total_verifications);
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n");
    
    // Theoretical speedup calculation
    printf("\n┌─────────────────────────────────────────────────────────────────┐\n");
    printf("│ THEORETICAL SPEEDUP (if acceptance improves)                    │\n");
    printf("├─────────────────────────────────────────────────────────────────┤\n");
    
    // Speedup formula: S = (1 + α * γ) / (1 + γ * β)
    // where α = acceptance, γ = gamma, β = draft_speed_ratio
    double gamma = 4.0;  // Default
    double beta = d->baseline_tps / d->draft_tps;  // Target/draft speed ratio
    
    printf("│ Acceptance │ Speedup │ Notes                                  │\n");
    printf("│────────────┼─────────┼────────────────────────────────────────│\n");
    
    for (double target_alpha = 0.0; target_alpha <= 1.0; target_alpha += 0.1) {
        // Simplified speedup: S ≈ (1 + α * γ) / (1 + γ * β)
        double speedup = (1.0 + target_alpha * gamma) / (1.0 + gamma * beta);
        const char *note = "";
        if (target_alpha < 0.3) note = "Regression";
        else if (target_alpha < 0.5) note = "Marginal";
        else if (target_alpha < 0.7) note = "Good";
        else note = "Excellent";
        
        printf("│   %3.0f%%     │  %5.2fx  │ %s                               │\n",
               target_alpha * 100.0, speedup, note);
    }
    printf("└─────────────────────────────────────────────────────────────────┘\n");
}

// ═══════════════════════════════════════════════════════════════════════════════
// JSON Export (for IDE integration)
// ═══════════════════════════════════════════════════════════════════════════════

static inline void telemetry_export_json(const char *filename) {
    TelemetryDashboard *d = &g_telemetry;
    FILE *f = fopen(filename, "w");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }
    
    fprintf(f, "{\n");
    fprintf(f, "  \"total_tokens\": %d,\n", d->total_tokens);
    fprintf(f, "  \"total_draft_tokens\": %d,\n", d->total_draft_tokens);
    fprintf(f, "  \"total_accepted_tokens\": %d,\n", d->total_accepted_tokens);
    fprintf(f, "  \"acceptance_rate\": %.4f,\n",
            d->total_draft_tokens > 0 ? (double)d->total_accepted_tokens / d->total_draft_tokens : 0.0);
    fprintf(f, "  \"baseline_tps\": %.2f,\n", d->baseline_tps);
    fprintf(f, "  \"draft_tps\": %.2f,\n", d->draft_tps);
    fprintf(f, "  \"speculative_tps\": %.2f,\n",
            d->total_ms > 0 ? (double)d->total_tokens * 1000.0 / d->total_ms : 0.0);
    fprintf(f, "  \"total_ms\": %.2f,\n", d->total_ms);
    fprintf(f, "  \"draft_ms\": %.2f,\n", d->total_draft_ms);
    fprintf(f, "  \"verify_ms\": %.2f,\n", d->total_verify_ms);
    fprintf(f, "  \"verifications\": %d,\n", d->total_verifications);
    fprintf(f, "  \"fallbacks\": %d,\n", d->total_fallbacks);
    fprintf(f, "  \"frames\": [\n");
    
    for (int i = 0; i < d->frame_count && i < TELEMETRY_MAX_FRAMES; i++) {
        DiagnosticFrame *frame = &d->frames[i];
        fprintf(f, "    {\"step\": %d, \"pos\": %d, \"acceptance\": %.3f, \"roi\": %.3f, \"draft_ms\": %.2f, \"verify_ms\": %.2f}%s\n",
                frame->step_id, frame->position, frame->acceptance_rate, frame->speculative_roi,
                frame->draft_latency_ms, frame->verify_latency_ms,
                (i < d->frame_count - 1) ? "," : "");
    }
    
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);
    
    printf("Telemetry exported to %s\n", filename);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Real-time frame accessor (for IDE overlay)
// ═══════════════════════════════════════════════════════════════════════════════

static inline DiagnosticFrame* telemetry_get_latest_frame(void) {
    if (g_telemetry.frame_count == 0) return NULL;
    int idx = (g_telemetry.frame_head - 1 + TELEMETRY_MAX_FRAMES) % TELEMETRY_MAX_FRAMES;
    return &g_telemetry.frames[idx];
}

static inline float telemetry_get_acceptance_rate(void) {
    return g_telemetry.current_acceptance_rate;
}

static inline float telemetry_get_roi(void) {
    return g_telemetry.current_roi;
}

static inline double telemetry_get_avg_acceptance(void) {
    if (g_telemetry.total_draft_tokens == 0) return 0.0;
    return (double)g_telemetry.total_accepted_tokens / (double)g_telemetry.total_draft_tokens;
}

#endif // SPEC_TELEMETRY_H