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

} // namespace RawrXD
