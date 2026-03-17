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
    if (!device_ || !commandList_ || !commandAllocator_ || !commandQueue_) return false;
    if (!data || size == 0) return false;

    // 1. Reset command allocator + list for new recording
    HRESULT hr = commandAllocator_->Reset();
    if (FAILED(hr)) return false;
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    if (FAILED(hr)) return false;

    // 2. Create upload + UAV buffers
    size_t alignedSize = (size + 255) & ~255; // D3D12 constant buffer alignment
    D3D12_HEAP_PROPERTIES uploadHeap = { D3D12_HEAP_TYPE_UPLOAD };
    D3D12_HEAP_PROPERTIES defaultHeap = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_HEAP_PROPERTIES readbackHeap = { D3D12_HEAP_TYPE_READBACK };
    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = alignedSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuf, uavBuf, readbackBuf;

    // Upload buffer (CPU -> GPU)
    hr = device_->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuf));
    if (FAILED(hr)) return false;

    // UAV buffer (GPU compute target)
    bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    hr = device_->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavBuf));
    if (FAILED(hr)) return false;

    // Readback buffer (GPU -> CPU)
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    hr = device_->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuf));
    if (FAILED(hr)) return false;

    // 3. Map upload buffer and copy data in
    void* mapped = nullptr;
    D3D12_RANGE readRange = {0, 0}; // We won't read from upload buffer
    hr = uploadBuf->Map(0, &readRange, &mapped);
    if (FAILED(hr)) return false;
    memcpy(mapped, data, size);
    uploadBuf->Unmap(0, nullptr);

    // 4. Record copy upload -> UAV
    commandList_->CopyResource(uavBuf.Get(), uploadBuf.Get());

    // Barrier: COPY_DEST -> UNORDERED_ACCESS
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = uavBuf.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList_->ResourceBarrier(1, &barrier);

    // NOTE: Actual PSO/root-signature/dispatch would go here with a compiled
    // compute shader. For now, the data round-trip (upload->UAV->readback)
    // validates the GPU pipeline is functional.

    // Barrier: UNORDERED_ACCESS -> COPY_SOURCE
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    commandList_->ResourceBarrier(1, &barrier);

    // Copy UAV -> readback
    commandList_->CopyResource(readbackBuf.Get(), uavBuf.Get());

    // 5. Close and execute
    hr = commandList_->Close();
    if (FAILED(hr)) return false;

    ID3D12CommandList* lists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);

    // 6. GPU fence wait
    fenceValue_++;
    hr = commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (FAILED(hr)) return false;

    if (fence_->GetCompletedValue() < fenceValue_) {
        hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        if (FAILED(hr)) return false;
        WaitForSingleObject(fenceEvent_, 10000); // 10s timeout
    }

    // 7. Read results back into caller buffer
    void* readback = nullptr;
    D3D12_RANGE fullRange = {0, size};
    hr = readbackBuf->Map(0, &fullRange, &readback);
    if (SUCCEEDED(hr)) {
        memcpy(data, readback, size);
        D3D12_RANGE emptyRange = {0, 0};
        readbackBuf->Unmap(0, &emptyRange);
    }

    return true;
}

void D3D12Compute::Shutdown() {
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = NULL;
    }
}