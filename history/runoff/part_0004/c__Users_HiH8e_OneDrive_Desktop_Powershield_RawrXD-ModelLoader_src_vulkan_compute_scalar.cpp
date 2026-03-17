#include "vulkan_compute.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <array>

// SCALAR-ONLY IMPLEMENTATION: All GPU/Vulkan code removed - pure CPU scalar operations

VulkanCompute::VulkanCompute() {
    std::memset(&device_info_, 0, sizeof(VulkanDeviceInfo));
    device_info_.device_name = "Scalar CPU (No GPU)";
    device_info_.vendor_id = 0x0000;
    device_info_.device_id = 0x0000;
    device_info_.supports_compute = true;
    device_info_.compute_queue_family = 0;
}

VulkanCompute::~VulkanCompute() {
    Cleanup();
}

bool VulkanCompute::Initialize() {
    std::cout << "Scalar CPU compute initialized (no GPU)" << std::endl;
    std::cout << "Device: " << device_info_.device_name << std::endl;
    return true;
}

bool VulkanCompute::CreateInstance() {
    return true;  // Scalar mode: no Vulkan instance needed
}

bool VulkanCompute::SelectPhysicalDevice() {
    return true;  // Scalar mode: using CPU only
}

bool VulkanCompute::CreateLogicalDevice() {
    return true;  // Scalar mode: no logical device needed
}

bool VulkanCompute::CreateCommandPool() {
    return true;  // Scalar mode: no command pool needed
}

bool VulkanCompute::LoadShader(const std::string& name, const std::string& spirv_path) {
    ComputeShader shader;
    shader.name = name;
    shaders_[name] = shader;
    std::cout << "Scalar mode: Shader '" << name << "' registered (not loaded)" << std::endl;
    return true;
}

bool VulkanCompute::CreateComputePipeline(const std::string& shader_name) {
    std::cout << "Scalar mode: Pipeline '" << shader_name << "' created (CPU-only)" << std::endl;
    return true;
}

VulkanTensor VulkanCompute::TransferGGUFTensor(const std::string& tensor_name,
                                                const void* data_ptr,
                                                size_t size_bytes,
                                                uint32_t usage) {
    VulkanTensor tensor;
    tensor.name = tensor_name;
    tensor.size_bytes = size_bytes;
    
    // Copy data to host memory (scalar CPU storage)
    if (data_ptr && size_bytes > 0) {
        size_t num_floats = size_bytes / sizeof(float);
        tensor.host_data.resize(num_floats);
        std::memcpy(tensor.host_data.data(), data_ptr, size_bytes);
    }
    
    uploaded_tensors_.push_back(tensor);
    std::cout << "Scalar mode: Tensor '" << tensor_name << "' transferred to CPU memory (" 
              << size_bytes << " bytes)" << std::endl;
    return tensor;
}

void VulkanCompute::ReleaseTensors() {
    uploaded_tensors_.clear();
    std::cout << "Scalar mode: All tensors released from CPU memory" << std::endl;
}

bool VulkanCompute::EnsureMatMulPipeline(const std::string& spirv_path) {
    std::cout << "Scalar mode: MatMul pipeline ensured (CPU-only)" << std::endl;
    return true;
}

bool VulkanCompute::DispatchMatMul(uint32_t input_a_idx,
                                   uint32_t input_b_idx,
                                   uint32_t output_idx,
                                   uint32_t M,
                                   uint32_t K,
                                   uint32_t N) {
    if (input_a_idx >= uploaded_tensors_.size() || 
        input_b_idx >= uploaded_tensors_.size() || 
        output_idx >= uploaded_tensors_.size()) {
        std::cerr << "Invalid tensor indices for MatMul" << std::endl;
        return false;
    }
    
    const float* A = uploaded_tensors_[input_a_idx].host_data.data();
    const float* B = uploaded_tensors_[input_b_idx].host_data.data();
    float* C = uploaded_tensors_[output_idx].host_data.data();
    
    return ExecuteMatMul(A, B, C, M, K, N);
}

bool VulkanCompute::AllocateBuffer(size_t size, uint32_t& buffer_idx, size_t& memory_size) {
    VulkanTensor tensor;
    tensor.name = "buffer_" + std::to_string(uploaded_tensors_.size());
    tensor.size_bytes = size;
    tensor.host_data.resize(size / sizeof(float));
    
    buffer_idx = static_cast<uint32_t>(uploaded_tensors_.size());
    memory_size = size;
    uploaded_tensors_.push_back(tensor);
    
    std::cout << "Scalar mode: Allocated CPU buffer " << buffer_idx << " (" << size << " bytes)" << std::endl;
    return true;
}

bool VulkanCompute::CopyBufferToHost(uint32_t buffer_idx, void* host_data, size_t size) {
    if (buffer_idx >= uploaded_tensors_.size()) {
        std::cerr << "Invalid buffer index" << std::endl;
        return false;
    }
    
    std::memcpy(host_data, uploaded_tensors_[buffer_idx].host_data.data(), size);
    return true;
}

bool VulkanCompute::CopyHostToBuffer(void* host_data, uint32_t buffer_idx, size_t size) {
    if (buffer_idx >= uploaded_tensors_.size()) {
        std::cerr << "Invalid buffer index" << std::endl;
        return false;
    }
    
    std::memcpy(uploaded_tensors_[buffer_idx].host_data.data(), host_data, size);
    return true;
}

bool VulkanCompute::ExecuteMatMul(const float* input_a, const float* input_b, 
                                  float* output, uint32_t m, uint32_t k, uint32_t n) {
    if (!input_a || !input_b || !output) return false;
    
    // Scalar CPU matrix multiplication: C[m][n] = A[m][k] * B[k][n]
    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            float sum = 0.0f;
            for (uint32_t p = 0; p < k; ++p) {
                sum += input_a[i * k + p] * input_b[p * n + j];  // Explicit scalar multiply-add
            }
            output[i * n + j] = sum;
        }
    }
    return true;
}

bool VulkanCompute::ExecuteAttention(const float* queries, const float* keys, const float* values,
                                     float* output, uint32_t seq_len, uint32_t head_dim) {
    if (!queries || !keys || !values || !output) return false;
    
    // Scalar CPU attention: Q*K^T / sqrt(d), softmax, then * V
    std::vector<float> scores(seq_len * seq_len);
    float scale = 1.0f / std::sqrt(static_cast<float>(head_dim));
    
    // Compute Q*K^T and scale
    for (uint32_t i = 0; i < seq_len; ++i) {
        for (uint32_t j = 0; j < seq_len; ++j) {
            float sum = 0.0f;
            for (uint32_t d = 0; d < head_dim; ++d) {
                sum += queries[i * head_dim + d] * keys[j * head_dim + d];  // Scalar dot product
            }
            scores[i * seq_len + j] = sum * scale;
        }
    }
    
    // Softmax per row (scalar)
    for (uint32_t i = 0; i < seq_len; ++i) {
        float max_val = scores[i * seq_len];
        for (uint32_t j = 1; j < seq_len; ++j) {
            max_val = std::max(max_val, scores[i * seq_len + j]);
        }
        
        float sum = 0.0f;
        for (uint32_t j = 0; j < seq_len; ++j) {
            scores[i * seq_len + j] = std::exp(scores[i * seq_len + j] - max_val);  // Scalar exp
            sum += scores[i * seq_len + j];
        }
        
        for (uint32_t j = 0; j < seq_len; ++j) {
            scores[i * seq_len + j] /= sum;  // Scalar division
        }
    }
    
    // Multiply by V (scalar)
    for (uint32_t i = 0; i < seq_len; ++i) {
        for (uint32_t d = 0; d < head_dim; ++d) {
            float sum = 0.0f;
            for (uint32_t j = 0; j < seq_len; ++j) {
                sum += scores[i * seq_len + j] * values[j * head_dim + d];  // Scalar multiply-add
            }
            output[i * head_dim + d] = sum;
        }
    }
    
    return true;
}

bool VulkanCompute::ExecuteRoPE(float* embeddings, uint32_t dim, uint32_t seq_pos, uint32_t rotation_dim) {
    if (!embeddings) return false;
    
    // Scalar RoPE (Rotary Position Embedding)
    for (uint32_t i = 0; i < rotation_dim / 2; ++i) {
        float theta = seq_pos / std::pow(10000.0f, 2.0f * i / rotation_dim);
        float cos_theta = std::cos(theta);
        float sin_theta = std::sin(theta);
        
        float x0 = embeddings[2 * i];
        float x1 = embeddings[2 * i + 1];
        
        embeddings[2 * i] = x0 * cos_theta - x1 * sin_theta;      // Scalar operations
        embeddings[2 * i + 1] = x0 * sin_theta + x1 * cos_theta;  // Scalar operations
    }
    
    return true;
}

bool VulkanCompute::ExecuteRMSNorm(float* data, uint32_t size, float epsilon) {
    if (!data) return false;
    
    // Scalar RMSNorm: y = x / sqrt(mean(x^2) + eps)
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        sum_sq += static_cast<double>(data[i]) * static_cast<double>(data[i]);  // Scalar square
    }
    
    double mean_sq = sum_sq / std::max<uint32_t>(size, 1);
    double rms = std::sqrt(mean_sq + static_cast<double>(epsilon));
    
    if (rms == 0.0) rms = 1.0;  // Safety
    float inv_rms = static_cast<float>(1.0 / rms);
    
    for (uint32_t i = 0; i < size; ++i) {
        data[i] *= inv_rms;  // Scalar multiplication
    }
    
    return true;
}

bool VulkanCompute::ExecuteSiLU(float* data, uint32_t size) {
    if (!data) return false;
    
    // Scalar SiLU: x * sigmoid(x)
    for (uint32_t i = 0; i < size; ++i) {
        float x = data[i];
        float sigmoid = 1.0f / (1.0f + std::exp(-x));  // Scalar exp and division
        data[i] = x * sigmoid;  // Scalar multiplication
    }
    
    return true;
}

bool VulkanCompute::ExecuteSoftmax(float* data, uint32_t size) {
    if (!data || size == 0) return true;
    
    // Scalar Softmax
    float max_val = data[0];
    for (uint32_t i = 1; i < size; ++i) {
        max_val = std::max(max_val, data[i]);  // Scalar max
    }
    
    double sum = 0.0;
    for (uint32_t i = 0; i < size; ++i) {
        data[i] = std::exp(data[i] - max_val);  // Scalar exp
        sum += data[i];
    }
    
    if (sum == 0.0) return true;
    double inv_sum = 1.0 / sum;
    
    for (uint32_t i = 0; i < size; ++i) {
        data[i] = static_cast<float>(data[i] * inv_sum);  // Scalar division
    }
    
    return true;
}

bool VulkanCompute::ExecuteDequantize(const uint8_t* quantized, float* output,
                                      uint32_t elements, const std::string& quant_type) {
    if (!quantized || !output || elements == 0) return false;
    
    // Scalar dequantization based on type
    if (quant_type == "F32") {
        const float* src = reinterpret_cast<const float*>(quantized);
        for (uint32_t i = 0; i < elements; ++i) {
            output[i] = src[i];  // Scalar copy
        }
        return true;
    } 
    else if (quant_type == "Q2_K") {
        // 2-bit values packed (4 per byte) - scalar unpacking
        uint32_t out_idx = 0;
        for (uint32_t b = 0; out_idx < elements; ++b) {
            uint8_t packed = quantized[b];
            for (int shift = 0; shift < 8 && out_idx < elements; shift += 2) {
                uint8_t val = (packed >> shift) & 0x3;
                output[out_idx++] = (val / 3.0f) * 2.0f - 1.0f;  // Scalar conversion
            }
        }
        return true;
    } 
    else if (quant_type == "Q4_K") {
        // 4-bit values packed (2 per byte) - scalar unpacking
        uint32_t out_idx = 0;
        for (uint32_t b = 0; out_idx < elements; ++b) {
            uint8_t packed = quantized[b];
            uint8_t lo = packed & 0xF;
            uint8_t hi = (packed >> 4) & 0xF;
            
            output[out_idx++] = (lo / 15.0f) * 2.0f - 1.0f;  // Scalar conversion
            if (out_idx < elements) {
                output[out_idx++] = (hi / 15.0f) * 2.0f - 1.0f;  // Scalar conversion
            }
        }
        return true;
    } 
    else {
        // Fallback: byte to [0,1] scalar conversion
        const float scale = 1.0f / 255.0f;
        for (uint32_t i = 0; i < elements; ++i) {
            output[i] = quantized[i] * scale;  // Scalar multiplication
        }
        return true;
    }
}

bool VulkanCompute::LoadSPIRVCode(const std::string& path, std::vector<uint32_t>& code) {
    std::cout << "Scalar mode: SPIR-V loading skipped (CPU-only)" << std::endl;
    return true;  // Scalar mode doesn't use SPIR-V
}

uint32_t VulkanCompute::FindMemoryType(uint32_t type_filter, uint32_t properties) {
    return 0;  // Scalar mode: no GPU memory types
}

void VulkanCompute::Cleanup() {
    uploaded_tensors_.clear();
    shaders_.clear();
    std::cout << "Scalar mode: Cleanup completed" << std::endl;
}
