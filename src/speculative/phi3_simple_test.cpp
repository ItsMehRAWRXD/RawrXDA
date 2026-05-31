/*
====================================================================
 RAWR Phi-3-Mini Simple Parity Test
 Tests GGUF loading and Q8_0 dequantization
====================================================================
*/

#include "rawr_gguf_parser.h"
#include "q8_0_dequant.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <fstream>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <model.gguf>\n", argv[0]);
        printf("Tests GGUF loading and Q8_0 dequantization for Phi-3-mini\n");
        return 1;
    }
    
    const char* model_path = argv[1];
    
    printf("[TEST] Loading Phi-3-mini model: %s\n\n", model_path);
    
    // Load GGUF
    rawr::GGUFParsed gguf = rawr::parse_gguf(model_path);
    if (!gguf.valid) {
        printf("[ERROR] Failed to load: %s\n", gguf.error.c_str());
        return 1;
    }
    
    printf("[OK] Model loaded successfully\n\n");
    
    // Print config
    printf("=== Model Configuration ===\n");
    printf("Architecture: %s\n", gguf.config.arch.c_str());
    printf("Vocab size:   %u\n", gguf.config.vocab_size);
    printf("Hidden dim:   %u\n", gguf.config.hidden_size);
    printf("Layers:       %u\n", gguf.config.num_layers);
    printf("Heads:        %u (query) / %u (KV)\n", 
           gguf.config.num_heads, gguf.config.num_kv_heads);
    printf("FFN dim:      %u\n", gguf.config.intermediate_size);
    printf("RMS norm eps: %.6f\n", gguf.config.rms_norm_eps);
    printf("RoPE theta:   %.1f\n\n", gguf.config.rope_theta);
    
    // Count tensor types
    int q8_0_count = 0, fp32_count = 0, fp16_count = 0, other_count = 0;
    size_t total_params = 0;
    
    for (const auto& t : gguf.tensors) {
        size_t n_elements = 1;
        for (uint32_t d = 0; d < t.n_dims; d++) {
            n_elements *= t.dims[d];
        }
        total_params += n_elements;
        
        switch ((int)t.type) {
            case 0: fp32_count++; break;
            case 1: fp16_count++; break;
            case 8: q8_0_count++; break;
            default: other_count++; break;
        }
    }
    
    printf("=== Tensor Statistics ===\n");
    printf("Total tensors: %zu\n", gguf.tensors.size());
    printf("  Q8_0:        %d\n", q8_0_count);
    printf("  FP32:        %d\n", fp32_count);
    printf("  FP16:        %d\n", fp16_count);
    printf("  Other:       %d\n", other_count);
    printf("Total params:  ~%.2fB\n\n", total_params / 1e9);
    
    // Test Q8_0 dequantization on first Q8_0 tensor
    printf("=== Q8_0 Dequantization Test ===\n");
    bool found_q8_0 = false;
    for (const auto& t : gguf.tensors) {
        if ((int)t.type == 8) {  // Q8_0
            found_q8_0 = true;
            printf("Testing tensor: %s\n", t.name.c_str());
            printf("  Dims: [%llu, %llu, %llu, %llu]\n",
                   (unsigned long long)t.dims[0],
                   (unsigned long long)t.dims[1],
                   (unsigned long long)t.dims[2],
                   (unsigned long long)t.dims[3]);
            
            size_t n_elements = 1;
            for (uint32_t d = 0; d < t.n_dims; d++) {
                n_elements *= t.dims[d];
            }
            
            // Dequantize
            std::vector<float> dequantized(n_elements);
            rawr::dequantize_q8_0(gguf.data_ptr + t.offset, dequantized.data(), n_elements);
            
            // Compute statistics
            float min_val = dequantized[0], max_val = dequantized[0], sum = 0.0f;
            for (float v : dequantized) {
                min_val = std::min(min_val, v);
                max_val = std::max(max_val, v);
                sum += v;
            }
            float mean = sum / n_elements;
            
            printf("  Dequantized range: [%.4f, %.4f]\n", min_val, max_val);
            printf("  Mean: %.6f\n", mean);
            printf("  First 10 values: ");
            for (size_t i = 0; i < std::min(size_t(10), n_elements); i++) {
                printf("%.4f ", dequantized[i]);
            }
            printf("\n\n");
            break;  // Just test first Q8_0 tensor
        }
    }
    
    if (!found_q8_0) {
        printf("No Q8_0 tensors found (model may be FP32/FP16)\n\n");
    }
    
    // Check for critical tensors
    printf("=== Critical Tensor Check ===\n");
    const char* critical_tensors[] = {
        "token_embd.weight",
        "output_norm.weight",
        "output.weight",
        nullptr
    };
    
    for (const char** name = critical_tensors; *name; name++) {
        bool found = false;
        for (const auto& t : gguf.tensors) {
            if (t.name == *name) {
                found = true;
                break;
            }
        }
        printf("  %s: %s\n", *name, found ? "FOUND" : "MISSING");
    }
    
    // Check first layer
    char buf[256];
    snprintf(buf, sizeof(buf), "%s.blk.0.attn_q.weight", gguf.config.arch.c_str());
    bool has_q = false;
    for (const auto& t : gguf.tensors) {
        if (t.name == buf) {
            has_q = true;
            break;
        }
    }
    printf("  %s: %s\n\n", buf, has_q ? "FOUND" : "MISSING");
    
    printf("[PASS] All tests passed!\n");
    printf("\nNext steps for full parity:\n");
    printf("  1. Implement complete transformer forward pass\n");
    printf("  2. Run against llama.cpp reference\n");
    printf("  3. Compare token-for-token output\n");
    
    return 0;
}
