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

    // Q4_0 matrix-vector dispatch using CSMatVecQ4 kernel.
    bool DispatchMatVecQ4(ID3D12Resource* matrixBuffer,
                          ID3D12Resource* vectorBuffer,
                          ID3D12Resource* outputBuffer,
                          uint32_t rows,
                          uint32_t cols);

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
    };

    struct RootAndPSO {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoMatVecQ4;
    };

    struct ResourceState {
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    };

    bool buildRootSignatureAndPSO();
    bool executeAndWait();
    bool transition(ID3D12GraphicsCommandList* list,
                    ID3D12Resource* resource,
                    D3D12_RESOURCE_STATES newState);

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
};

} // namespace RawrXD
