// ============================================================================
// vulkan_compute_stub.hpp — CPU Compute Fallback with Vulkan-Ready API
// ============================================================================
// Full CPU compute implementation for matrix operations, providing a
// drop-in replacement when Vulkan GPU compute is unavailable. Implements
// GEMM (General Matrix Multiply), quantized matmul, and element-wise ops.
//
// Pattern: PatchResult-style structured results, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <cstddef>

// ============================================================================
// Result type
// ============================================================================
struct ComputeResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static ComputeResult ok(const char* msg = "OK") { return { true, msg, 0 }; }
    static ComputeResult error(const char* msg, int code = -1) { return { false, msg, code }; }
};

// ============================================================================
// Quantization types supported
// ============================================================================
enum QuantType {
    QUANT_F32   = 0,
    QUANT_F16   = 1,
    QUANT_Q8_0  = 2,
    QUANT_Q4_0  = 3,
    QUANT_Q4_1  = 4,
    QUANT_Q2_K  = 5,
    QUANT_Q3_K  = 6,
    QUANT_Q4_K  = 7,
    QUANT_Q5_K  = 8,
    QUANT_Q6_K  = 9
};

// ============================================================================
// Compute buffer descriptor
// ============================================================================
struct ComputeBuffer {
    void*       data;
    size_t      sizeBytes;
    QuantType   quantType;
    int         rows;
    int         cols;
};

// ============================================================================
// Class: VulkanCompute — CPU Fallback Implementation
// ============================================================================
class VulkanCompute {
public:
    VulkanCompute();
    ~VulkanCompute();

    // Lifecycle
    bool Initialize();
    void Cleanup();
    bool IsInitialized() const { return m_initialized; }
    bool IsGPU() const { return false; }  // CPU fallback

    // Matrix operations
    ComputeResult MatMul(const ComputeBuffer& A, const ComputeBuffer& B,
                         ComputeBuffer& C);
    ComputeResult MatMulQuantized(const ComputeBuffer& A, const ComputeBuffer& B,
                                   ComputeBuffer& C, QuantType outputType);

    // Element-wise operations
    ComputeResult Add(const ComputeBuffer& A, const ComputeBuffer& B, ComputeBuffer& C);
    ComputeResult Mul(const ComputeBuffer& A, const ComputeBuffer& B, ComputeBuffer& C);
    ComputeResult RMSNorm(const ComputeBuffer& input, ComputeBuffer& output, float eps);
    ComputeResult SoftMax(const ComputeBuffer& input, ComputeBuffer& output);
    ComputeResult GeLU(const ComputeBuffer& input, ComputeBuffer& output);
    ComputeResult SiLU(const ComputeBuffer& input, ComputeBuffer& output);
    ComputeResult RoPE(ComputeBuffer& data, int nHead, int headDim, int posOffset);

    // Buffer management
    ComputeResult AllocateBuffer(ComputeBuffer& buf, int rows, int cols, QuantType type);
    void FreeBuffer(ComputeBuffer& buf);

    // Metrics
    double GetLastOpTimeMs() const { return m_lastOpTimeMs; }
    uint64_t GetTotalOps() const { return m_totalOps; }
    uint64_t GetTotalFlops() const { return m_totalFlops; }

private:
    // Internal helpers
    void dequantize(const void* src, float* dst, int count, QuantType type);
    void quantize(const float* src, void* dst, int count, QuantType type);
    size_t bytesPerElement(QuantType type) const;

    bool     m_initialized   = false;
    int      m_numThreads    = 1;
    double   m_lastOpTimeMs  = 0.0;
    uint64_t m_totalOps      = 0;
    uint64_t m_totalFlops    = 0;

    // Thread pool handle (Windows)
    PTP_POOL m_threadPool    = nullptr;
    TP_CALLBACK_ENVIRON m_callbackEnv = {};
};

// ============================================================================
// C API
// ============================================================================
extern "C" {
    VulkanCompute* VulkanCompute_Create();
    int  VulkanCompute_Initialize(VulkanCompute* vc);
    void VulkanCompute_Cleanup(VulkanCompute* vc);
    int  VulkanCompute_MatMul(VulkanCompute* vc, const ComputeBuffer* A,
                               const ComputeBuffer* B, ComputeBuffer* C);
    int  VulkanCompute_IsGPU(VulkanCompute* vc);
    double VulkanCompute_GetLastOpTimeMs(VulkanCompute* vc);
    void VulkanCompute_Destroy(VulkanCompute* vc);
}
