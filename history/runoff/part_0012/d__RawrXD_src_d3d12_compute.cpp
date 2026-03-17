#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

// Native DirectX 12 compute implementation
class D3D12Compute {
public:
    D3D12Compute();
    ~D3D12Compute();

    bool Initialize();
    bool ExecuteComputeShader(void* data, size_t size);
    void Shutdown();

private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_;
    UINT64 fenceValue_;
};

// Basic implementation
D3D12Compute::D3D12Compute() : fenceValue_(0), fenceEvent_(NULL) {}

D3D12Compute::~D3D12Compute() {
    Shutdown();
}

bool D3D12Compute::Initialize() {
    // Create DXGI factory
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return false;
    }

    // Create D3D12 device
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) {
        return false;
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    if (FAILED(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue_)))) {
        return false;
    }

    // Create command allocator
    if (FAILED(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&commandAllocator_)))) {
        return false;
    }

    // Create command list
    if (FAILED(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_)))) {
        return false;
    }

    // Create fence
    if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))) {
        return false;
    }

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
        return false;
    }

    return true;
}

bool D3D12Compute::ExecuteComputeShader(void* data, size_t size) {
    // Placeholder - implement actual compute shader execution
    // This would load and execute compute shaders for model inference
    return true;
}

void D3D12Compute::Shutdown() {
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = NULL;
    }
}