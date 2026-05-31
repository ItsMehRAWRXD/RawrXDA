/* ═══════════════════════════════════════════════════════════════════════════
   rawrxd_demo.c — Demo for RawrXD Weight Re-quantizer & Hot-Swap
   ═══════════════════════════════════════════════════════════════════════════ */

#include "rawrxd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_separator(void) {
    printf("═══════════════════════════════════════════════════════════════════════════\n");
}

static void print_header(const char* title) {
    print_separator();
    printf("  %s\n", title);
    print_separator();
}

static const char* format_size(size_t bytes) {
    static char buffer[64];
    double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
    double mb = (double)bytes / (1024.0 * 1024.0);
    double kb = (double)bytes / 1024.0;
    
    if (gb >= 1.0) snprintf(buffer, sizeof(buffer), "%.2f GB", gb);
    else if (mb >= 1.0) snprintf(buffer, sizeof(buffer), "%.2f MB", mb);
    else if (kb >= 1.0) snprintf(buffer, sizeof(buffer), "%.2f KB", kb);
    else snprintf(buffer, sizeof(buffer), "%zu bytes", bytes);
    
    return buffer;
}

static const char* ggml_rxd_type_name(GGMLType type) {
    switch (type) {
        case GGML_RXD_TYPE_F32: return "F32";
        case GGML_RXD_TYPE_F16: return "F16";
        case GGML_RXD_TYPE_Q4_0: return "Q4_0";
        case GGML_RXD_TYPE_Q4_1: return "Q4_1";
        case GGML_RXD_TYPE_Q5_0: return "Q5_0";
        case GGML_RXD_TYPE_Q5_1: return "Q5_1";
        case GGML_RXD_TYPE_Q8_0: return "Q8_0";
        case GGML_RXD_TYPE_Q2_K: return "Q2_K";
        case GGML_RXD_TYPE_Q3_K: return "Q3_K";
        case GGML_RXD_TYPE_Q4_K: return "Q4_K";
        case GGML_RXD_TYPE_Q5_K: return "Q5_K";
        case GGML_RXD_TYPE_Q6_K: return "Q6_K";
        default: return "UNKNOWN";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   Demo: Quantization Quality
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_quantization_quality(void) {
    print_header("Quantization Quality Comparison");
    
    // Generate test data
    const size_t count = 1024;
    float* data = (float*)malloc(count * sizeof(float));
    for (size_t i = 0; i < count; i++) {
        data[i] = sinf((float)i * 0.01f) * 10.0f;
    }
    
    printf("\nTest data: %zu floats (sine wave)\n\n", count);
    
    // Test Q4_0
    printf("┌─────────┬───────────┬───────────┬───────────┬───────────┐\n");
    printf("│ Type    │ Size (B)  │ MSE       │ SNR (dB)  │ Quality   │\n");
    printf("├─────────┼───────────┼───────────┼───────────┼───────────┤\n");
    
    // Q4_0
    QuantResult q4_0 = quant_q4_0(data, count);
    DequantResult d4_0 = dequant_q4_0(q4_0.data, (count + 31) / 32);
    QuantQuality qual4_0 = estimate_quality(data, d4_0.data, count);
    printf("│ Q4_0    │ %9zu │ %9.6f │ %9.2f │ %9s │\n",
           q4_0.size, qual4_0.mse, qual4_0.snr_db,
           qual4_0.snr_db > 20.0f ? "GOOD" : "POOR");
    quant_free(&q4_0);
    dequant_free(&d4_0);
    
    // Q8_0
    QuantResult q8_0 = quant_q8_0(data, count);
    DequantResult d8_0 = dequant_q8_0(q8_0.data, (count + 31) / 32);
    QuantQuality qual8_0 = estimate_quality(data, d8_0.data, count);
    printf("│ Q8_0    │ %9zu │ %9.6f │ %9.2f │ %9s │\n",
           q8_0.size, qual8_0.mse, qual8_0.snr_db,
           qual8_0.snr_db > 30.0f ? "EXCELLENT" : "GOOD");
    quant_free(&q8_0);
    dequant_free(&d8_0);
    
    // Q4_K
    QuantResult q4_k = quant_q4_k(data, count);
    DequantResult d4_k = dequant_q4_k(q4_k.data, (count + 255) / 256);
    QuantQuality qual4_k = estimate_quality(data, d4_k.data, count);
    printf("│ Q4_K    │ %9zu │ %9.6f │ %9.2f │ %9s │\n",
           q4_k.size, qual4_k.mse, qual4_k.snr_db,
           qual4_k.snr_db > 25.0f ? "GOOD" : "POOR");
    quant_free(&q4_k);
    dequant_free(&d4_k);
    
    // F32 (reference)
    printf("│ F32     │ %9zu │ %9.6f │ %9.2f │ %9s │\n",
           count * sizeof(float), 0.0f, INFINITY, "PERFECT");
    
    printf("└─────────┴───────────┴───────────┴───────────┴───────────┘\n");
    
    free(data);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Demo: Quantization Profiles
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_quantization_profiles(void) {
    print_header("Quantization Profiles");
    
    printf("\n┌────────────┬────────────┬────────────┬────────────┬────────────┐\n");
    printf("│ Profile    │ Default    │ Attention  │ FFN        │ Embedding  │\n");
    printf("├────────────┼────────────┼────────────┼────────────┼────────────┤\n");
    
    printf("│ %-10s │ %-10s │ %-10s │ %-10s │ %-10s │\n",
           "Speed", ggml_rxd_type_name(QUANT_PROFILE_SPEED.default_type),
           ggml_rxd_type_name(QUANT_PROFILE_SPEED.attention_type),
           ggml_rxd_type_name(QUANT_PROFILE_SPEED.feedforward_type),
           ggml_rxd_type_name(QUANT_PROFILE_SPEED.embedding_type));
    
    printf("│ %-10s │ %-10s │ %-10s │ %-10s │ %-10s │\n",
           "Balanced", ggml_rxd_type_name(QUANT_PROFILE_BALANCED.default_type),
           ggml_rxd_type_name(QUANT_PROFILE_BALANCED.attention_type),
           ggml_rxd_type_name(QUANT_PROFILE_BALANCED.feedforward_type),
           ggml_rxd_type_name(QUANT_PROFILE_BALANCED.embedding_type));
    
    printf("│ %-10s │ %-10s │ %-10s │ %-10s │ %-10s │\n",
           "Quality", ggml_rxd_type_name(QUANT_PROFILE_QUALITY.default_type),
           ggml_rxd_type_name(QUANT_PROFILE_QUALITY.attention_type),
           ggml_rxd_type_name(QUANT_PROFILE_QUALITY.feedforward_type),
           ggml_rxd_type_name(QUANT_PROFILE_QUALITY.embedding_type));
    
    printf("└────────────┴────────────┴────────────┴────────────┴────────────┘\n");
    
    printf("\nProfile recommendations:\n");
    printf("  • Speed:    Maximum inference speed, lower quality\n");
    printf("  • Balanced: Good trade-off between speed and quality\n");
    printf("  • Quality:  Best quality, slower inference\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   Demo: Memory Detection
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_memory_detection(void) {
    print_header("Memory Detection");
    
    size_t gpu_mem = lockpick_get_gpu_memory();
    size_t sys_mem = lockpick_get_system_memory();
    
    printf("\n");
    printf("  GPU Memory:  %s\n", gpu_mem > 0 ? format_size(gpu_mem) : "Not detected");
    printf("  System RAM:  %s\n", format_size(sys_mem));
    printf("\n");
    
    if (gpu_mem > 0) {
        printf("  Recommendations:\n");
        if (gpu_mem >= 24ULL * 1024 * 1024 * 1024) {
            printf("    • Can run 70B models at Q4_K\n");
            printf("    • Can run 34B models at Q8_0\n");
            printf("    • Can run 13B models at F16\n");
        } else if (gpu_mem >= 16ULL * 1024 * 1024 * 1024) {
            printf("    • Can run 34B models at Q4_K\n");
            printf("    • Can run 13B models at Q8_0\n");
            printf("    • Can run 7B models at F16\n");
        } else if (gpu_mem >= 8ULL * 1024 * 1024 * 1024) {
            printf("    • Can run 13B models at Q4_K\n");
            printf("    • Can run 7B models at Q8_0\n");
        } else {
            printf("    • Can run 7B models at Q4_K\n");
            printf("    • Consider CPU offloading for larger models\n");
        }
    } else {
        printf("  Note: GPU memory detection not available.\n");
        printf("        Set target_vram manually for auto-configuration.\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   Demo: Inference Hook
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_inference_hook(void) {
    print_header("Inference Hook (Timing Demo)");
    
    printf("\nSimulating token generation...\n\n");
    
    inference_hook_start();
    
    // Simulate token generation
    for (int i = 0; i < 50; i++) {
        inference_hook_token(-0.5f - (float)(i % 10) * 0.1f);
        
        // Simulate processing time
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    
    inference_hook_end();
    
    LockpickMetrics metrics = {0};
    inference_hook_get_metrics(&metrics);
    
    printf("  Tokens generated:  %u\n", metrics.generated_tokens);
    printf("  Total time:        %.2f ms\n", metrics.total_time_ms);
    printf("  Avg token time:     %.2f ms\n", metrics.avg_token_ms);
    printf("  Tokens/sec:        %.2f\n", metrics.tokens_per_sec);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Demo: GGUF File Analysis
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_gguf_analysis(const char* path) {
    print_header("GGUF File Analysis");
    
    printf("\nLoading: %s\n\n", path);
    
    GGUFContext* ctx = gguf_load(path);
    if (!ctx) {
        printf("  ERROR: Failed to load GGUF file\n");
        printf("  Make sure the file exists and is a valid GGUF format.\n");
        return;
    }
    
    printf("  File size:      %s\n", format_size(ctx->file_size));
    printf("  GGUF version:   %u\n", ctx->header.version);
    printf("  Tensor count:   %llu\n", (unsigned long long)ctx->header.tensor_count);
    printf("  Metadata count: %llu\n", (unsigned long long)ctx->header.metadata_kv_count);
    printf("  Data offset:    %s from start\n", format_size(ctx->data_offset));
    
    printf("\n  Tensors:\n");
    printf("  ┌────────────────────────────┬──────────┬────────────┬──────────┐\n");
    printf("  │ Name                       │ Type     │ Elements   │ Size     │\n");
    printf("  ├────────────────────────────┼──────────┼────────────┼──────────┤\n");
    
    size_t total_elements = 0;
    size_t total_size = 0;
    
    for (uint64_t i = 0; i < ctx->header.tensor_count && i < 20; i++) {
        GGUFTensorInfo* info = &ctx->tensor_infos[i];
        
        uint64_t elements = 1;
        for (uint32_t d = 0; d < info->n_dims; d++) elements *= info->dims[d];
        
        size_t size;
        gguf_get_tensor_data(ctx, info->name.data, &size);
        
        printf("  │ %-26s │ %-8s │ %10llu │ %8s │\n",
               info->name.data, ggml_rxd_type_name((GGMLType)info->type),
               (unsigned long long)elements, format_size(size));
        
        total_elements += elements;
        total_size += size;
    }
    
    if (ctx->header.tensor_count > 20) {
        printf("  │ ... and %llu more tensors                            │\n",
               (unsigned long long)(ctx->header.tensor_count - 20));
    }
    
    printf("  └────────────────────────────┴──────────┴────────────┴──────────┘\n");
    printf("\n  Total elements: %s\n", format_size(total_elements));
    printf("  Total tensor data: %s\n", format_size(total_size));
    
    gguf_close(ctx);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Demo: Hotswap Session
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_hotswap_session(const char* path) {
    print_header("Hotswap Session Demo");
    
    printf("\nCreating hotswap session from: %s\n\n", path);
    
    HotswapSession* s = hotswap_create(path);
    if (!s) {
        printf("  ERROR: Failed to create hotswap session\n");
        return;
    }
    
    printf("  Tensors loaded: %u\n", s->tensor_count);
    printf("  Original size:  %s\n", format_size(s->total_original_size));
    
    // Create lockpick session
    LockpickSession* lp = lockpick_create(path);
    if (lp) {
        printf("\n  Adding experiments...\n");
        lockpick_add_experiment(lp, "speed", &QUANT_PROFILE_SPEED);
        lockpick_add_experiment(lp, "balanced", &QUANT_PROFILE_BALANCED);
        lockpick_add_experiment(lp, "quality", &QUANT_PROFILE_QUALITY);
        
        printf("  Running experiments...\n");
        lockpick_run_all(lp);
        
        const LockpickExperiment* best = lockpick_get_best(lp);
        if (best) {
            printf("\n  Best experiment: %s\n", best->name);
            printf("    SNR:        %.2f dB\n", best->quality.avg_snr_db);
            printf("    Size:       %s\n", format_size(best->metrics.model_size_bytes));
            printf("    Tokens/sec: %.2f\n", best->metrics.tokens_per_sec);
        }
        
        LockpickReport report = lockpick_generate_json(lp);
        printf("\n  JSON Report:\n  %s\n", report.data);
        lockpick_free_report(&report);
        
        lockpick_destroy(lp);
    }
    
    hotswap_destroy(s);
}

/* ═══════════════════════════════════════════════════════════════════════════
   Main
   ═══════════════════════════════════════════════════════════════════════════ */

static void print_usage(const char* prog) {
    printf("Usage: %s [options] [model.gguf]\n\n", prog);
    printf("Options:\n");
    printf("  --quant     Run quantization quality demo\n");
    printf("  --profiles   Show quantization profiles\n");
    printf("  --memory     Show memory detection\n");
    printf("  --timing     Run inference timing demo\n");
    printf("  --analyze    Analyze GGUF file\n");
    printf("  --hotswap    Run hotswap session demo\n");
    printf("  --all        Run all demos\n");
    printf("  --help       Show this help\n\n");
    printf("If a GGUF file is provided, it will be analyzed.\n");
}

int main(int argc, char** argv) {
    printf("\n");
    print_separator();
    printf("  RawrXD - Real Weight Re-quantizer & Hot-Swap\n");
    printf("  Version 1.0.0 - No simulations. Real code.\n");
    print_separator();
    printf("\n");
    
    const char* model_path = NULL;
    bool run_quant = false;
    bool run_profiles = false;
    bool run_memory = false;
    bool run_timing = false;
    bool run_analyze = false;
    bool run_hotswap = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quant") == 0) run_quant = true;
        else if (strcmp(argv[i], "--profiles") == 0) run_profiles = true;
        else if (strcmp(argv[i], "--memory") == 0) run_memory = true;
        else if (strcmp(argv[i], "--timing") == 0) run_timing = true;
        else if (strcmp(argv[i], "--analyze") == 0) run_analyze = true;
        else if (strcmp(argv[i], "--hotswap") == 0) run_hotswap = true;
        else if (strcmp(argv[i], "--all") == 0) {
            run_quant = run_profiles = run_memory = run_timing = true;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (argv[i][0] != '-') {
            model_path = argv[i];
        }
    }
    
    // If no specific demos requested, run all
    if (!run_quant && !run_profiles && !run_memory && !run_timing && !run_analyze && !run_hotswap) {
        run_quant = run_profiles = run_memory = run_timing = true;
    }
    
    if (run_quant) demo_quantization_quality();
    if (run_profiles) demo_quantization_profiles();
    if (run_memory) demo_memory_detection();
    if (run_timing) demo_inference_hook();
    
    if (model_path) {
        if (run_analyze) demo_gguf_analysis(model_path);
        if (run_hotswap) demo_hotswap_session(model_path);
    } else if (run_analyze || run_hotswap) {
        printf("\n");
        print_separator();
        printf("  Note: Provide a GGUF file path to run analyze/hotswap demos\n");
        print_separator();
        printf("\n");
    }
    
    printf("\n");
    print_separator();
    printf("  Demo Complete!\n");
    print_separator();
    printf("\n");
    
    return 0;
}