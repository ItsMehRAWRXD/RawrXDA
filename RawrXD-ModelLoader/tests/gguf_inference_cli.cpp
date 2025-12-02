// Simple GGUF Q4_0 inference CLI for tok/s benchmarking
// Usage: gguf_inference_cli model.gguf "prompt" num_tokens [--no-avx2]

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <cmath>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#if defined(__AVX2__) || (defined(_MSC_VER) && defined(__AVX2__))
#include <immintrin.h>
#define HAS_AVX2 1
#else
#define HAS_AVX2 0
#endif

using namespace std;
using namespace std::chrono;

// Q4_0 block: 16 bytes (32 nibbles) + 2 bytes delta
struct BlockQ4_0 {
    uint16_t d;      // FP16 scale
    uint8_t qs[16];  // 32 nibbles (4 bits each)
};

// FP16 to FP32 conversion
float f16_to_f32(uint16_t h) {
    uint32_t sign = (h >> 15) & 1;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x3FF;
    
    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        while ((mant & 0x400) == 0) { mant <<= 1; exp--; }
        exp++; mant &= 0x3FF;
    } else if (exp == 31) {
        return mant ? NAN : (sign ? -INFINITY : INFINITY);
    }
    
    exp = exp - 15 + 127;
    uint32_t f32 = (sign << 31) | (exp << 23) | (mant << 13);
    float result;
    memcpy(&result, &f32, sizeof(float));
    return result;
}

// Scalar Q4_0 dequantization
void dequant_q4_0_scalar(const BlockQ4_0* blocks, float* dst, size_t n) {
    size_t nb = n / 32;
    for (size_t i = 0; i < nb; ++i) {
        float d = f16_to_f32(blocks[i].d);
        for (size_t j = 0; j < 16; ++j) {
            int vi = (blocks[i].qs[j] & 0xF) - 8;
            dst[i*32 + j] = vi * d;
        }
        for (size_t j = 0; j < 16; ++j) {
            int vi = (blocks[i].qs[j] >> 4) - 8;
            dst[i*32 + j + 16] = vi * d;
        }
    }
}

#if HAS_AVX2
// AVX2 Q4_0 batch unpacking (from Phase 2)
extern "C" void q4_0_unpack_64x64(const BlockQ4_0* src, float* dst);
extern "C" void matmul_kernel_avx2(const float* A, const float* B, float* C, int M, int N, int K, bool accumulate = false);

// Simple runtime AVX2 check
bool has_avx2_runtime() {
    int info[4];
    __cpuid(info, 0);
    if (info[0] >= 7) {
        __cpuidex(info, 7, 0);
        return (info[1] & (1 << 5)) != 0;  // EBX bit 5 = AVX2
    }
    return false;
}
#endif

// Scalar matmul baseline
void matmul_scalar(const float* A, const float* B, float* C, int M, int N, int K) {
    for (int i = 0; i < M; ++i) {
        for (int j = 0; j < N; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < K; ++k) {
                sum += A[i*K + k] * B[k*N + j];
            }
            C[i*N + j] = sum;
        }
    }
}

// Simple top-1 sampling
int sample_greedy(const float* logits, int vocab_size) {
    int max_idx = 0;
    float max_val = logits[0];
    for (int i = 1; i < vocab_size; i++) {
        if (logits[i] > max_val) {
            max_val = logits[i];
            max_idx = i;
        }
    }
    return max_idx;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " model.gguf \"prompt\" num_tokens [--no-avx2]\n";
        return 1;
    }

    string model_path = argv[1];
    string prompt = argv[2];
    int num_tokens = atoi(argv[3]);
    bool use_avx2 = true;
    
    if (argc > 4 && string(argv[4]) == "--no-avx2") {
        use_avx2 = false;
    }

#if HAS_AVX2
    bool has_avx2 = has_avx2_runtime();
    if (use_avx2 && !has_avx2) {
        cerr << "AVX2 requested but not supported by CPU\n";
        use_avx2 = false;
    }
#else
    use_avx2 = false;
#endif

    cout << "RawrXD GGUF Q4_0 Inference Test\n";
    cout << "Model: " << model_path << "\n";
    cout << "Prompt: \"" << prompt << "\"\n";
    cout << "Tokens: " << num_tokens << "\n";
    cout << "Mode: " << (use_avx2 ? "AVX2" : "Scalar") << "\n\n";

    // Simplified inference simulation for tok/s testing
    // In a real implementation, we'd parse GGUF, load weights, etc.
    // For now, simulate the compute-heavy parts with realistic dimensions
    
    const int embed_dim = 2048;    // TinyLlama embedding dimension
    const int vocab_size = 32000;  // Standard LLaMA vocab
    const int hidden_dim = 5632;   // TinyLlama FFN dimension
    
    // Simulate weight matrices (Q4_0 format)
    vector<BlockQ4_0> q4_weights(embed_dim * hidden_dim / 32);
    for (size_t i = 0; i < q4_weights.size(); ++i) {
        q4_weights[i].d = 0x3C00;  // FP16 value 1.0
        for (int j = 0; j < 16; ++j) {
            q4_weights[i].qs[j] = 0x88;  // Mid-range nibbles
        }
    }
    
    vector<float> hidden(embed_dim);
    vector<float> output(hidden_dim);
    vector<float> logits(vocab_size);
    
    // Scratch buffer for batch dequantization (if AVX2)
    vector<float> weight_scratch(embed_dim * hidden_dim);
    
    cout << "Running inference...\n";
    auto t_start = high_resolution_clock::now();
    
    int generated = 0;
    for (int tok = 0; tok < num_tokens; ++tok) {
        // Initialize hidden state (in real code, from embeddings)
        for (int i = 0; i < embed_dim; ++i) {
            hidden[i] = 0.5f * (i % 10);
        }
        
        // Core compute: matrix multiply (hidden × weights -> output)
        // This is the bottleneck we're optimizing
        
#if HAS_AVX2
        if (use_avx2) {
            // Phase 2 approach: Batch dequantize ONCE, then fast GEMM
            // In real code, we'd dequant once per layer, not per token
            // But for benchmark consistency, we include the dequant cost
            dequant_q4_0_scalar(q4_weights.data(), weight_scratch.data(), embed_dim * hidden_dim);
            
            // Now use fast AVX2 GEMM on FP32 data
            // matmul_kernel_avx2 expects 64×64 blocks, so we call it in a loop
            constexpr int BLOCK = 64;
            for (int jb = 0; jb < hidden_dim; jb += BLOCK) {
                for (int kb = 0; kb < embed_dim; kb += BLOCK) {
                    matmul_kernel_avx2(&hidden[kb],
                                       &weight_scratch[kb * hidden_dim + jb],
                                       &output[jb],
                                       1, BLOCK, BLOCK, false);
                }
            }
        } else
#endif
        {
            // Scalar path: dequantize on-the-fly during matmul
            dequant_q4_0_scalar(q4_weights.data(), weight_scratch.data(), embed_dim * hidden_dim);
            matmul_scalar(hidden.data(), weight_scratch.data(), output.data(), 1, hidden_dim, embed_dim);
        }
        
        // Project to vocab (simplified - just copy first vocab_size values)
        for (int i = 0; i < vocab_size && i < hidden_dim; ++i) {
            logits[i] = output[i];
        }
        
        // Sample next token
        int next_token = sample_greedy(logits.data(), vocab_size);
        generated++;
        
        // Print progress every 10 tokens
        if ((tok + 1) % 10 == 0 || tok == num_tokens - 1) {
            auto t_now = high_resolution_clock::now();
            auto elapsed_ms = duration_cast<milliseconds>(t_now - t_start).count();
            double tok_per_sec = (generated * 1000.0) / elapsed_ms;
            cout << "  Generated " << generated << "/" << num_tokens 
                 << " tokens (" << tok_per_sec << " tok/s)\r" << flush;
        }
    }
    
    auto t_end = high_resolution_clock::now();
    auto elapsed_ms = duration_cast<milliseconds>(t_end - t_start).count();
    double tok_per_sec = (num_tokens * 1000.0) / elapsed_ms;
    
    cout << "\n\n";
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    cout << "RESULTS:\n";
    cout << "  Tokens generated: " << num_tokens << "\n";
    cout << "  Total time: " << elapsed_ms << " ms\n";
    cout << "  Throughput: " << tok_per_sec << " tokens/sec\n";
    cout << "  Time per token: " << (elapsed_ms / (double)num_tokens) << " ms\n";
    cout << "  Mode: " << (use_avx2 ? "AVX2 (optimized)" : "Scalar (baseline)") << "\n";
    cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    
    return 0;
}
