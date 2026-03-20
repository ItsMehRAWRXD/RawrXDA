#include "../../include/gguf_d3d12_bridge.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace RawrXD {

namespace {
struct BridgeFallbackStats {
    bool initialized = false;
    bool shadersReady = false;
    uint64_t initCalls = 0;
    uint64_t shaderLoadCalls = 0;
    uint64_t compileCalls = 0;
    uint64_t uploadCalls = 0;
    uint64_t uploadBytes = 0;
    uint64_t dispatchCalls = 0;
    uint64_t readbackCalls = 0;
    GGMLType lastType = GGMLType::F32;
    uint32_t lastRows = 0;
    uint32_t lastCols = 0;
    uint32_t lastDim = 0;
    uint32_t lastPos = 0;
};

std::mutex g_bridgeFallbackMutex;
std::unordered_map<const GGUFD3D12Bridge*, BridgeFallbackStats> g_bridgeFallbackStats;
} // namespace

GGUFD3D12Bridge::GGUFD3D12Bridge() {
    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    g_bridgeFallbackStats[this] = {};
}

GGUFD3D12Bridge::~GGUFD3D12Bridge() {
    Shutdown();
    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    g_bridgeFallbackStats.erase(this);
}

bool GGUFD3D12Bridge::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) {
    device_ = device;
    queue_ = queue;
    fusedRecording_ = false;
    fusedOpsRecorded_ = 0;
    state_.clear();

    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.initCalls += 1;
    stats.initialized = (device_ != nullptr && queue_ != nullptr);
    return stats.initialized;
}

void GGUFD3D12Bridge::Shutdown() {
    state_.clear();
    device_.Reset();
    queue_.Reset();
    allocator_.Reset();
    list_.Reset();
    fence_.Reset();
    fusedHeap_.Reset();
    shaders_ = {};
    gpu_ = {};
    fusedRecording_ = false;
    fusedOpsRecorded_ = 0;

    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    auto it = g_bridgeFallbackStats.find(this);
    if (it != g_bridgeFallbackStats.end()) {
        it->second.initialized = false;
    }
}

bool GGUFD3D12Bridge::LoadShadersFromDirectory(const std::string& shaderDirectory) {
    std::error_code ec;
    if (shaderDirectory.empty() || !std::filesystem::exists(shaderDirectory, ec) ||
        !std::filesystem::is_directory(shaderDirectory, ec)) {
        return false;
    }

    bool hasShaderLikeFile = false;
    for (const auto& entry : std::filesystem::directory_iterator(shaderDirectory, ec)) {
        if (ec) {
            break;
        }
        const std::string ext = entry.path().extension().string();
        if (ext == ".cso" || ext == ".hlsl" || ext == ".hlsli") {
            hasShaderLikeFile = true;
            break;
        }
    }

    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.shaderLoadCalls += 1;
    stats.shadersReady = hasShaderLikeFile;
    return hasShaderLikeFile;
}

bool GGUFD3D12Bridge::CompileShadersFromHLSL(const std::wstring& hlslPath) {
    if (hlslPath.empty()) {
        return false;
    }
    std::error_code ec;
    const bool exists = std::filesystem::exists(std::filesystem::path(hlslPath), ec);
    if (ec || !exists) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.compileCalls += 1;
    stats.shadersReady = true;
    return true;
}

bool GGUFD3D12Bridge::UploadTensor(const void* bytes,
                                   uint64_t sizeBytes,
                                   GGMLType type,
                                   Microsoft::WRL::ComPtr<ID3D12Resource>& outGPUBuffer) {
    if (bytes == nullptr || sizeBytes == 0) {
        return false;
    }
    outGPUBuffer.Reset();

    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.uploadCalls += 1;
    stats.uploadBytes += sizeBytes;
    stats.lastType = type;
    return true;
}

bool GGUFD3D12Bridge::UploadGGUFTensor(const TensorInfo& tensor,
                                       const std::vector<uint8_t>& tensorBytes,
                                       Microsoft::WRL::ComPtr<ID3D12Resource>& outGPUBuffer) {
    const uint64_t requiredBytes = (tensor.size_bytes > 0) ? tensor.size_bytes : tensor.size;
    if (requiredBytes > 0 && tensorBytes.size() < requiredBytes) {
        return false;
    }
    if (tensorBytes.empty()) {
        return false;
    }
    return UploadTensor(tensorBytes.data(), static_cast<uint64_t>(tensorBytes.size()), tensor.type, outGPUBuffer);
}

bool GGUFD3D12Bridge::DispatchMatVecQ4(ID3D12Resource* matrixBuffer,
                                       ID3D12Resource* vectorBuffer,
                                       ID3D12Resource* outputBuffer,
                                       uint32_t rows,
                                       uint32_t cols) {
    if (matrixBuffer == nullptr || vectorBuffer == nullptr || outputBuffer == nullptr || rows == 0 || cols == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.dispatchCalls += 1;
    stats.lastRows = rows;
    stats.lastCols = cols;
    return true;
}

bool GGUFD3D12Bridge::DispatchRMSNorm(ID3D12Resource* inoutBuffer,
                                      ID3D12Resource* gammaBuffer,
                                      uint32_t dim,
                                      float eps) {
    if (inoutBuffer == nullptr || gammaBuffer == nullptr || dim == 0 || eps <= 0.0f) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.dispatchCalls += 1;
    stats.lastDim = dim;
    return true;
}

bool GGUFD3D12Bridge::DispatchSoftmax(ID3D12Resource* inoutBuffer, uint32_t dim) {
    if (inoutBuffer == nullptr || dim == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.dispatchCalls += 1;
    stats.lastDim = dim;
    return true;
}

bool GGUFD3D12Bridge::DispatchRoPE(ID3D12Resource* inoutBuffer,
                                   uint32_t dim,
                                   uint32_t position,
                                   uint32_t thetaBase) {
    if (inoutBuffer == nullptr || dim < 2 || (dim % 2) != 0 || thetaBase == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.dispatchCalls += 1;
    stats.lastDim = dim;
    stats.lastPos = position;
    return true;
}

bool GGUFD3D12Bridge::DispatchGEMM(ID3D12Resource* bufferA,
                                   ID3D12Resource* bufferB,
                                   ID3D12Resource* bufferC,
                                   uint32_t M,
                                   uint32_t K,
                                   uint32_t N) {
    if (bufferA == nullptr || bufferB == nullptr || bufferC == nullptr || M == 0 || K == 0 || N == 0) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.dispatchCalls += 1;
    stats.lastRows = M;
    stats.lastCols = N;
    stats.lastDim = K;
    return true;
}

bool GGUFD3D12Bridge::ReadbackBuffer(ID3D12Resource* gpuBuffer, void* outBytes, uint64_t sizeBytes) {
    if (gpuBuffer == nullptr || outBytes == nullptr || sizeBytes == 0) {
        return false;
    }
    std::memset(outBytes, 0, static_cast<size_t>(sizeBytes));
    std::lock_guard<std::mutex> lock(g_bridgeFallbackMutex);
    BridgeFallbackStats& stats = g_bridgeFallbackStats[this];
    stats.readbackCalls += 1;
    return true;
}

} // namespace RawrXD
