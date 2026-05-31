/*
====================================================================
 RAWR ZERO-COPY TENSOR ACCESSOR
 Direct views into mmap'd GGUF memory - no copies
====================================================================
*/

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace rawr {

// Tensor view - non-owning reference to mmap'd memory
struct TensorView {
    const void* data = nullptr;
    size_t size_bytes = 0;
    uint32_t n_dims = 0;
    size_t dims[4] = {0, 0, 0, 0};
    size_t stride[4] = {0, 0, 0, 0};
    uint32_t type = 0;  // GGUF type
    bool is_transposed = false;
    
    // Get element size based on GGUF type
    size_t element_size() const {
        switch (type) {
            case 0:  return 4;    // F32
            case 1:  return 2;    // F16
            case 2:  return 1;    // Q4_0
            case 3:  return 2;    // Q4_1
            case 12: return 4;    // Q4_K
            case 13: return 6;    // Q5_K
            case 14: return 8;    // Q6_K
            default: return 4;
        }
    }
    
    // Get pointer to element at indices (no bounds check)
    template<typename T>
    const T* ptr_at(size_t i0, size_t i1 = 0, size_t i2 = 0, size_t i3 = 0) const {
        size_t offset = i0 * stride[0] + i1 * stride[1] + i2 * stride[2] + i3 * stride[3];
        return reinterpret_cast<const T*>(static_cast<const uint8_t*>(data) + offset);
    }
    
    // Access float element (for F32 tensors)
    float f32_at(size_t i0, size_t i1 = 0) const {
        if (type != 0) return 0.0f;  // Only F32
        return *ptr_at<float>(i0, i1);
    }
    
    // Check if view is valid
    bool valid() const { return data != nullptr; }
    
    // Total number of elements
    size_t num_elements() const {
        size_t n = 1;
        for (uint32_t i = 0; i < n_dims; i++) n *= dims[i];
        return n;
    }
};

// Zero-copy tensor accessor for GGUF
class ZeroCopyTensorAccessor {
public:
    // Create view from GGUF tensor info
    static TensorView create_view(const void* gguf_data, 
                                   uint64_t offset,
                                   uint32_t type,
                                   uint32_t n_dims,
                                   const uint64_t* dims,
                                   bool detect_transpose = true) {
        TensorView view;
        view.data = static_cast<const uint8_t*>(gguf_data) + offset;
        view.type = type;
        view.n_dims = n_dims;
        
        size_t elem_size = view.element_size();
        
        // Copy dimensions
        for (uint32_t i = 0; i < n_dims && i < 4; i++) {
            view.dims[i] = dims[i];
        }
        
        // Calculate strides (row-major default)
        if (n_dims >= 2) {
            view.stride[0] = dims[1] * elem_size;  // Row stride
            view.stride[1] = elem_size;             // Column stride
            
            // Detect transposed weight matrices
            if (detect_transpose && dims[0] < dims[1]) {
                view.is_transposed = true;
            }
        } else if (n_dims == 1) {
            view.stride[0] = elem_size;
        }
        
        // Calculate total size
        view.size_bytes = view.num_elements() * elem_size;
        
        return view;
    }
    
    // Access row as contiguous slice (for matrix multiplication)
    template<typename T>
    static const T* get_row(const TensorView& view, size_t row) {
        if (view.n_dims < 2) return nullptr;
        return reinterpret_cast<const T*>(
            static_cast<const uint8_t*>(view.data) + row * view.stride[0]
        );
    }
    
    // Prefetch a range of rows (for cache optimization)
    static void prefetch_rows(const TensorView& view, size_t start_row, size_t num_rows) {
        if (!view.valid() || view.n_dims < 2) return;
        
        const char* start = static_cast<const char*>(view.data) + start_row * view.stride[0];
        size_t size = num_rows * view.stride[0];
        
        // Platform-specific prefetch
        #if defined(__x86_64__)
            for (size_t i = 0; i < size; i += 64) {
                __builtin_prefetch(start + i, 0, 3);
            }
        #elif defined(_WIN32)
            // Windows prefetch via VirtualAlloc hint not available for mapped files
            // Use madvise equivalent via Win32 API if needed
        #endif
    }
};

// Quantized tensor dequantization (on-the-fly)
class QuantizedTensorAccess {
public:
    // Dequantize Q4_0 block to float
    static void dequantize_q4_0(const void* block_data, float* out, size_t n) {
        // Q4_0: 16 bytes for 32 weights
        // Layout: [scale (f16)] [qs (32 x 4-bit)]
        const uint16_t* scale_ptr = static_cast<const uint16_t*>(block_data);
        const uint8_t* qs = static_cast<const uint8_t*>(block_data) + 2;
        
        // Decode half-precision scale
        float scale = fp16_to_fp32(*scale_ptr);
        
        for (size_t i = 0; i < n && i < 32; i++) {
            uint8_t q = (qs[i / 2] >> (4 * (i % 2))) & 0x0F;
            out[i] = (q - 8) * scale;  // Convert to signed and scale
        }
    }
    
    // Dequantize Q4_K block to float
    static void dequantize_q4_k(const void* block_data, float* out, size_t n) {
        // Q4_K: Complex block structure
        // Simplified: just zero for now (implement full spec later)
        for (size_t i = 0; i < n; i++) out[i] = 0.0f;
    }
    
private:
    static float fp16_to_fp32(uint16_t h) {
        // Simple FP16 to FP32 conversion
        uint32_t sign = (h >> 15) & 0x1;
        uint32_t exp = (h >> 10) & 0x1F;
        uint32_t mant = h & 0x3FF;
        
        if (exp == 0) {
            if (mant == 0) return sign ? -0.0f : 0.0f;
            return (sign ? -1.0f : 1.0f) * ldexpf(mant / 1024.0f, -14);
        }
        if (exp == 31) {
            if (mant) return NAN;
            return sign ? -INFINITY : INFINITY;
        }
        
        uint32_t f32 = (sign << 31) | ((exp + 112) << 23) | (mant << 13);
        float result;
        memcpy(&result, &f32, 4);
        return result;
    }
};

} // namespace rawr
