// Kernel and stub implementations for linking
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include "engine/inference_kernels.h"
#include "common_types.h"

// Real kernel implementations
void InferenceKernels::softmax_avx512(float* x, int n) {
    float max_val = x[0];
    for (int i = 1; i < n; i++) if (x[i] > max_val) max_val = x[i];
    
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        x[i] = std::exp(x[i] - max_val);
        sum += x[i];
    }
    for (int i = 0; i < n; i++) x[i] /= sum;
}

void InferenceKernels::rmsnorm_avx512(float* o, const float* x, const float* weight, int n, float eps) {
    float rms = 0.0f;
    for (int i = 0; i < n; i++) rms += x[i] * x[i];
    rms = 1.0f / std::sqrt(rms / n + eps);
    for (int i = 0; i < n; i++) o[i] = x[i] * rms * weight[i];
}

void InferenceKernels::rope_avx512(float* x, float* y, int pos, int head_dim, float theta, float freq_base) {
    for (int i = 0; i < head_dim; i += 2) {
        float freq = freq_base == 0 ? 1.0f / std::pow(theta, i / (float)head_dim) : 1.0f / std::pow(freq_base, i / (float)head_dim);
        float angle = pos * freq;
        float cos_a = std::cos(angle);
        float sin_a = std::sin(angle);
        
        float x_val = x[i];
        float x_val_next = i+1 < head_dim ? x[i+1] : 0.0f;
        x[i] = x_val * cos_a - x_val_next * sin_a;
        if (i+1 < head_dim) x[i+1] = x_val * sin_a + x_val_next * cos_a;
    }
}

void InferenceKernels::matmul_q4_0_fused(const float* x, const block_q4_0* y, float* z, int n, int m, int k) {
    // Quantized 4-bit matmul: x(n,k) @ y(k,m) -> z(n,m)
    std::memset(z, 0, n * m * sizeof(float));
    
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            float sum = 0.0f;
            for (int l = 0; l < k; l++) {
                int block_idx = (l / 32) * m + j;
                if (block_idx < k/32 * m) {
                    const block_q4_0& block = y[block_idx];
                    int in_block = l % 32;
                    uint8_t qval = (in_block % 2 == 0) ? 
                        (block.qs[in_block/2] & 0x0F) : (block.qs[in_block/2] >> 4);
                    float val = (qval - 8) * block.d;
                    sum += x[i * k + l] * val;
                }
            }
            z[i * m + j] = sum;
        }
    }
}

// LZ77-style codec
namespace codec {
    std::vector<unsigned char> deflate(const std::vector<unsigned char>& data, bool* ok) {
        if (data.empty()) { if (ok) *ok = true; return {}; }
        
        std::vector<unsigned char> out;
        out.reserve(data.size());
        // Header: original size (4 bytes LE)
        uint32_t origSize = static_cast<uint32_t>(data.size());
        out.push_back(origSize & 0xFF);
        out.push_back((origSize >> 8) & 0xFF);
        out.push_back((origSize >> 16) & 0xFF);
        out.push_back((origSize >> 24) & 0xFF);
        
        size_t i = 0;
        while (i < data.size()) {
            // Search for longest match in sliding window (up to 255 back, up to 255 length)
            int bestLen = 0, bestDist = 0;
            int maxBack = (i < 255) ? static_cast<int>(i) : 255;
            for (int d = 1; d <= maxBack; d++) {
                int len = 0;
                while (len < 255 && i + len < data.size() && data[i + len] == data[i - d + (len % d)]) {
                    len++;
                }
                if (len > bestLen && len >= 3) {
                    bestLen = len;
                    bestDist = d;
                }
            }
            if (bestLen >= 3) {
                out.push_back(0x00); // match marker
                out.push_back(static_cast<unsigned char>(bestDist));
                out.push_back(static_cast<unsigned char>(bestLen));
                i += bestLen;
            } else {
                // Count run of literals (up to 255)
                size_t litStart = i;
                int litCount = 0;
                while (litCount < 255 && i < data.size()) {
                    // Quick check: would next position be a match?
                    if (i + 3 <= data.size() && i >= 1) {
                        bool foundMatch = false;
                        int mb = (i < 255) ? static_cast<int>(i) : 255;
                        for (int d = 1; d <= mb && !foundMatch; d++) {
                            if (data[i] == data[i-d] && data[i+1] == data[i-d+1] && data[i+2] == data[i-d+2]) {
                                foundMatch = true;
                            }
                        }
                        if (foundMatch) break;
                    }
                    litCount++;
                    i++;
                }
                out.push_back(static_cast<unsigned char>(litCount)); // literal count (1-255, non-zero)
                for (int j = 0; j < litCount; j++) {
                    out.push_back(data[litStart + j]);
                }
            }
        }
        if (ok) *ok = true;
        return out;
    }
    
    std::vector<unsigned char> inflate(const std::vector<unsigned char>& data, bool* ok) {
        if (data.size() < 4) { if (ok) *ok = false; return {}; }
        
        uint32_t origSize = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
        std::vector<unsigned char> out;
        out.reserve(origSize);
        
        size_t i = 4;
        while (i < data.size() && out.size() < origSize) {
            unsigned char tag = data[i++];
            if (tag == 0x00 && i + 1 < data.size()) {
                // Match: distance, length
                int dist = data[i++];
                int len = data[i++];
                for (int j = 0; j < len && out.size() < origSize; j++) {
                    out.push_back(out[out.size() - dist]);
                }
            } else {
                // Literals: tag = count
                int count = tag;
                for (int j = 0; j < count && i < data.size() && out.size() < origSize; j++) {
                    out.push_back(data[i++]);
                }
            }
        }
        if (ok) *ok = (out.size() == origSize);
        return out;
    }
}

// Diagnostic stubs
namespace Diagnostics {
    void error(const std::string& title, const std::string& message) {
        std::cerr << "[ERROR] " << title << ": " << message << std::endl;
    }
}

// RLE-based compression
namespace brutal {
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        if (data.empty()) return {};
        std::vector<uint8_t> out;
        out.reserve(data.size());
        
        size_t i = 0;
        while (i < data.size()) {
            uint8_t val = data[i];
            uint8_t count = 1;
            while (count < 255 && i + count < data.size() && data[i + count] == val) {
                count++;
            }
            if (count >= 3) {
                out.push_back(0xFF); // RLE marker
                out.push_back(count);
                out.push_back(val);
                i += count;
            } else {
                // Literal: if byte is 0xFF, escape it
                if (val == 0xFF) {
                    out.push_back(0xFF);
                    out.push_back(1);
                    out.push_back(0xFF);
                } else {
                    out.push_back(val);
                }
                i++;
            }
        }
        return out;
    }
    std::vector<uint8_t> decompress(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> out;
        out.reserve(data.size() * 2);
        
        size_t i = 0;
        while (i < data.size()) {
            if (data[i] == 0xFF && i + 2 < data.size()) {
                uint8_t count = data[i + 1];
                uint8_t val = data[i + 2];
                for (uint8_t j = 0; j < count; j++) {
                    out.push_back(val);
                }
                i += 3;
            } else {
                out.push_back(data[i]);
                i++;
            }
        }
        return out;
    }
}

// Forward-declare kernel registration entries
extern void InferenceKernels_Register();

void register_sovereign_engines() {
    // Register all available compute kernels
    // This is called once at startup to populate the kernel dispatch table
    InferenceKernels_Register();
}

// Default registration (no-op if engine headers aren't linked)
void __attribute__((weak)) InferenceKernels_Register() {}
