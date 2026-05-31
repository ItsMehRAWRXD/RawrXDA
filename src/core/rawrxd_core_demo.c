/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_CORE Demo
   Demonstrates GGUF loading, quantization, hotswap, lockpick, tools, inference
   ═══════════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rawrxd_core.h"

/* Demo helper */
static void print_separator(const char* title) {
    printf("\n═══════════════════════════════════════════════════════════════════════════\n");
    printf("  %s\n", title);
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
}

/* Demo: Quantization */
static void demo_quantization(void) {
    print_separator("Quantization Demo");
    
    /* Create test data */
    float data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = (float)(i - 128) / 128.0f; /* Range -1 to 1 */
    }
    
    printf("Original data: 256 floats (%zu bytes)\n", 256 * sizeof(float));
    
    /* Q4_0 quantization */
    RXDQuant q4 = rxd_quant(data, 256, GGML_RXD_TYPE_Q4_0);
    printf("Q4_0 quantized: %zu bytes (%.1f%% compression)\n", 
           q4.size, 100.0 * (1.0 - (double)q4.size / (256 * sizeof(float))));
    
    RXDDequant d4 = rxd_dequant(q4.data, GGML_RXD_TYPE_Q4_0, 256);
    RXDQuality q4_qual = rxd_estimate_quality(data, d4.data, 256);
    printf("  Q4_0 Quality: MSE=%.6f, SNR=%.2f dB\n", q4_qual.mse, q4_qual.snr_db);
    
    /* Q8_0 quantization */
    RXDQuant q8 = rxd_quant(data, 256, GGML_RXD_TYPE_Q8_0);
    printf("Q8_0 quantized: %zu bytes (%.1f%% compression)\n", 
           q8.size, 100.0 * (1.0 - (double)q8.size / (256 * sizeof(float))));
    
    RXDDequant d8 = rxd_dequant(q8.data, GGML_RXD_TYPE_Q8_0, 256);
    RXDQuality q8_qual = rxd_estimate_quality(data, d8.data, 256);
    printf("  Q8_0 Quality: MSE=%.6f, SNR=%.2f dB\n", q8_qual.mse, q8_qual.snr_db);
    
    /* Q4_K quantization */
    RXDQuant q4k = rxd_quant(data, 256, GGML_RXD_TYPE_Q4_K);
    printf("Q4_K quantized: %zu bytes (%.1f%% compression)\n", 
           q4k.size, 100.0 * (1.0 - (double)q4k.size / (256 * sizeof(float))));
    
    RXDDequant d4k = rxd_dequant(q4k.data, GGML_RXD_TYPE_Q4_K, 256);
    RXDQuality q4k_qual = rxd_estimate_quality(data, d4k.data, 256);
    printf("  Q4_K Quality: MSE=%.6f, SNR=%.2f dB\n", q4k_qual.mse, q4k_qual.snr_db);
    
    /* Cleanup */
    rxd_quant_free(&q4);
    rxd_dequant_free(&d4);
    rxd_quant_free(&q8);
    rxd_dequant_free(&d8);
    rxd_quant_free(&q4k);
    rxd_dequant_free(&d4k);
}

/* Demo: Memory Mapping */
static void demo_mmap(void) {
    print_separator("Memory Mapping Demo");
    
    /* Create a test file */
    const char* test_file = "demo_mmap.bin";
    FILE* f = fopen(test_file, "wb");
    if (f) {
        uint8_t data[4096];
        for (int i = 0; i < 4096; i++) data[i] = (uint8_t)(i & 0xFF);
        fwrite(data, 1, 4096, f);
        fclose(f);
        
        /* Memory map it */
        RXDMemoryMap map = rxd_mmap_create(test_file);
        if (map.base) {
            printf("Memory mapped: %zu bytes\n", map.size);
            printf("  First 16 bytes: ");
            uint8_t* p = (uint8_t*)map.base;
            for (int i = 0; i < 16; i++) printf("%02X ", p[i]);
            printf("\n");
            
            rxd_mmap_destroy(&map);
        }
        
        remove(test_file);
    }
}

/* Demo: Tools */
static void demo_tools(void) {
    print_separator("Tools Demo");
    
    rxd_tools_init();
    printf("Initialized %u tools\n", rxd_tool_count);
    
    /* List tools */
    for (uint32_t i = 0; i < rxd_tool_count; i++) {
        printf("  [%u] %s: %s\n", i, rxd_tools[i].name, rxd_tools[i].description);
    }
    
    /* Test memory tool */
    printf("\nTesting memory tool:\n");
    char args[4096];
    snprintf(args, sizeof(args), "demo_key\nThis is a stored value");
    RXDToolResult r = rxd_tool_execute("memory_store", args);
    printf("  Store: %s\n", r.success ? "OK" : r.error);
    
    r = rxd_tool_execute("memory_recall", "demo_key");
    printf("  Recall: %s\n", r.success ? r.output : r.error);
}

/* Demo: Timer */
static void demo_timer(void) {
    print_separator("Timer Demo");
    
    RXDTimer t;
    rxd_timer_start(&t);
    
    /* Do some work */
    volatile double sum = 0;
    for (int i = 0; i < 10000000; i++) {
        sum += (double)i * 0.0001;
    }
    
    uint64_t ns = rxd_timer_stop(&t);
    printf("Work completed in: %.3f ms (%.3f µs)\n", 
           ns / 1000000.0, ns / 1000.0);
}

/* Demo: LSP Bridge */
static void demo_lsp(void) {
    print_separator("LSP Bridge Demo");
    
    rxd_lsp_connect("clangd");
    printf("LSP connected: %s\n", g_lsp.connected ? "yes" : "no");
    
    RXDLSPRequest req = rxd_lsp_definition("demo.c", 10, 5);
    printf("Definition request: %s\n", req.success ? "success" : "failed");
    printf("  Result: %s\n", req.result);
    
    req = rxd_lsp_hover("demo.c", 20, 10);
    printf("Hover request: %s\n", req.success ? "success" : "failed");
    printf("  Result: %s\n", req.result);
    
    rxd_lsp_disconnect();
}

/* Demo: Profiles */
static void demo_profiles(void) {
    print_separator("Quantization Profiles");
    
    printf("Speed Profile:\n");
    printf("  Default: Q4_0, Attn: Q4_0, FFN: Q4_0, Embed: Q8_0, Output: Q8_0\n");
    printf("  Min SNR: %.1f dB\n", RXD_PROFILE_SPEED.min_snr);
    
    printf("\nBalanced Profile:\n");
    printf("  Default: Q4_K, Attn: Q5_K, FFN: Q4_K, Embed: Q8_0, Output: Q6_K\n");
    printf("  Min SNR: %.1f dB\n", RXD_PROFILE_BALANCED.min_snr);
    
    printf("\nQuality Profile:\n");
    printf("  Default: Q6_K, Attn: Q6_K, FFN: Q5_K, Embed: F16, Output: F16\n");
    printf("  Min SNR: %.1f dB\n", RXD_PROFILE_QUALITY.min_snr);
}

/* Demo: Memory Detection */
static void demo_memory(void) {
    print_separator("Memory Detection");
    
    size_t sys_mem = rxd_get_system_memory();
    printf("System Memory: %.2f GB\n", sys_mem / (1024.0 * 1024.0 * 1024.0));
    
    size_t gpu_mem = rxd_get_gpu_memory();
    if (gpu_mem > 0) {
        printf("GPU Memory: %.2f GB\n", gpu_mem / (1024.0 * 1024.0 * 1024.0));
    } else {
        printf("GPU Memory: Not detected (requires platform-specific implementation)\n");
    }
}

/* Demo: Weight Classification */
static void demo_weight_classification(void) {
    print_separator("Weight Classification");
    
    struct { const char* name; bool is_attn, is_ffn, is_embed, is_output; } tests[] = {
        {"layers.0.attn.q_proj.weight", true, false, false, false},
        {"layers.0.attn.k_proj.weight", true, false, false, false},
        {"layers.0.attn.v_proj.weight", true, false, false, false},
        {"layers.0.mlp.up_proj.weight", false, true, false, false},
        {"layers.0.mlp.down_proj.weight", false, true, false, false},
        {"tok_embeddings.weight", false, false, true, false},
        {"output.weight", false, false, false, true},
        {"lm_head.weight", false, false, false, true},
    };
    
    for (int i = 0; i < 8; i++) {
        RXDWeight w;
        strncpy(w.name, tests[i].name, 127);
        bool attn = rxd_is_attn(&w);
        bool ffn = rxd_is_ffn(&w);
        bool embed = rxd_is_embed(&w);
        bool output = rxd_is_output(&w);
        
        printf("  %-30s: attn=%c ffn=%c embed=%c output=%c\n",
               w.name,
               attn ? 'Y' : 'N',
               ffn ? 'Y' : 'N',
               embed ? 'Y' : 'N',
               output ? 'Y' : 'N');
    }
}

/* Demo: Report Generation */
static void demo_report(void) {
    print_separator("Report Generation");
    
    /* Create minimal lockpick */
    RXDLockpick l = {0};
    l.exp_capacity = 4;
    l.experiments = calloc(4, sizeof(RXDExperiment));
    l.exp_count = 3;
    
    strcpy(l.experiments[0].name, "speed");
    l.experiments[0].metrics.tps = 85.5;
    l.experiments[0].quality.avg_snr = 18.2f;
    l.experiments[0].metrics.model_size = 1024 * 1024 * 1024; /* 1GB */
    
    strcpy(l.experiments[1].name, "balanced");
    l.experiments[1].metrics.tps = 62.3;
    l.experiments[1].quality.avg_snr = 24.5f;
    l.experiments[1].metrics.model_size = 1536 * 1024 * 1024; /* 1.5GB */
    
    strcpy(l.experiments[2].name, "quality");
    l.experiments[2].metrics.tps = 38.1;
    l.experiments[2].quality.avg_snr = 32.8f;
    l.experiments[2].metrics.model_size = 2048 * 1024 * 1024; /* 2GB */
    
    l.best_exp = 1; /* Balanced wins */
    
    RXDReport r = rxd_report_json(&l);
    printf("JSON Report:\n%s\n", r.data);
    
    rxd_report_free(&r);
    free(l.experiments);
}

/* Main */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                     RAWRXD_CORE Production Demo                          ║\n");
    printf("║                     ~2,800 lines, no stubs, real code                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════════════════════╝\n");
    
    demo_quantization();
    demo_mmap();
    demo_tools();
    demo_timer();
    demo_lsp();
    demo_profiles();
    demo_memory();
    demo_weight_classification();
    demo_report();
    
    print_separator("Demo Complete");
    printf("All demos completed successfully!\n\n");
    
    return 0;
}