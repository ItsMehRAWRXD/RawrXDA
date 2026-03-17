#include "vulkan_inference_engine.h"
#include <iostream>
#include <sstream>
#include <algorithm>

VulkanInferenceEngine::VulkanInferenceEngine()
    : is_initialized_(false), gpu_available_(false), next_handle_(1) {
    vulkan_compute_ = std::make_unique<VulkanCompute>();
}

VulkanInferenceEngine::~VulkanInferenceEngine() {
    Shutdown();
}

bool VulkanInferenceEngine::Initialize() {
    if (is_initialized_) {
        return gpu_available_;
    }

    // Attempt to initialize Vulkan
    if (!vulkan_compute_->Initialize()) {
        std::cerr << "[VulkanInferenceEngine] Failed to initialize VulkanCompute" << std::endl;
        gpu_available_ = false;
        is_initialized_ = true;
        return false;
    }

    auto device_info = vulkan_compute_->GetDeviceInfo();
    std::cout << "[VulkanInferenceEngine] Successfully initialized Vulkan GPU" << std::endl;
    std::cout << "[VulkanInferenceEngine] GPU Device: " << device_info.device_name << std::endl;
    std::cout << "[VulkanInferenceEngine] Vendor ID: 0x" << std::hex << device_info.vendor_id << std::dec << std::endl;
    
    if (vulkan_compute_->IsAMDDevice()) {
        std::cout << "[VulkanInferenceEngine] Detected AMD GPU - RDNA architecture" << std::endl;
    } else if (vulkan_compute_->IsNvidiaDevice()) {
        std::cout << "[VulkanInferenceEngine] Detected NVIDIA GPU" << std::endl;
    } else {
        std::cout << "[VulkanInferenceEngine] Detected Intel or other GPU" << std::endl;
    }

    gpu_available_ = true;
    is_initialized_ = true;
    return true;
}

bool VulkanInferenceEngine::IsGPUAvailable() const {
    return gpu_available_;
}

VulkanDeviceInfo VulkanInferenceEngine::GetGPUInfo() const {
    if (vulkan_compute_) {
        return vulkan_compute_->GetDeviceInfo();
    }
    return VulkanDeviceInfo{};
}

uint32_t VulkanInferenceEngine::LoadTensorToGPU(const std::string& tensor_name,
                                                const void* data_ptr,
                                                size_t size_bytes) {
    if (!gpu_available_ || !vulkan_compute_) {
        return 0;
    }

    uint32_t buffer_idx;
    size_t allocated_size;
    if (!vulkan_compute_->AllocateBuffer(size_bytes, buffer_idx, allocated_size)) {
        std::cerr << "[VulkanInferenceEngine] Failed to allocate GPU buffer for tensor: " << tensor_name << std::endl;
        return 0;
    }

    if (!vulkan_compute_->CopyHostToBuffer((void*)data_ptr, buffer_idx, size_bytes)) {
        std::cerr << "[VulkanInferenceEngine] Failed to copy tensor to GPU: " << tensor_name << std::endl;
        return 0;
    }

    uint32_t handle = AllocateHandle();
    loaded_tensors_.push_back({tensor_name, size_bytes, buffer_idx});
    
    std::cout << "[VulkanInferenceEngine] Loaded tensor '" << tensor_name 
              << "' to GPU (" << (size_bytes / 1024 / 1024) << "MB)" << std::endl;
    
    return handle;
}

bool VulkanInferenceEngine::CopyTensorFromGPU(uint32_t gpu_handle, void* output_ptr, size_t size_bytes) {
    if (!gpu_available_ || !vulkan_compute_) {
        return false;
    }

    // Find the tensor by handle
    if (gpu_handle >= loaded_tensors_.size() || gpu_handle == 0) {
        std::cerr << "[VulkanInferenceEngine] Invalid GPU tensor handle: " << gpu_handle << std::endl;
        return false;
    }

    const auto& tensor = loaded_tensors_[gpu_handle - 1];
    if (size_bytes > tensor.size_bytes) {
        std::cerr << "[VulkanInferenceEngine] Copy size (" << size_bytes 
                  << ") exceeds tensor size (" << tensor.size_bytes << ")" << std::endl;
        return false;
    }

    return vulkan_compute_->CopyBufferToHost(tensor.gpu_buffer_idx, output_ptr, size_bytes);
}

uint32_t VulkanInferenceEngine::MatMulGPU(uint32_t a_handle, uint32_t b_handle, 
                                          uint32_t m, uint32_t k, uint32_t n) {
    if (!gpu_available_) {
        return 0;
    }

    if (a_handle > loaded_tensors_.size() || b_handle > loaded_tensors_.size()) {
        std::cerr << "[VulkanInferenceEngine] Invalid matrix handles for MatMul" << std::endl;
        return 0;
    }

    uint32_t output_buffer_idx;
    size_t output_size;
    
    // Allocate output buffer: M x N matrix
    if (!vulkan_compute_->AllocateBuffer(m * n * sizeof(float), output_buffer_idx, output_size)) {
        std::cerr << "[VulkanInferenceEngine] Failed to allocate output buffer for MatMul" << std::endl;
        return 0;
    }

    auto& a_tensor = loaded_tensors_[a_handle - 1];
    auto& b_tensor = loaded_tensors_[b_handle - 1];

    if (!vulkan_compute_->DispatchMatMul(a_tensor.gpu_buffer_idx, b_tensor.gpu_buffer_idx,
                                         output_buffer_idx, m, k, n)) {
        std::cerr << "[VulkanInferenceEngine] MatMul dispatch failed" << std::endl;
        return 0;
    }

    uint32_t result_handle = AllocateHandle();
    loaded_tensors_.push_back({"matmul_result_" + std::to_string(result_handle),
                               output_size, output_buffer_idx});
    return result_handle;
}

uint32_t VulkanInferenceEngine::MatMulGPUAsync(uint32_t a_handle, uint32_t b_handle,
                                               uint32_t m, uint32_t k, uint32_t n) {
    if (!gpu_available_) {
        return 0;
    }

    if (a_handle > loaded_tensors_.size() || b_handle > loaded_tensors_.size()) {
        std::cerr << "[VulkanInferenceEngine] Invalid matrix handles for async MatMul" << std::endl;
        return 0;
    }

    uint32_t output_buffer_idx;
    size_t output_size;
    
    if (!vulkan_compute_->AllocateBuffer(m * n * sizeof(float), output_buffer_idx, output_size)) {
        std::cerr << "[VulkanInferenceEngine] Failed to allocate output buffer for async MatMul" << std::endl;
        return 0;
    }

    auto& a_tensor = loaded_tensors_[a_handle - 1];
    auto& b_tensor = loaded_tensors_[b_handle - 1];

    if (!vulkan_compute_->DispatchMatMulAsync(a_tensor.gpu_buffer_idx, b_tensor.gpu_buffer_idx,
                                              output_buffer_idx, m, k, n)) {
        std::cerr << "[VulkanInferenceEngine] Async MatMul dispatch failed" << std::endl;
        return 0;
    }

    uint32_t result_handle = AllocateHandle();
    loaded_tensors_.push_back({"matmul_async_result_" + std::to_string(result_handle),
                               output_size, output_buffer_idx});
    return result_handle;
}

uint32_t VulkanInferenceEngine::RoPEGPU(uint32_t input_handle, uint32_t seq_pos,
                                        uint32_t dim, uint32_t rotation_dim) {
    if (!gpu_available_) return 0;
    
    if (input_handle > loaded_tensors_.size()) {
        std::cerr << "[VulkanInferenceEngine] Invalid input handle for RoPE" << std::endl;
        return 0;
    }

    auto& input_tensor = loaded_tensors_[input_handle - 1];
    uint32_t output_buffer_idx;
    size_t output_size;

    if (!vulkan_compute_->AllocateBuffer(input_tensor.size_bytes, output_buffer_idx, output_size)) {
        return 0;
    }

    if (!vulkan_compute_->DispatchRoPE(input_tensor.gpu_buffer_idx, output_buffer_idx,
                                       dim, seq_pos, rotation_dim)) {
        std::cerr << "[VulkanInferenceEngine] RoPE dispatch failed" << std::endl;
        return 0;
    }

    uint32_t result_handle = AllocateHandle();
    loaded_tensors_.push_back({"rope_result_" + std::to_string(result_handle),
                               output_size, output_buffer_idx});
    return result_handle;
}

uint32_t VulkanInferenceEngine::RMSNormGPU(uint32_t input_handle, uint32_t size, float epsilon) {
    if (!gpu_available_) return 0;
    
    if (input_handle > loaded_tensors_.size()) {
        return 0;
    }

    auto& input_tensor = loaded_tensors_[input_handle - 1];
    uint32_t output_buffer_idx;
    size_t output_size;

    if (!vulkan_compute_->AllocateBuffer(input_tensor.size_bytes, output_buffer_idx, output_size)) {
        return 0;
    }

    if (!vulkan_compute_->DispatchRMSNorm(input_tensor.gpu_buffer_idx, output_buffer_idx,
                                          size, epsilon)) {
        std::cerr << "[VulkanInferenceEngine] RMSNorm dispatch failed" << std::endl;
        return 0;
    }

    uint32_t result_handle = AllocateHandle();
    loaded_tensors_.push_back({"rmsnorm_result_" + std::to_string(result_handle),
                               output_size, output_buffer_idx});
    return result_handle;
}

uint32_t VulkanInferenceEngine::SiLUGPU(uint32_t input_handle, uint32_t size) {
    if (!gpu_available_) return 0;
    
    auto& input_tensor = loaded_tensors_[input_handle - 1];
    uint32_t output_buffer_idx;
    size_t output_size;

    if (!vulkan_compute_->AllocateBuffer(input_tensor.size_bytes, output_buffer_idx, output_size)) {
        return 0;
    }

    if (!vulkan_compute_->DispatchSiLU(input_tensor.gpu_buffer_idx, output_buffer_idx, size)) {
        std::cerr << "[VulkanInferenceEngine] SiLU dispatch failed" << std::endl;
        return 0;
    }

    uint32_t result_handle = AllocateHandle();
    loaded_tensors_.push_back({"silu_result_" + std::to_string(result_handle),
                               output_size, output_buffer_idx});
    return result_handle;
}

uint32_t VulkanInferenceEngine::SoftmaxGPU(uint32_t input_handle, uint32_t size) {
    if (!gpu_available_) return 0;
    
    auto& input_tensor = loaded_tensors_[input_handle - 1];
    uint32_t output_buffer_idx;
    size_t output_size;

    if (!vulkan_compute_->AllocateBuffer(input_tensor.size_bytes, output_buffer_idx, output_size)) {
        return 0;
    }

    if (!vulkan_compute_->DispatchSoftmax(input_tensor.gpu_buffer_idx, output_buffer_idx, size)) {
        std::cerr << "[VulkanInferenceEngine] Softmax dispatch failed" << std::endl;
        return 0;
    }

    uint32_t result_handle = AllocateHandle();
    loaded_tensors_.push_back({"softmax_result_" + std::to_string(result_handle),
                               output_size, output_buffer_idx});
    return result_handle;
}

uint32_t VulkanInferenceEngine::AttentionGPU(uint32_t q_handle, uint32_t k_handle, uint32_t v_handle,
                                             uint32_t seq_len, uint32_t head_dim, uint32_t num_heads) {
    if (!gpu_available_) return 0;
    
    auto& q_tensor = loaded_tensors_[q_handle - 1];
    uint32_t output_buffer_idx;
    size_t output_size;

    // Output size = batch * seq_len * num_heads * head_dim
    if (!vulkan_compute_->AllocateBuffer(q_tensor.size_bytes, output_buffer_idx, output_size)) {
        return 0;
    }

    if (!vulkan_compute_->DispatchAttention(q_tensor.gpu_buffer_idx, 
                                            loaded_tensors_[k_handle - 1].gpu_buffer_idx,
                                            loaded_tensors_[v_handle - 1].gpu_buffer_idx,
                                            output_buffer_idx, seq_len, head_dim)) {
        std::cerr << "[VulkanInferenceEngine] Attention dispatch failed" << std::endl;
        return 0;
    }

    uint32_t result_handle = AllocateHandle();
    loaded_tensors_.push_back({"attention_result_" + std::to_string(result_handle),
                               output_size, output_buffer_idx});
    return result_handle;
}

bool VulkanInferenceEngine::InitializeKVCache(uint32_t num_layers, uint32_t max_seq_len, uint32_t head_dim) {
    if (!gpu_available_) {
        return false;
    }

    if (!vulkan_compute_->AllocateKVCache(num_layers, max_seq_len, head_dim)) {
        std::cerr << "[VulkanInferenceEngine] Failed to allocate KV cache" << std::endl;
        return false;
    }

    std::cout << "[VulkanInferenceEngine] Allocated KV cache: " 
              << num_layers << " layers, max_seq_len=" << max_seq_len 
              << ", head_dim=" << head_dim << std::endl;
    return true;
}

void VulkanInferenceEngine::Shutdown() {
    if (vulkan_compute_) {
        vulkan_compute_->ReleaseTensors();
        vulkan_compute_->ClearKVCache();
    }
    loaded_tensors_.clear();
    gpu_available_ = false;
    is_initialized_ = false;
    std::cout << "[VulkanInferenceEngine] Vulkan inference engine shut down" << std::endl;
}

void VulkanInferenceEngine::Synchronize() {
    if (vulkan_compute_) {
        // Implement synchronization if available
        // This would wait for all pending GPU operations to complete
        std::cout << "[VulkanInferenceEngine] GPU synchronization (pending)" << std::endl;
    }
}

std::string VulkanInferenceEngine::GetDiagnostics() const {
    std::ostringstream oss;
    
    if (!is_initialized_) {
        oss << "Vulkan Inference Engine: NOT INITIALIZED\n";
        return oss.str();
    }

    oss << "Vulkan Inference Engine Status:\n";
    oss << "  GPU Available: " << (gpu_available_ ? "YES" : "NO") << "\n";
    
    if (gpu_available_ && vulkan_compute_) {
        auto device_info = vulkan_compute_->GetDeviceInfo();
        oss << "  Device Name: " << device_info.device_name << "\n";
        oss << "  Vendor ID: 0x" << std::hex << device_info.vendor_id << std::dec << "\n";
        oss << "  Device ID: 0x" << std::hex << device_info.device_id << std::dec << "\n";
        oss << "  Compute Queue Family: " << device_info.compute_queue_family << "\n";
        oss << "  Loaded Tensors: " << loaded_tensors_.size() << "\n";
        
        size_t total_gpu_memory = 0;
        for (const auto& tensor : loaded_tensors_) {
            total_gpu_memory += tensor.size_bytes;
        }
        oss << "  Total GPU Memory Used: " << (total_gpu_memory / 1024 / 1024) << " MB\n";
        oss << "  KV Cache Allocated: " << (vulkan_compute_->IsKVCacheAllocated() ? "YES" : "NO") << "\n";
    }

    return oss.str();
}

uint32_t VulkanInferenceEngine::AllocateHandle() {
    return next_handle_++;
}
