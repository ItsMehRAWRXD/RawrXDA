// rawr_phi3_parity.cpp - Minimal Phi-3-mini forward pass with Q8_0 support
// Compile: g++ -std=c++17 -O2 -mavx2 -o rawr_phi3_parity.exe rawr_phi3_parity.cpp

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// Q8_0 block: 32 weights (int8) + 1 fp16 scale
struct block_q8_0 {
    uint16_t d;     // scale (fp16)
    int8_t qs[32];  // 32 quantized weights
};

// Dequantize Q8_0 to float
void dequantize_q8_0(const uint8_t* src, float* dst, size_t n) {
    const block_q8_0* blocks = (const block_q8_0*)src;
    size_t nb = (n + 31) / 32;
    
    for (size_t b = 0; b < nb; ++b) {
        // Convert fp16 scale to fp32
        uint16_t h = blocks[b].d;
        float d;
        if (h == 0) {
            d = 0.0f;
        } else {
            uint32_t sign = (h >> 15) & 1;
            uint32_t exp = (h >> 10) & 0x1F;
            uint32_t mant = h & 0x3FF;
            if (exp == 0) {
                d = 0.0f;
            } else {
                uint32_t f32 = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
                memcpy(&d, &f32, 4);
            }
        }
        
        // Dequantize 32 weights
        for (int i = 0; i < 32 && (b * 32 + i) < n; ++i) {
            dst[b * 32 + i] = blocks[b].qs[i] * d;
        }
    }
}

// Simple matrix-vector multiplication: y = Wx
void matmul(const float* W, const float* x, float* y, int rows, int cols) {
    for (int i = 0; i < rows; ++i) {
        float sum = 0.0f;
        for (int j = 0; j < cols; ++j) {
            sum += W[i * cols + j] * x[j];
        }
        y[i] = sum;
    }
}

// RMSNorm: x = x / sqrt(mean(x^2) + eps)
void rmsnorm(float* x, int n, float eps) {
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        sum += x[i] * x[i];
    }
    float scale = 1.0f / sqrtf(sum / n + eps);
    for (int i = 0; i < n; ++i) {
        x[i] *= scale;
    }
}

// Softmax
void softmax(float* x, int n) {
    float maxv = x[0];
    for (int i = 1; i < n; ++i) {
        if (x[i] > maxv) maxv = x[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        x[i] = expf(x[i] - maxv);
        sum += x[i];
    }
    for (int i = 0; i < n; ++i) {
        x[i] /= sum;
    }
}

// Simple tokenizer (character-level for testing)
std::vector<int> tokenize(const std::string& text) {
    std::vector<int> tokens;
    for (char c : text) {
        if (c >= 'a' && c <= 'z') tokens.push_back(c - 'a' + 1);
        else if (c >= 'A' && c <= 'Z') tokens.push_back(c - 'A' + 1);
        else if (c == ' ') tokens.push_back(0);
        else tokens.push_back(27); // unknown
    }
    return tokens;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: %s <model.gguf> <prompt> [max_tokens]\n", argv[0]);
        return 1;
    }
    
    const char* model_path = argv[1];
    const char* prompt = argv[2];
    int max_tokens = (argc > 3) ? atoi(argv[3]) : 5;
    
    printf("[RAWR] Phi-3-mini Parity Test\n");
    printf("[RAWR] Model: %s\n", model_path);
    printf("[RAWR] Prompt: \"%s\"\n", prompt);
    
    // For now, just test Q8_0 dequantization
    printf("[RAWR] Testing Q8_0 dequantization...\n");
    
    // Create test data: 64 floats
    float test_in[64];
    for (int i = 0; i < 64; ++i) {
        test_in[i] = (i - 32) * 0.1f;  // Range: -3.2 to +3.1
    }
    
    // Quantize to Q8_0 (simplified)
    block_q8_0 block;
    float max_abs = 0.0f;
    for (int i = 0; i < 32; ++i) {
        if (fabsf(test_in[i]) > max_abs) max_abs = fabsf(test_in[i]);
    }
    float scale = max_abs / 127.0f;
    // Pack scale as fp16
    // (simplified - just use raw bytes)
    memcpy(&block.d, &scale, 2);
    for (int i = 0; i < 32; ++i) {
        block.qs[i] = (int8_t)roundf(test_in[i] / scale);
    }
    
    // Dequantize
    float test_out[32];
    dequantize_q8_0((uint8_t*)&block, test_out, 32);
    
    // Check error
    float max_err = 0.0f;
    for (int i = 0; i < 32; ++i) {
        float err = fabsf(test_in[i] - test_out[i]);
        if (err > max_err) max_err = err;
    }
    printf("[RAWR] Q8_0 dequantization max error: %f\n", max_err);
    
    if (max_err < 0.01f) {
        printf("[RAWR] Q8_0 dequantization: PASS\n");
    } else {
        printf("[RAWR] Q8_0 dequantization: FAIL\n");
    }
    
    // TODO: Load actual Phi-3-mini model and run forward pass
    printf("[RAWR] Full Phi-3-mini forward pass: NOT YET IMPLEMENTED\n");
    printf("[RAWR] Need to implement:\n");
    printf("  - GGUF loading with Q8_0 tensors\n");
    printf("  - Token embedding lookup\n");
    printf("  - Transformer layers (RMSNorm, QKV, RoPE, attention, FFN)\n");
    printf("  - Output projection\n");
    printf("  - Logit dumping\n");
    
    return 0;
}
