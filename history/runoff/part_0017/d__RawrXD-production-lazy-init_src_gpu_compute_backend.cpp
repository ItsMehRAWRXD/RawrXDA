#include "gpu_backend.h"
#include <iostream>
#include <cstring>
#include <cmath>

namespace RawrXD {
namespace GPU {

// CPU fallback backend for when no GPU is available
class CPUBackend : public Backend {
public:
    const char* name() const override { return "CPU"; }
    const char* vendor() const override { return "Generic"; }
    int device_count() const override { return 1; }
    
    std::unique_ptr<Buffer> allocate(size_t size, DataType dtype) override {
        return std::make_unique<CPUBuffer>(size, dtype);
    }
    
    void copy_to_device(Buffer* dst, const void* src, size_t size) override {
        // For CPU backend, "device" is just host memory
        std::memcpy(dst->device_ptr(), src, size);
    }
    
    void copy_to_host(void* dst, const Buffer* src, size_t size) override {
        std::memcpy(dst, src->device_ptr(), size);
    }
    
    std::unique_ptr<Kernel> create_kernel(const std::string& name) override {
        return std::make_unique<CPUKernel>(name);
    }
    
    void synchronize() override {
        // No-op for CPU
    }
    
    void mat_mul(const Buffer* A, const Buffer* B, Buffer* C,
                size_t M, size_t N, size_t K, bool transA, bool transB) override {
        cpu_mat_mul(A, B, C, M, N, K, transA, transB);
    }
    
    void relu(Buffer* x, size_t size) override {
        float* data = static_cast<float*>(x->device_ptr());
        for (size_t i = 0; i < size; ++i) {
            data[i] = std::max(0.0f, data[i]);
        }
    }
    
    void gelu(Buffer* x, size_t size) override {
        float* data = static_cast<float*>(x->device_ptr());
        for (size_t i = 0; i < size; ++i) {
            data[i] = gelu_impl(data[i]);
        }
    }
    
    void silu(Buffer* x, size_t size) override {
        float* data = static_cast<float*>(x->device_ptr());
        for (size_t i = 0; i < size; ++i) {
            data[i] = silu_impl(data[i]);
        }
    }
    
    void layer_norm(Buffer* x, const Buffer* weight, const Buffer* bias,
                   size_t rows, size_t cols, float epsilon) override {
        cpu_layer_norm(x, weight, bias, rows, cols, epsilon);
    }
    
    void rms_norm(Buffer* x, const Buffer* weight,
                 size_t rows, size_t cols, float epsilon) override {
        cpu_rms_norm(x, weight, rows, cols, epsilon);
    }
    
    void softmax(Buffer* x, size_t rows, size_t cols) override {
        cpu_softmax(x, rows, cols);
    }
    
    void rope(Buffer* x, size_t rows, size_t cols, size_t pos, float theta) override {
        cpu_rope(x, rows, cols, pos, theta);
    }

private:
    // CPU buffer implementation
    struct CPUBuffer : public Buffer {
        CPUBuffer(size_t size, DataType dtype) 
            : size_(size), dtype_(dtype), data_(new uint8_t[size]) {}
        
        ~CPUBuffer() { delete[] data_; }
        
        size_t size() const override { return size_; }
        DataType dtype() const override { return dtype_; }
        void* device_ptr() override { return data_; }
        const void* device_ptr() const override { return data_; }
        
    private:
        size_t size_;
        DataType dtype_;
        uint8_t* data_;
    };
    
    // CPU kernel implementation
    struct CPUKernel : public Kernel {
        CPUKernel(const std::string& name) : name_(name) {}
        const char* name() const override { return name_.c_str(); }
        bool launch(void* stream, const void* args, size_t arg_size) override {
            // For CPU, just execute the kernel function directly
            std::cout << "[CPUKernel] Executing kernel: " << name_ << std::endl;
            return true;
        }
        std::string name_;
    };
    
    static float gelu_impl(float x) {
        // GELU approximation: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
        const float sqrt_2_over_pi = 0.7978845608f;
        const float coeff = 0.044715f;
        float x3 = x * x * x;
        return 0.5f * x * (1.0f + std::tanh(sqrt_2_over_pi * (x + coeff * x3)));
    }
    
    static float silu_impl(float x) {
        // SiLU: x * sigmoid(x)
        return x / (1.0f + std::exp(-x));
    }
    
    static void cpu_mat_mul(const Buffer* A, const Buffer* B, Buffer* C,
                           size_t M, size_t N, size_t K, bool transA, bool transB) {
        const float* a = static_cast<const float*>(A->device_ptr());
        const float* b = static_cast<const float*>(B->device_ptr());
        float* c = static_cast<float*>(C->device_ptr());
        
        // Simple CPU matrix multiplication
        for (size_t i = 0; i < M; ++i) {
            for (size_t j = 0; j < N; ++j) {
                float sum = 0.0f;
                for (size_t k = 0; k < K; ++k) {
                    size_t a_idx = transA ? (k * M + i) : (i * K + k);
                    size_t b_idx = transB ? (j * K + k) : (k * N + j);
                    sum += a[a_idx] * b[b_idx];
                }
                c[i * N + j] = sum;
            }
        }
    }
    
    static void cpu_layer_norm(Buffer* x, const Buffer* weight, const Buffer* bias,
                              size_t rows, size_t cols, float epsilon) {
        float* data = static_cast<float*>(x->device_ptr());
        const float* w = weight ? static_cast<const float*>(weight->device_ptr()) : nullptr;
        const float* b = bias ? static_cast<const float*>(bias->device_ptr()) : nullptr;
        
        for (size_t i = 0; i < rows; ++i) {
            float* row = data + i * cols;
            
            // Compute mean
            float mean = 0.0f;
            for (size_t j = 0; j < cols; ++j) {
                mean += row[j];
            }
            mean /= cols;
            
            // Compute variance
            float variance = 0.0f;
            for (size_t j = 0; j < cols; ++j) {
                float diff = row[j] - mean;
                variance += diff * diff;
            }
            variance /= cols;
            
            // Normalize
            float inv_std = 1.0f / std::sqrt(variance + epsilon);
            for (size_t j = 0; j < cols; ++j) {
                row[j] = (row[j] - mean) * inv_std;
                if (w) row[j] *= w[j];
                if (b) row[j] += b[j];
            }
        }
    }
    
    static void cpu_rms_norm(Buffer* x, const Buffer* weight,
                            size_t rows, size_t cols, float epsilon) {
        float* data = static_cast<float*>(x->device_ptr());
        const float* w = weight ? static_cast<const float*>(weight->device_ptr()) : nullptr;
        
        for (size_t i = 0; i < rows; ++i) {
            float* row = data + i * cols;
            
            // Compute RMS
            float sum_sq = 0.0f;
            for (size_t j = 0; j < cols; ++j) {
                sum_sq += row[j] * row[j];
            }
            float rms = std::sqrt(sum_sq / cols + epsilon);
            float inv_rms = 1.0f / rms;
            
            // Normalize
            for (size_t j = 0; j < cols; ++j) {
                row[j] = row[j] * inv_rms;
                if (w) row[j] *= w[j];
            }
        }
    }
    
    static void cpu_softmax(Buffer* x, size_t rows, size_t cols) {
        float* data = static_cast<float*>(x->device_ptr());
        
        for (size_t i = 0; i < rows; ++i) {
            float* row = data + i * cols;
            
            // Find max for numerical stability
            float max_val = row[0];
            for (size_t j = 1; j < cols; ++j) {
                if (row[j] > max_val) max_val = row[j];
            }
            
            // Compute exp and sum
            float sum = 0.0f;
            for (size_t j = 0; j < cols; ++j) {
                row[j] = std::exp(row[j] - max_val);
                sum += row[j];
            }
            
            // Normalize
            float inv_sum = 1.0f / sum;
            for (size_t j = 0; j < cols; ++j) {
                row[j] *= inv_sum;
            }
        }
    }
    
    static void cpu_rope(Buffer* x, size_t rows, size_t cols, size_t pos, float theta) {
        float* data = static_cast<float*>(x->device_ptr());
        
        for (size_t i = 0; i < rows; ++i) {
            float* row = data + i * cols;
            
            for (size_t j = 0; j < cols; j += 2) {
                float freq = 1.0f / std::pow(theta, float(j) / cols);
                float angle = pos * freq;
                
                float cos_val = std::cos(angle);
                float sin_val = std::sin(angle);
                
                float x0 = row[j];
                float x1 = row[j + 1];
                
                row[j] = x0 * cos_val - x1 * sin_val;
                row[j + 1] = x0 * sin_val + x1 * cos_val;
            }
        }
    }
};

// Factory function
std::unique_ptr<Backend> create_backend(const std::string& preference) {
    // For now, always return CPU backend
    // In the future, we can add ROCm, CUDA, etc. backends here
    return std::make_unique<CPUBackend>();
}

// Global backend instance
static std::unique_ptr<Backend> g_global_backend;

Backend* get_global_backend() {
    if (!g_global_backend) {
        g_global_backend = create_backend();
    }
    return g_global_backend.get();
}

void set_global_backend(std::unique_ptr<Backend> backend) {
    g_global_backend = std::move(backend);
}

} // namespace GPU
} // namespace RawrXD