// bench_flash_all_quant.cpp — Flash-Attention All-Quant Benchmark
// Target: ≥10× speedup on 4K context with FP32 baseline vs Flash O(n) memory

#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <algorithm>

using namespace std::chrono;

// Full baseline attention: O(n²) memory, materializes full attention matrix
extern "C" void attention_baseline(const float* Q, const float* K, const float* V, float* O, int seqLen, int headDim) {
    const float scale = 1.0f / std::sqrt(static_cast<float>(headDim));
    
    // Allocate attention matrix: seqLen × seqLen
    std::vector<float> scores(seqLen * seqLen);
    
    // Compute Q @ K^T (scaled)
    for (int i = 0; i < seqLen; ++i) {
        for (int j = 0; j < seqLen; ++j) {
            float sum = 0.0f;
            for (int d = 0; d < headDim; ++d) {
                sum += Q[i * headDim + d] * K[j * headDim + d];
            }
            scores[i * seqLen + j] = sum * scale;
        }
    }
    
    // Softmax over each row
    for (int i = 0; i < seqLen; ++i) {
        float maxVal = scores[i * seqLen];
        for (int j = 1; j < seqLen; ++j) {
            maxVal = std::max(maxVal, scores[i * seqLen + j]);
        }
        
        float sumExp = 0.0f;
        for (int j = 0; j < seqLen; ++j) {
            scores[i * seqLen + j] = std::exp(scores[i * seqLen + j] - maxVal);
            sumExp += scores[i * seqLen + j];
        }
        
        for (int j = 0; j < seqLen; ++j) {
            scores[i * seqLen + j] /= sumExp;
        }
    }
    
    // Compute output: attention @ V
    for (int i = 0; i < seqLen; ++i) {
        for (int d = 0; d < headDim; ++d) {
            float sum = 0.0f;
            for (int j = 0; j < seqLen; ++j) {
                sum += scores[i * seqLen + j] * V[j * headDim + d];
            }
            O[i * headDim + d] = sum;
        }
    }
}

// Flash Attention: O(n) memory using tiled online softmax
extern "C" void flash_attention(const float* Q, const float* K, const float* V, float* O, int seqLen, int headDim) {
    const float scale = 1.0f / std::sqrt(static_cast<float>(headDim));
    const int tileSize = 256; // Block size for tiling
    
    // Initialize output and running statistics
    std::fill(O, O + seqLen * headDim, 0.0f);
    std::vector<float> rowMax(seqLen, -std::numeric_limits<float>::infinity());
    std::vector<float> rowSum(seqLen, 0.0f);
    
    // Process in tiles to maintain O(n) memory
    for (int kStart = 0; kStart < seqLen; kStart += tileSize) {
        int kEnd = std::min(kStart + tileSize, seqLen);
        int kSize = kEnd - kStart;
        
        // Tile buffer for scores
        std::vector<float> tileScores(seqLen * kSize);
        
        // Compute Q @ K^T for this tile
        for (int i = 0; i < seqLen; ++i) {
            for (int j = 0; j < kSize; ++j) {
                float sum = 0.0f;
                for (int d = 0; d < headDim; ++d) {
                    sum += Q[i * headDim + d] * K[(kStart + j) * headDim + d];
                }
                tileScores[i * kSize + j] = sum * scale;
            }
        }
        
        // Online softmax: update running max and sum
        for (int i = 0; i < seqLen; ++i) {
            float oldMax = rowMax[i];
            float newMax = oldMax;
            
            // Find new max in this tile
            for (int j = 0; j < kSize; ++j) {
                newMax = std::max(newMax, tileScores[i * kSize + j]);
            }
            
            // Rescale previous output and sum if max changed
            if (newMax > oldMax) {
                float expDiff = std::exp(oldMax - newMax);
                for (int d = 0; d < headDim; ++d) {
                    O[i * headDim + d] *= expDiff;
                }
                rowSum[i] *= expDiff;
            }
            
            // Compute exp and accumulate for this tile
            float tileSum = 0.0f;
            for (int j = 0; j < kSize; ++j) {
                tileScores[i * kSize + j] = std::exp(tileScores[i * kSize + j] - newMax);
                tileSum += tileScores[i * kSize + j];
            }
            
            // Accumulate weighted values from this tile
            for (int d = 0; d < headDim; ++d) {
                float sum = 0.0f;
                for (int j = 0; j < kSize; ++j) {
                    sum += tileScores[i * kSize + j] * V[(kStart + j) * headDim + d];
                }
                O[i * headDim + d] += sum;
            }
            
            rowMax[i] = newMax;
            rowSum[i] += tileSum;
        }
    }
    
    // Final normalization
    for (int i = 0; i < seqLen; ++i) {
        for (int d = 0; d < headDim; ++d) {
            O[i * headDim + d] /= rowSum[i];
        }
    }
}

int main() {
    const int seqLen = 4096;
    const int headDim = 64;
    
    std::cout << "Flash-Attention All-Quant Benchmark\n";
    std::cout << "Shape: " << seqLen << " × " << headDim << " (4K context)\n\n";
    
    // Allocate buffers
    std::vector<float> Q(seqLen * headDim);
    std::vector<float> K(seqLen * headDim);
    std::vector<float> V(seqLen * headDim);
    std::vector<float> O_baseline(seqLen * headDim);
    std::vector<float> O_flash(seqLen * headDim);
    
    // Random init
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (auto& x : Q) x = dist(rng);
    for (auto& x : K) x = dist(rng);
    for (auto& x : V) x = dist(rng);
    
    // Warmup
    std::cout << "Warming up...\n";
    attention_baseline(Q.data(), K.data(), V.data(), O_baseline.data(), seqLen, headDim);
    flash_attention(Q.data(), K.data(), V.data(), O_flash.data(), seqLen, headDim);
    
    // Benchmark FP32 baseline (full materialization)
    std::cout << "Running FP32 baseline (O(n²) memory)...\n";
    auto t0 = high_resolution_clock::now();
    attention_baseline(Q.data(), K.data(), V.data(), O_baseline.data(), seqLen, headDim);
    auto t1 = high_resolution_clock::now();
    double ms_fp32 = duration<double, std::milli>(t1 - t0).count();
    
    // Benchmark Flash-Attention (tiled online-softmax)
    std::cout << "Running Flash-Attention (O(n) memory)...\n";
    t0 = high_resolution_clock::now();
    flash_attention(Q.data(), K.data(), V.data(), O_flash.data(), seqLen, headDim);
    t1 = high_resolution_clock::now();
    double ms_flash = duration<double, std::milli>(t1 - t0).count();
    
    // Verify correctness
    float maxDiff = 0.0f;
    for (size_t i = 0; i < O_baseline.size(); ++i) {
        maxDiff = std::max(maxDiff, std::abs(O_baseline[i] - O_flash[i]));
    }
    
    // Report
    double speedup = ms_fp32 / ms_flash;
    std::cout << "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "Max abs diff: " << maxDiff << "\n";
    std::cout << "FP32 baseline: " << ms_fp32 << " ms\n";
    std::cout << "Flash (O(n)):  " << ms_flash << " ms\n";
    std::cout << "Speedup: " << speedup << "x\n";
    
    if (speedup >= 10.0) {
        std::cout << "✅ END-TO-END: >= 10× speedup achieved\n";
        return 0;
    } else {
        std::cout << "❌ END-TO-END: < 10× speedup (got " << speedup << "x)\n";
        return 1;
    }
}
