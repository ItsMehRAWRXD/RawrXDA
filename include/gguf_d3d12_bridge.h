#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "RawrXD_Interfaces.h"

namespace RawrXD {

class GGUFD3D12Bridge {
public:
    GGUFD3D12Bridge();
    ~GGUFD3D12Bridge();

    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* queue);
    void Shutdown();

    // Shader loading: prefer precompiled CSO, fallback to runtime compile.
    bool LoadShadersFromDirectory(const std::string& shaderDirectory);
    bool CompileShadersFromHLSL(const std::wstring& hlslPath);

    // Upload raw tensor bytes into GPU default-heap buffer.
    bool UploadTensor(const void* bytes,
                      uint64_t sizeBytes,
                      GGMLType type,
                      Microsoft::WRL::ComPtr<ID3D12Resource>& outGPUBuffer);

    // Helper overload for GGUF tensor metadata + bytes from loader.
    bool UploadGGUFTensor(const TensorInfo& tensor,
                          const std::vector<uint8_t>& tensorBytes,
                          Microsoft::WRL::ComPtr<ID3D12Resource>& outGPUBuffer);

    // ── Per-op dispatch (Phase C) ──────────────────────────────────────────
    // Q4_0 matrix-vector dispatch using CSMatVecQ4 kernel.
    bool DispatchMatVecQ4(ID3D12Resource* matrixBuffer,
                          ID3D12Resource* vectorBuffer,
                          ID3D12Resource* outputBuffer,
                          uint32_t rows,
                          uint32_t cols);

    // ── Phase D: Full kernel dispatch suite ────────────────────────────────
    // RMSNorm in-place: inoutBuffer[0..dim-1] normalized using gammaBuffer weights
    bool DispatchRMSNorm(ID3D12Resource* inoutBuffer,
                         ID3D12Resource* gammaBuffer,
                         uint32_t dim,
                         float eps = 1e-5f);

    // Softmax in-place: inoutBuffer[0..dim-1] → probabilities
    bool DispatchSoftmax(ID3D12Resource* inoutBuffer,
                         uint32_t dim);

    // RoPE in-place: inoutBuffer[0..dim-1] rotated at token position
        bool DispatchCSRoPE_Fused(ID3D12Resource* q_buffer, ID3D12Resource* k_buffer, ID3D12Resource* cossin_buffer,
                              uint32_t seq_pos, uint32_t head_dim, uint32_t num_heads);
    bool DispatchRoPE(ID3D12Resource* inoutBuffer,
                      uint32_t dim,
                      uint32_t position,
                      uint32_t thetaBase = 10000);

    bool DispatchRoPEFused(ID3D12Resource* q_buffer,
                           ID3D12Resource* k_buffer,
                           ID3D12Resource* cossin_buffer,
                           uint32_t seq_len,
                           uint32_t head_dim,
                           uint32_t num_heads);

    // SiLU in-place: inoutBuffer[0..dim-1] = x * sigmoid(x)
    bool DispatchSiLU(ID3D12Resource* inoutBuffer,
                      uint32_t dim);

    // Residual add in-place: inoutBuffer[i] += residualBuffer[i]
    bool DispatchResidualAdd(ID3D12Resource* inoutBuffer,
                             ID3D12Resource* residualBuffer,
                             uint32_t dim);

    // FP32 MatVec: matrixBuffer (rows×cols floats as ByteAddress) × vectorBuffer → outputBuffer
    bool DispatchMatVecFP32(ID3D12Resource* matrixBuffer,
                            ID3D12Resource* vectorBuffer,
                            ID3D12Resource* outputBuffer,
                            uint32_t rows,
                            uint32_t cols);

    // Element-wise multiply: outputBuffer[i] = aBuffer[i] * bBuffer[i]
    bool DispatchElementwiseMul(ID3D12Resource* aBuffer,
                                ID3D12Resource* bBuffer,
                                ID3D12Resource* outputBuffer,
                                uint32_t dim);

    // ── Phase E: GPU-Resident KV Cache ─────────────────────────────────
    // KV cache layout: interleaved K/V per head per position
    //   offset(pos, head, isV) = ((pos * nHeads + head) * 2 + isV) * headDim
    //   Total size = maxSeqLen * nHeads * 2 * headDim * sizeof(float)

    // Allocate a GPU-resident KV cache buffer for all layers.
    // Returns a single buffer holding the full cache.
    bool AllocateKVCache(uint32_t maxSeqLen, uint32_t nHeads, uint32_t headDim,
                         Microsoft::WRL::ComPtr<ID3D12Resource>& outKVBuffer);

    // Write a single head's K or V vector into the cache at a given position.
    // kvBuffer: the GPU KV cache
    // vecBuffer: head_dim floats to write
    // pos: sequence position to write at
    // headIdx: which attention head
    // isValue: false=write K, true=write V
    bool DispatchKVCacheWrite(ID3D12Resource* kvBuffer,
                              ID3D12Resource* vecBuffer,
                              uint32_t pos, uint32_t headIdx, bool isValue,
                              uint32_t nHeads, uint32_t headDim);

    // Compute single-head attention: Q × K_cache → softmax → × V_cache → output
    // kvBuffer: GPU KV cache (interleaved K/V)
    // queryBuffer: head_dim floats (the query vector for this head)
    // outputBuffer: head_dim floats (result)
    // seqLen: number of valid positions in cache
    bool DispatchAttentionHead(ID3D12Resource* kvBuffer,
                               ID3D12Resource* queryBuffer,
                               ID3D12Resource* outputBuffer,
                               uint32_t seqLen, uint32_t headIdx,
                               uint32_t nHeads, uint32_t headDim);

    // ── Phase E: Fused KV cache Record* variants ──────────────────────
    bool RecordKVCacheWrite(ID3D12Resource* kvBuffer,
                            ID3D12Resource* vecBuffer,
                            uint32_t pos, uint32_t headIdx, bool isValue,
                            uint32_t nHeads, uint32_t headDim);
    bool RecordAttentionHead(ID3D12Resource* kvBuffer,
                             ID3D12Resource* queryBuffer,
                             ID3D12Resource* outputBuffer,
                             uint32_t seqLen, uint32_t headIdx,
                             uint32_t nHeads, uint32_t headDim);

    // ── Phase D: Fused dispatch pipeline ───────────────────────────────────
    // Begin a fused recording session (resets command list, no execute).
    bool BeginFusedDispatch();

    // Record kernel dispatches (UAV barrier inserted between each).
    // These do NOT execute — they just record into the command list.
    bool RecordMatVecQ4(ID3D12Resource* matrixBuffer,
                        ID3D12Resource* vectorBuffer,
                        ID3D12Resource* outputBuffer,
                        uint32_t rows, uint32_t cols);
    bool RecordRMSNorm(ID3D12Resource* inoutBuffer,
                       ID3D12Resource* gammaBuffer,
                       uint32_t dim, float eps = 1e-5f);
    bool RecordSoftmax(ID3D12Resource* inoutBuffer, uint32_t dim);
    bool RecordRoPE(ID3D12Resource* inoutBuffer, uint32_t dim,
                    uint32_t position, uint32_t thetaBase = 10000);
    bool RecordRoPEFused(ID3D12Resource* q_buffer,
                         ID3D12Resource* k_buffer,
                         ID3D12Resource* cossin_buffer,
                         uint32_t seq_len,
                         uint32_t head_dim,
                         uint32_t num_heads);
    bool RecordSiLU(ID3D12Resource* inoutBuffer, uint32_t dim);
    bool RecordResidualAdd(ID3D12Resource* inoutBuffer,
                           ID3D12Resource* residualBuffer, uint32_t dim);
    bool RecordMatVecFP32(ID3D12Resource* matrixBuffer,
                          ID3D12Resource* vectorBuffer,
                          ID3D12Resource* outputBuffer,
                          uint32_t rows, uint32_t cols);
    bool RecordElementwiseMul(ID3D12Resource* aBuffer,
                              ID3D12Resource* bBuffer,
                              ID3D12Resource* outputBuffer,
                              uint32_t dim);

    // Flush: close command list, execute, fence wait. Returns all recorded work.
    bool FlushAndWait();

    // ── GPU Buffer Pool ────────────────────────────────────────────────────
    // Allocate a persistent GPU buffer (default heap, UAV-capable).
    bool AllocateBuffer(uint64_t sizeBytes,
                        Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer);

    // Utility: read back GPU buffer bytes to CPU.
    bool ReadbackBuffer(ID3D12Resource* gpuBuffer,
                        void* outBytes,
                        uint64_t sizeBytes);

private:
    struct ShaderBlobs {
        Microsoft::WRL::ComPtr<ID3DBlob> matVecQ4;
        Microsoft::WRL::ComPtr<ID3DBlob> rmsNorm;
        Microsoft::WRL::ComPtr<ID3DBlob> softmax;
        Microsoft::WRL::ComPtr<ID3DBlob> rope;
        Microsoft::WRL::ComPtr<ID3DBlob> ropeFused;
        // Phase D additions
        Microsoft::WRL::ComPtr<ID3DBlob> silu;
        Microsoft::WRL::ComPtr<ID3DBlob> residualAdd;
        Microsoft::WRL::ComPtr<ID3DBlob> matVecFP32;
        Microsoft::WRL::ComPtr<ID3DBlob> elementwiseMul;
        // Phase E additions
        Microsoft::WRL::ComPtr<ID3DBlob> kvCacheWrite;
        Microsoft::WRL::ComPtr<ID3DBlob> attentionHead;
    };

    struct RootAndPSO {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoMatVecQ4;
        // Phase D: PSOs for all kernels
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoRMSNorm;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoSoftmax;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoRoPE;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoRoPEFused;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoSiLU;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoResidualAdd;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoMatVecFP32;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoElementwiseMul;
        // Phase E
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoKVCacheWrite;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoAttentionHead;
    };

    struct ResourceState {
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    };

    bool buildRootSignatureAndPSO();
    bool executeAndWait();
    bool transition(ID3D12GraphicsCommandList* list,
                    ID3D12Resource* resource,
                    D3D12_RESOURCE_STATES newState);

    // Phase D: Create descriptor heap and bind resources for a dispatch
    bool setupDescriptorsForDispatch(
        ID3D12Resource* matrix, uint32_t matrixElements,
        ID3D12Resource* vec, uint32_t vecElements,
        ID3D12Resource* gamma, uint32_t gammaElements,
        ID3D12Resource* cossin, uint32_t cossinElements,
        ID3D12Resource* inout, uint32_t inoutElements,
        ID3D12Resource* out, uint32_t outElements);

    // Persistent descriptor heap for fused dispatch
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> fusedHeap_;

    // Insert UAV barrier (ensures previous dispatch writes complete)
    void insertUAVBarrier();

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    uint64_t fenceValue_ = 0;

    ShaderBlobs shaders_{};
    RootAndPSO gpu_{};

    std::unordered_map<ID3D12Resource*, ResourceState> state_;

    // Phase D: fused dispatch state
    bool fusedRecording_ = false;
    int fusedOpsRecorded_ = 0;
};

} // namespace RawrXD
