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
    if (!device_ || !commandQueue_ || !commandAllocator_ || !commandList_) return false;
    if (!data || size == 0) return false;

    // Create upload buffer (CPU-visible)
    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = size;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuf;
    if (FAILED(device_->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
        &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuf)))) {
        return false;
    }

    // Create GPU-local buffer (default heap)
    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> gpuBuf;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    if (FAILED(device_->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
        &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&gpuBuf)))) {
        return false;
    }

    // Map upload buffer and copy data
    void* mapped = nullptr;
    D3D12_RANGE readRange = {0, 0};
    if (FAILED(uploadBuf->Map(0, &readRange, &mapped))) return false;
    memcpy(mapped, data, size);
    uploadBuf->Unmap(0, nullptr);

    // Reset command list
    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);

    // Copy upload -> GPU buffer
    commandList_->CopyResource(gpuBuf.Get(), uploadBuf.Get());

    // Transition to UAV for compute
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = gpuBuf.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // Close and execute
    commandList_->Close();
    ID3D12CommandList* lists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);

    // Wait for GPU completion
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // Copy results back: transition GPU buffer to copy source
    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    commandList_->ResourceBarrier(1, &barrier);

    // Create readback buffer
    D3D12_HEAP_PROPERTIES readbackHeap = {};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC readbackDesc = bufDesc;
    readbackDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3D12Resource> readbackBuf;
    if (FAILED(device_->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE,
        &readbackDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuf)))) {
        return false;
    }

    commandList_->CopyResource(readbackBuf.Get(), gpuBuf.Get());
    commandList_->Close();
    commandQueue_->ExecuteCommandLists(1, lists);

    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    // Read results back to CPU
    void* readbackMapped = nullptr;
    if (SUCCEEDED(readbackBuf->Map(0, nullptr, &readbackMapped))) {
        memcpy(data, readbackMapped, size);
        readbackBuf->Unmap(0, nullptr);
    }

    return true;
}

void D3D12Compute::Shutdown() {
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = NULL;
    }
}