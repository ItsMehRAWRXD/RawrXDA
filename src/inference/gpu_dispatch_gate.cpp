#include "gpu_dispatch_gate.h"
#include "cpu_inference_engine.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <cstring>
#include <cmath>

namespace RawrXD {

GPUDispatchGate::GPUDispatchGate() = default;
GPUDispatchGate::~GPUDispatchGate() = default;

bool GPUDispatchGate::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) {
    gpuBridge_ = std::make_unique<GGUFD3D12Bridge>();

    if (!gpuBridge_->Initialize(device, queue)) {
        std::cerr << "[GPUDispatchGate] Failed to initialize GPU bridge" << std::endl;
        gpuBridge_.reset();
        return false;
    }

    // Load precompiled shaders from build/shaders directory
    std::string shaderDir = "build/shaders";
    if (!gpuBridge_->LoadShadersFromDirectory(shaderDir)) {
        std::cerr << "[GPUDispatchGate] Failed to load GPU shaders from " << shaderDir << std::endl;
        gpuBridge_.reset();
        return false;
    }

    std::cout << "[GPUDispatchGate] Initialized with GPU acceleration and CPU fallback" << std::endl;
    return true;
}

void GPUDispatchGate::Shutdown() {
    if (gpuBridge_) {
        gpuBridge_->Shutdown();
        gpuBridge_.reset();
    }
    std::cout << "[GPUDispatchGate] Shutdown complete" << std::endl;
}

// Initialize with self-created D3D12 device/queue
bool GPUDispatchGate::Initialize() {
    // Create D3D12 device and queue
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    
    // Enable D3D12 debug layer in debug builds
#ifdef _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

    // Create DXGI factory
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        std::cerr << "[GPUDispatchGate] Failed to create DXGI factory" << std::endl;
        return false;
    }

    // Find suitable adapter
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        
        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        
        // Try to create device
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
            std::wcout << L"[GPUDispatchGate] Using GPU: " << desc.Description << std::endl;
            break;
        }
    }

    if (!device) {
        std::cerr << "[GPUDispatchGate] Failed to create D3D12 device" << std::endl;
        return false;
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)))) {
        std::cerr << "[GPUDispatchGate] Failed to create command queue" << std::endl;
        return false;
    }

    return Initialize(device.Get(), queue.Get());
}

bool GPUDispatchGate::MatVecQ4(const float* matrix, const float* vector, float* output,
                               uint32_t rows, uint32_t cols, bool enableParityCheck) {
    if (!gpuBridge_) {
        // Fallback to CPU only
        return cpuEngine_.MatVecQ4(matrix, vector, output, rows, cols);
    }

    // GPU dispatch attempt
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuMatrix, gpuVector, gpuOutput;

    // Upload tensors to GPU (assuming Q4_0 quantization for matrix)
    RawrXD::TensorInfo matrixInfo;
    matrixInfo.name = "matrix";
    matrixInfo.shape = {cols, rows, 1, 1};  // columns, rows, 1, 1
    matrixInfo.type = RawrXD::GGMLType::Q4_0;
    matrixInfo.size_bytes = rows * cols * sizeof(float); // Approximate for Q4_0

    std::vector<uint8_t> matrixBytes(reinterpret_cast<const uint8_t*>(matrix),
                                     reinterpret_cast<const uint8_t*>(matrix) + rows * cols * sizeof(float));

    if (!gpuBridge_->UploadGGUFTensor(matrixInfo, matrixBytes, gpuMatrix)) {
        std::cerr << "[GPUDispatchGate] Failed to upload matrix to GPU" << std::endl;
        return cpuEngine_.MatVecQ4(matrix, vector, output, rows, cols);
    }

    // Upload vector (FP32)
    RawrXD::TensorInfo vectorInfo;
    vectorInfo.name = "vector";
    vectorInfo.shape = {cols, 1, 1, 1};
    vectorInfo.type = RawrXD::GGMLType::F32;
    vectorInfo.size_bytes = cols * sizeof(float);

    std::vector<uint8_t> vectorBytes(reinterpret_cast<const uint8_t*>(vector),
                                     reinterpret_cast<const uint8_t*>(vector) + cols * sizeof(float));

    if (!gpuBridge_->UploadGGUFTensor(vectorInfo, vectorBytes, gpuVector)) {
        std::cerr << "[GPUDispatchGate] Failed to upload vector to GPU" << std::endl;
        return cpuEngine_.MatVecQ4(matrix, vector, output, rows, cols);
    }

    // Create output buffer
    RawrXD::TensorInfo outputInfo;
    outputInfo.name = "output";
    outputInfo.shape = {rows, 1, 1, 1};
    outputInfo.type = RawrXD::GGMLType::F32;
    outputInfo.size_bytes = rows * sizeof(float);

    std::vector<uint8_t> dummyOutput(rows * sizeof(float), 0);
    if (!gpuBridge_->UploadGGUFTensor(outputInfo, dummyOutput, gpuOutput)) {
        std::cerr << "[GPUDispatchGate] Failed to create output buffer on GPU" << std::endl;
        return cpuEngine_.MatVecQ4(matrix, vector, output, rows, cols);
    }

    // Dispatch GPU kernel
    if (!gpuBridge_->DispatchMatVecQ4(gpuMatrix.Get(), gpuVector.Get(), gpuOutput.Get(), rows, cols)) {
        std::cerr << "[GPUDispatchGate] GPU dispatch failed, falling back to CPU" << std::endl;
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            stats_.cpuMatVecFallbacks++;
        }
        return cpuEngine_.MatVecQ4(matrix, vector, output, rows, cols);
    }

    // Read back result
    if (!gpuBridge_->ReadbackBuffer(gpuOutput.Get(), output, rows * sizeof(float))) {
        std::cerr << "[GPUDispatchGate] Failed to read back GPU result" << std::endl;
        return cpuEngine_.MatVecQ4(matrix, vector, output, rows, cols);
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.gpuMatVecCalls++;
    }

    // Parity check if enabled
    if (enableParityCheck) {
        std::vector<float> cpuResult(rows);
        auto start = std::chrono::high_resolution_clock::now();
        bool cpuSuccess = cpuEngine_.MatVecQ4(matrix, vector, cpuResult.data(), rows, cols);
        auto end = std::chrono::high_resolution_clock::now();
        double cpuTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

        if (cpuSuccess && checkParity(output, cpuResult.data(), rows, "MatVecQ4")) {
            // GPU result matches CPU within tolerance
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.avgParityCheckTimeMs = (stats_.avgParityCheckTimeMs + cpuTimeMs) / 2.0;
            }
            return true;
        } else {
            // Parity failure - fallback to CPU result
            std::cerr << "[GPUDispatchGate] Parity check failed for MatVecQ4, using CPU result" << std::endl;
            std::memcpy(output, cpuResult.data(), rows * sizeof(float));
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                stats_.parityFailures++;
                stats_.cpuMatVecFallbacks++;
            }
            return cpuSuccess;
        }
    }

    return true;
}

bool GPUDispatchGate::Softmax(float* data, uint32_t size, bool enableParityCheck) {
    if (!gpuBridge_) {
        // Fallback to CPU only
        return cpuEngine_.Softmax(data, size);
    }

    // For now, implement CPU-only softmax as GPU softmax kernel may not be ready
    // TODO: Implement GPU softmax dispatch when CSSoftmax kernel is fully integrated

    bool result = cpuEngine_.Softmax(data, size);

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        stats_.cpuSoftmaxFallbacks++;  // Currently always CPU
    }

    return result;
}

bool GPUDispatchGate::checkParity(const float* gpuResult, const float* cpuResult,
                                  uint32_t size, const char* operationName) {
    float maxErr = 0.0f;
    for (uint32_t i = 0; i < size; ++i) {
        float err = std::abs(gpuResult[i] - cpuResult[i]);
        if (err > maxErr) maxErr = err;
    }

    if (maxErr > MAX_PARITY_ERR) {
        std::cerr << "[GPUDispatchGate] Parity check FAILED for " << operationName
                  << ": max error = " << maxErr << " > " << MAX_PARITY_ERR << std::endl;
        return false;
    }

    std::cout << "[GPUDispatchGate] Parity check PASSED for " << operationName
              << ": max error = " << maxErr << std::endl;
    return true;
}

} // namespace RawrXD