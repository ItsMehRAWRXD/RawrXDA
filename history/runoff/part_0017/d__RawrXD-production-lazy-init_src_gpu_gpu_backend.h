#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <string>

namespace RawrXD {
namespace GPU {

// Data types for GPU computations
enum class DataType {
    F32,    // 32-bit float
    F16,    // 16-bit float
    Q8_0,   // 8-bit quantized
    Q4_0,   // 4-bit quantized
    Q2_K,   // 2-bit quantized
    COUNT
};

// Memory buffer on GPU
class Buffer {
public:
    virtual ~Buffer() = default;
    virtual size_t size() const = 0;
    virtual DataType dtype() const = 0;
    virtual void* device_ptr() = 0;
    virtual const void* device_ptr() const {
        return const_cast<Buffer*>(this)->device_ptr();
    }
};

// Compute kernel interface
class Kernel {
public:
    virtual ~Kernel() = default;
    virtual const char* name() const = 0;
    virtual bool launch(void* stream, const void* args, size_t arg_size) = 0;
};

// Main GPU backend interface
class Backend {
public:
    virtual ~Backend() = default;
    
    // Backend info
    virtual const char* name() const = 0;
    virtual const char* vendor() const = 0;
    virtual int device_count() const = 0;
    
    // Memory management
    virtual std::unique_ptr<Buffer> allocate(size_t size, DataType dtype) = 0;
    virtual void copy_to_device(Buffer* dst, const void* src, size_t size) = 0;
    virtual void copy_to_host(void* dst, const Buffer* src, size_t size) = 0;
    
    // Kernel management
    virtual std::unique_ptr<Kernel> create_kernel(const std::string& name) = 0;
    
    // Synchronization
    virtual void synchronize() = 0;
    
    // Matrix operations (core operations for inference)
    virtual void mat_mul(const Buffer* A, const Buffer* B, Buffer* C, 
                        size_t M, size_t N, size_t K, bool transA, bool transB) = 0;
    
    // Activation functions
    virtual void relu(Buffer* x, size_t size) = 0;
    virtual void gelu(Buffer* x, size_t size) = 0;
    virtual void silu(Buffer* x, size_t size) = 0;
    
    // Normalization
    virtual void layer_norm(Buffer* x, const Buffer* weight, const Buffer* bias,
                           size_t rows, size_t cols, float epsilon) = 0;
    virtual void rms_norm(Buffer* x, const Buffer* weight,
                         size_t rows, size_t cols, float epsilon) = 0;
    
    // Attention operations
    virtual void softmax(Buffer* x, size_t rows, size_t cols) = 0;
    virtual void rope(Buffer* x, size_t rows, size_t cols, size_t pos, float theta) = 0;
};

// Factory function to create appropriate backend
std::unique_ptr<Backend> create_backend(const std::string& preference = "auto");

// Global backend instance
Backend* get_global_backend();
void set_global_backend(std::unique_ptr<Backend> backend);

} // namespace GPU
} // namespace RawrXD