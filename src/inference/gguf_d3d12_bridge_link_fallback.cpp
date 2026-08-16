#include "../../include/gguf_d3d12_bridge.h"

#include <cstring>

namespace RawrXD {

GGUFD3D12Bridge::GGUFD3D12Bridge() {
    fenceEvent_ = nullptr;
    fenceValue_ = 0;
    fusedRecording_ = false;
    fusedOpsRecorded_ = 0;
}
GGUFD3D12Bridge::~GGUFD3D12Bridge() = default;

bool GGUFD3D12Bridge::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) {
    if (!device || !queue) {
        return false;
    }
    device_ = device;
    queue_ = queue;
    fusedRecording_ = false;
    fusedOpsRecorded_ = 0;
    return true;
}

void GGUFD3D12Bridge::Shutdown() {
    state_.clear();
    fusedHeap_.Reset();
    allocator_.Reset();
    list_.Reset();
    fence_.Reset();
    queue_.Reset();
    device_.Reset();
    fenceEvent_ = nullptr;
    fenceValue_ = 0;
    fusedRecording_ = false;
    fusedOpsRecorded_ = 0;
}

bool GGUFD3D12Bridge::LoadShadersFromDirectory(const std::string& shaderDirectory) {
    return !shaderDirectory.empty();
}

bool GGUFD3D12Bridge::CompileShadersFromHLSL(const std::wstring& hlslPath) {
    return !hlslPath.empty();
}

bool GGUFD3D12Bridge::UploadTensor(const void* bytes,
                                   uint64_t sizeBytes,
                                   GGMLType type,
                                   Microsoft::WRL::ComPtr<ID3D12Resource>& outGPUBuffer) {
    (void)type;
    outGPUBuffer.Reset();
    return bytes != nullptr && sizeBytes > 0;
}

bool GGUFD3D12Bridge::UploadGGUFTensor(const TensorInfo& tensor,
                                       const std::vector<uint8_t>& tensorBytes,
                                       Microsoft::WRL::ComPtr<ID3D12Resource>& outGPUBuffer) {
    const uint64_t expected = tensor.size_bytes > 0 ? tensor.size_bytes : tensor.size;
    if (!tensorBytes.empty() && expected > 0 && tensorBytes.size() < expected) {
        outGPUBuffer.Reset();
        return false;
    }
    return UploadTensor(tensorBytes.empty() ? nullptr : tensorBytes.data(),
                        static_cast<uint64_t>(tensorBytes.size()),
                        tensor.type,
                        outGPUBuffer);
}

bool GGUFD3D12Bridge::DispatchMatVecQ4(ID3D12Resource* matrixBuffer,
                                       ID3D12Resource* vectorBuffer,
                                       ID3D12Resource* outputBuffer,
                                       uint32_t rows,
                                       uint32_t cols) {
    return matrixBuffer != nullptr && vectorBuffer != nullptr && outputBuffer != nullptr && rows > 0 && cols > 0;
}

bool GGUFD3D12Bridge::DispatchRMSNorm(ID3D12Resource* inoutBuffer,
                                      ID3D12Resource* gammaBuffer,
                                      uint32_t dim,
                                      float eps) {
    return inoutBuffer != nullptr && gammaBuffer != nullptr && dim > 0 && eps > 0.0f;
}

bool GGUFD3D12Bridge::DispatchSoftmax(ID3D12Resource* inoutBuffer,
                                      uint32_t dim) {
    return inoutBuffer != nullptr && dim > 0;
}

bool GGUFD3D12Bridge::DispatchRoPE(ID3D12Resource* inoutBuffer,
                                   uint32_t dim,
                                   uint32_t position,
                                   uint32_t thetaBase) {
    (void)position;
    return inoutBuffer != nullptr && dim > 0 && thetaBase > 1;
}

bool GGUFD3D12Bridge::DispatchCSRoPE_Fused(ID3D12Resource* q_buffer,
                                           ID3D12Resource* k_buffer,
                                           ID3D12Resource* cossin_buffer,
                                           uint32_t seq_pos,
                                           uint32_t head_dim,
                                           uint32_t num_heads) {
    (void)seq_pos;
    return q_buffer != nullptr && k_buffer != nullptr && cossin_buffer != nullptr &&
           head_dim > 0 && num_heads > 0;
}

bool GGUFD3D12Bridge::DispatchRoPEFused(ID3D12Resource* q_buffer,
                                        ID3D12Resource* k_buffer,
                                        ID3D12Resource* cossin_buffer,
                                        uint32_t seq_len,
                                        uint32_t head_dim,
                                        uint32_t num_heads) {
    return DispatchCSRoPE_Fused(q_buffer, k_buffer, cossin_buffer, seq_len, head_dim, num_heads);
}

bool GGUFD3D12Bridge::DispatchGEMM(ID3D12Resource* bufferA,
                                   ID3D12Resource* bufferB,
                                   ID3D12Resource* bufferC,
                                   uint32_t M,
                                   uint32_t K,
                                   uint32_t N) {
    return bufferA != nullptr && bufferB != nullptr && bufferC != nullptr && M > 0 && K > 0 && N > 0;
}

bool GGUFD3D12Bridge::DispatchSiLU(ID3D12Resource* inoutBuffer,
                                   uint32_t dim) {
    return inoutBuffer != nullptr && dim > 0;
}

bool GGUFD3D12Bridge::DispatchResidualAdd(ID3D12Resource* inoutBuffer,
                                          ID3D12Resource* residualBuffer,
                                          uint32_t dim) {
    return inoutBuffer != nullptr && residualBuffer != nullptr && dim > 0;
}

bool GGUFD3D12Bridge::DispatchHybridFlashAttention(ID3D12Resource* qBuffer,
                                                   ID3D12Resource* kBuffer,
                                                   ID3D12Resource* vBuffer,
                                                   ID3D12Resource* oBuffer,
                                                   uint32_t seqLen,
                                                   uint32_t headDim,
                                                   uint32_t numHeads,
                                                   float scale,
                                                   uint32_t avx512Threshold) {
    (void)scale;
    (void)avx512Threshold;
    return qBuffer != nullptr && kBuffer != nullptr && vBuffer != nullptr && oBuffer != nullptr &&
           seqLen > 0 && headDim > 0 && numHeads > 0;
}

bool GGUFD3D12Bridge::DispatchMatVecFP32(ID3D12Resource* matrixBuffer,
                                         ID3D12Resource* vectorBuffer,
                                         ID3D12Resource* outputBuffer,
                                         uint32_t rows,
                                         uint32_t cols) {
    return matrixBuffer != nullptr && vectorBuffer != nullptr && outputBuffer != nullptr &&
           rows > 0 && cols > 0;
}

bool GGUFD3D12Bridge::DispatchHybridFlashAttention(ID3D12Resource* qBuffer,
                                                   ID3D12Resource* kBuffer,
                                                   ID3D12Resource* vBuffer,
                                                   ID3D12Resource* outBuffer,
                                                   uint32_t seqLen,
                                                   uint32_t headDim,
                                                   uint32_t numHeads) {
    return DispatchHybridFlashAttention(qBuffer,
                                        kBuffer,
                                        vBuffer,
                                        outBuffer,
                                        seqLen,
                                        headDim,
                                        numHeads,
                                        1.0f,
                                        512);
}

bool GGUFD3D12Bridge::DispatchFlashAttention(ID3D12Resource* qBuffer,
                                             ID3D12Resource* kBuffer,
                                             ID3D12Resource* vBuffer,
                                             ID3D12Resource* outBuffer,
                                             uint32_t seqLen,
                                             uint32_t headDim,
                                             uint32_t numHeads) {
    return qBuffer != nullptr && kBuffer != nullptr && vBuffer != nullptr && outBuffer != nullptr &&
           seqLen > 0 && headDim > 0 && numHeads > 0;
}

bool GGUFD3D12Bridge::DispatchElementwiseMul(ID3D12Resource* aBuffer,
                                             ID3D12Resource* bBuffer,
                                             ID3D12Resource* outputBuffer,
                                             uint32_t dim) {
    return aBuffer != nullptr && bBuffer != nullptr && outputBuffer != nullptr && dim > 0;
}

bool GGUFD3D12Bridge::AllocateKVCache(uint32_t maxSeqLen,
                                      uint32_t nHeads,
                                      uint32_t headDim,
                                      Microsoft::WRL::ComPtr<ID3D12Resource>& outKVBuffer) {
    outKVBuffer.Reset();
    return maxSeqLen > 0 && nHeads > 0 && headDim > 0;
}

bool GGUFD3D12Bridge::DispatchKVCacheWrite(ID3D12Resource* kvBuffer,
                                           ID3D12Resource* vecBuffer,
                                           uint32_t pos,
                                           uint32_t headIdx,
                                           bool isValue,
                                           uint32_t nHeads,
                                           uint32_t headDim) {
    (void)pos;
    (void)headIdx;
    (void)isValue;
    return kvBuffer != nullptr && vecBuffer != nullptr && nHeads > 0 && headDim > 0;
}

bool GGUFD3D12Bridge::DispatchAttentionHead(ID3D12Resource* kvBuffer,
                                            ID3D12Resource* queryBuffer,
                                            ID3D12Resource* outputBuffer,
                                            uint32_t seqLen,
                                            uint32_t headIdx,
                                            uint32_t nHeads,
                                            uint32_t headDim) {
    (void)headIdx;
    return kvBuffer != nullptr && queryBuffer != nullptr && outputBuffer != nullptr &&
           seqLen > 0 && nHeads > 0 && headDim > 0;
}

bool GGUFD3D12Bridge::RecordKVCacheWrite(ID3D12Resource* kvBuffer,
                                         ID3D12Resource* vecBuffer,
                                         uint32_t pos,
                                         uint32_t headIdx,
                                         bool isValue,
                                         uint32_t nHeads,
                                         uint32_t headDim) {
    if (!DispatchKVCacheWrite(kvBuffer, vecBuffer, pos, headIdx, isValue, nHeads, headDim)) {
        return false;
    }
    fusedRecording_ = true;
    fusedOpsRecorded_ += 1;
    return true;
}

bool GGUFD3D12Bridge::RecordAttentionHead(ID3D12Resource* kvBuffer,
                                          ID3D12Resource* queryBuffer,
                                          ID3D12Resource* outputBuffer,
                                          uint32_t seqLen,
                                          uint32_t headIdx,
                                          uint32_t nHeads,
                                          uint32_t headDim) {
    if (!DispatchAttentionHead(kvBuffer, queryBuffer, outputBuffer, seqLen, headIdx, nHeads, headDim)) {
        return false;
    }
    fusedRecording_ = true;
    fusedOpsRecorded_ += 1;
    return true;
}

} // namespace RawrXD
