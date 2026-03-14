#include "gguf_d3d12_bridge.h"

#include <algorithm>
#include <filesystem>
#include <vector>
#include <cstring>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace RawrXD {

namespace {

struct MatVecConstants {
    uint32_t cbRows;
    uint32_t cbCols;
    uint32_t cbBlocksPerRow;
    uint32_t cbDim;
    uint32_t cbPosition;
    uint32_t cbThetaBase;
    float cbEps;
    uint32_t cbPad;
};

static Microsoft::WRL::ComPtr<ID3DBlob> loadBlob(const std::filesystem::path& p) {
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    if (SUCCEEDED(D3DReadFileToBlob(p.c_str(), &blob))) {
        return blob;
    }
    return {};
}

static Microsoft::WRL::ComPtr<ID3DBlob> compileFromFile(
    const std::wstring& path,
    const char* entry,
    const char* target) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompileFromFile(
        path.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry,
        target,
        flags,
        0,
        &shader,
        &errors);
    if (FAILED(hr)) {
        return {};
    }
    return shader;
}

} // namespace

GGUFD3D12Bridge::GGUFD3D12Bridge() = default;

GGUFD3D12Bridge::~GGUFD3D12Bridge() {
    Shutdown();
}

bool GGUFD3D12Bridge::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) {
    if (!device || !queue) return false;

    device_ = device;
    queue_ = queue;

    if (FAILED(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&allocator_)))) {
        return false;
    }

    if (FAILED(device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator_.Get(),
            nullptr,
            IID_PPV_ARGS(&list_)))) {
        return false;
    }

    list_->Close();

    if (FAILED(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)))) {
        return false;
    }

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) return false;

    return true;
}

void GGUFD3D12Bridge::Shutdown() {
    state_.clear();
    gpu_.psoMatVecQ4.Reset();
    gpu_.rootSig.Reset();
    shaders_.matVecQ4.Reset();
    shaders_.rmsNorm.Reset();
    shaders_.softmax.Reset();
    shaders_.rope.Reset();

    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    fence_.Reset();
    list_.Reset();
    allocator_.Reset();
    queue_.Reset();
    device_.Reset();
}

bool GGUFD3D12Bridge::LoadShadersFromDirectory(const std::string& shaderDirectory) {
    std::filesystem::path base(shaderDirectory);

    shaders_.matVecQ4 = loadBlob(base / "CSMatVecQ4.cso");
    shaders_.rmsNorm  = loadBlob(base / "CSRMSNorm.cso");
    shaders_.softmax  = loadBlob(base / "CSSoftmax.cso");
    shaders_.rope     = loadBlob(base / "CSRoPE.cso");

    // Only MatVec is required for first bridge milestone.
    if (!shaders_.matVecQ4) return false;

    return buildRootSignatureAndPSO();
}

bool GGUFD3D12Bridge::CompileShadersFromHLSL(const std::wstring& hlslPath) {
    shaders_.matVecQ4 = compileFromFile(hlslPath, "CSMatVecQ4", "cs_5_1");
    shaders_.rmsNorm  = compileFromFile(hlslPath, "CSRMSNorm", "cs_5_1");
    shaders_.softmax  = compileFromFile(hlslPath, "CSSoftmax", "cs_5_1");
    shaders_.rope     = compileFromFile(hlslPath, "CSRoPE", "cs_5_1");

    if (!shaders_.matVecQ4) return false;

    return buildRootSignatureAndPSO();
}

bool GGUFD3D12Bridge::buildRootSignatureAndPSO() {
    if (!device_ || !shaders_.matVecQ4) return false;

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 3;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE uavRange{};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 2;
    uavRange.BaseShaderRegister = 0;
    uavRange.RegisterSpace = 0;
    uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[3] = {};

    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[0].Constants.Num32BitValues = 8;
    params[0].Constants.ShaderRegister = 0;
    params[0].Constants.RegisterSpace = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &srvRange;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges = &uavRange;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = _countof(params);
    rsDesc.pParameters = params;
    rsDesc.NumStaticSamplers = 0;
    rsDesc.pStaticSamplers = nullptr;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> rsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> rsErr;
    if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &rsErr))) {
        return false;
    }

    if (FAILED(device_->CreateRootSignature(
            0,
            rsBlob->GetBufferPointer(),
            rsBlob->GetBufferSize(),
            IID_PPV_ARGS(&gpu_.rootSig)))) {
        return false;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = gpu_.rootSig.Get();
    pso.CS.pShaderBytecode = shaders_.matVecQ4->GetBufferPointer();
    pso.CS.BytecodeLength = shaders_.matVecQ4->GetBufferSize();

    if (FAILED(device_->CreateComputePipelineState(&pso, IID_PPV_ARGS(&gpu_.psoMatVecQ4)))) {
        return false;
    }

    return true;
}

bool GGUFD3D12Bridge::transition(ID3D12GraphicsCommandList* list,
                                 ID3D12Resource* resource,
                                 D3D12_RESOURCE_STATES newState) {
    if (!list || !resource) return false;

    auto it = state_.find(resource);
    D3D12_RESOURCE_STATES oldState = D3D12_RESOURCE_STATE_COMMON;
    if (it != state_.end()) oldState = it->second.state;

    if (oldState == newState) return true;

    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = resource;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = oldState;
    b.Transition.StateAfter = newState;
    list->ResourceBarrier(1, &b);

    state_[resource].state = newState;
    return true;
}

bool GGUFD3D12Bridge::executeAndWait() {
    if (!queue_ || !list_ || !fence_ || !fenceEvent_) return false;

    HRESULT hr = list_->Close();
    if (FAILED(hr)) { fprintf(stderr, "[D3D12] list Close failed: 0x%08lX\n", hr); return false; }

    ID3D12CommandList* lists[] = { list_.Get() };
    queue_->ExecuteCommandLists(1, lists);

    ++fenceValue_;
    hr = queue_->Signal(fence_.Get(), fenceValue_);
    if (FAILED(hr)) { fprintf(stderr, "[D3D12] queue Signal failed: 0x%08lX\n", hr); return false; }

    if (fence_->GetCompletedValue() < fenceValue_) {
        hr = fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        if (FAILED(hr)) { fprintf(stderr, "[D3D12] SetEventOnCompletion failed: 0x%08lX\n", hr); return false; }
        DWORD result = WaitForSingleObject(fenceEvent_, 15000);
        if (result != WAIT_OBJECT_0) {
            fprintf(stderr, "[D3D12] fence wait returned %lu (expected 0=WAIT_OBJECT_0)\n", result);
        }
    }

    return true;
}

bool GGUFD3D12Bridge::UploadTensor(const void* bytes,
                                   uint64_t sizeBytes,
                                   GGMLType,
                                   Microsoft::WRL::ComPtr<ID3D12Resource>& outGPUBuffer) {
    if (!device_ || !queue_ || !allocator_ || !list_ || !bytes || sizeBytes == 0) {
        return false;
    }

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    // Allow UAV so buffers can be used as compute shader outputs
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    Microsoft::WRL::ComPtr<ID3D12Resource> gpu;
    if (FAILED(device_->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&gpu)))) {
        return false;
    }

    // Upload heap resource: must NOT have UAV flag
    D3D12_RESOURCE_DESC uploadDesc = desc;
    uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3D12Resource> upload;
    if (FAILED(device_->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&upload)))) {
        return false;
    }

    void* mapped = nullptr;
    D3D12_RANGE rr{0, 0};
    if (FAILED(upload->Map(0, &rr, &mapped))) {
        return false;
    }
    std::memcpy(mapped, bytes, (size_t)sizeBytes);
    upload->Unmap(0, nullptr);

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), nullptr))) return false;

    transition(list_.Get(), gpu.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
    list_->CopyBufferRegion(gpu.Get(), 0, upload.Get(), 0, sizeBytes);
    transition(list_.Get(), gpu.Get(), D3D12_RESOURCE_STATE_COMMON);

    if (!executeAndWait()) return false;

    outGPUBuffer = gpu;
    state_[outGPUBuffer.Get()].state = D3D12_RESOURCE_STATE_COMMON;
    return true;
}

bool GGUFD3D12Bridge::UploadGGUFTensor(const TensorInfo&,
                                       const std::vector<uint8_t>& tensorBytes,
                                       Microsoft::WRL::ComPtr<ID3D12Resource>& outGPUBuffer) {
    if (tensorBytes.empty()) return false;
    return UploadTensor(tensorBytes.data(), (uint64_t)tensorBytes.size(), GGMLType::Q4_0, outGPUBuffer);
}

bool GGUFD3D12Bridge::DispatchMatVecQ4(ID3D12Resource* matrixBuffer,
                                       ID3D12Resource* vectorBuffer,
                                       ID3D12Resource* outputBuffer,
                                       uint32_t rows,
                                       uint32_t cols) {
    if (!device_ || !gpu_.rootSig || !gpu_.psoMatVecQ4 || !allocator_ || !list_ || !queue_) {
        fprintf(stderr, "[D3D12] Dispatch: null ptrs device=%p rootsig=%p pso=%p alloc=%p list=%p queue=%p\n",
                (void*)device_.Get(), (void*)gpu_.rootSig.Get(), (void*)gpu_.psoMatVecQ4.Get(),
                (void*)allocator_.Get(), (void*)list_.Get(), (void*)queue_.Get());
        return false;
    }
    if (!matrixBuffer || !vectorBuffer || !outputBuffer || rows == 0 || cols == 0) {
        fprintf(stderr, "[D3D12] Dispatch: null buffers or zero dims (rows=%u cols=%u)\n", rows, cols);
        return false;
    }

    HRESULT hr;
    hr = allocator_->Reset();
    if (FAILED(hr)) { fprintf(stderr, "[D3D12] allocator Reset failed: 0x%08lX\n", hr); return false; }

    hr = list_->Reset(allocator_.Get(), gpu_.psoMatVecQ4.Get());
    if (FAILED(hr)) { fprintf(stderr, "[D3D12] list Reset failed: 0x%08lX\n", hr); return false; }

    transition(list_.Get(), matrixBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), vectorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 5; // t0,t1,t2,u0,u1
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
    if (FAILED(hr)) {
        fprintf(stderr, "[D3D12] CreateDescriptorHeap failed: 0x%08lX\n", hr);
        return false;
    }

    const UINT inc = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpu = heap->GetCPUDescriptorHandleForHeapStart();

    // t0: ByteAddressBuffer matrix
    D3D12_SHADER_RESOURCE_VIEW_DESC srvRaw{};
    srvRaw.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvRaw.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvRaw.Format = DXGI_FORMAT_R32_TYPELESS;
    srvRaw.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
    srvRaw.Buffer.FirstElement = 0;
    srvRaw.Buffer.NumElements = (UINT)(matrixBuffer->GetDesc().Width / 4ull);
    srvRaw.Buffer.StructureByteStride = 0;
    device_->CreateShaderResourceView(matrixBuffer, &srvRaw, cpu);

    // t1: StructuredBuffer<float> vector
    cpu.ptr += inc;
    D3D12_SHADER_RESOURCE_VIEW_DESC srvVec{};
    srvVec.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvVec.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvVec.Format = DXGI_FORMAT_UNKNOWN;
    srvVec.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    srvVec.Buffer.FirstElement = 0;
    srvVec.Buffer.NumElements = cols;
    srvVec.Buffer.StructureByteStride = sizeof(float);
    device_->CreateShaderResourceView(vectorBuffer, &srvVec, cpu);

    // t2: gamma (unused for MatVec, bind vector as harmless placeholder)
    cpu.ptr += inc;
    device_->CreateShaderResourceView(vectorBuffer, &srvVec, cpu);

    // u0: g_inout (unused for MatVec, bind output)
    cpu.ptr += inc;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav.Format = DXGI_FORMAT_UNKNOWN;
    uav.Buffer.FirstElement = 0;
    uav.Buffer.NumElements = rows;
    uav.Buffer.StructureByteStride = sizeof(float);
    uav.Buffer.CounterOffsetInBytes = 0;
    uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    device_->CreateUnorderedAccessView(outputBuffer, nullptr, &uav, cpu);

    // u1: g_out
    cpu.ptr += inc;
    device_->CreateUnorderedAccessView(outputBuffer, nullptr, &uav, cpu);

    ID3D12DescriptorHeap* heaps[] = { heap.Get() };
    list_->SetDescriptorHeaps(1, heaps);
    list_->SetComputeRootSignature(gpu_.rootSig.Get());

    MatVecConstants c{};
    c.cbRows = rows;
    c.cbCols = cols;
    c.cbBlocksPerRow = cols / 32u;
    c.cbDim = cols;
    c.cbPosition = 0;
    c.cbThetaBase = 10000u;
    c.cbEps = 1e-5f;
    c.cbPad = 0;

    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    auto gpuBase = heap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = gpuBase;
    D3D12_GPU_DESCRIPTOR_HANDLE uavGpu = gpuBase;
    uavGpu.ptr += (3ull * inc);

    list_->SetComputeRootDescriptorTable(1, srvGpu);
    list_->SetComputeRootDescriptorTable(2, uavGpu);

    const uint32_t groupsX = (rows + 255u) / 256u;
    list_->Dispatch(groupsX, 1, 1);

    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_COMMON);

    return executeAndWait();
}

bool GGUFD3D12Bridge::ReadbackBuffer(ID3D12Resource* gpuBuffer,
                                     void* outBytes,
                                     uint64_t sizeBytes) {
    if (!device_ || !allocator_ || !list_ || !queue_ || !gpuBuffer || !outBytes || sizeBytes == 0) {
        return false;
    }

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;

    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    if (FAILED(device_->CreateCommittedResource(
            &readbackHeap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&readback)))) {
        return false;
    }

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), nullptr))) return false;

    transition(list_.Get(), gpuBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
    list_->CopyBufferRegion(readback.Get(), 0, gpuBuffer, 0, sizeBytes);

    if (!executeAndWait()) return false;

    void* mapped = nullptr;
    D3D12_RANGE rr{0, (SIZE_T)sizeBytes};
    if (FAILED(readback->Map(0, &rr, &mapped))) {
        return false;
    }

    std::memcpy(outBytes, mapped, (size_t)sizeBytes);
    D3D12_RANGE wr{0, 0};
    readback->Unmap(0, &wr);

    transition(list_.Get(), gpuBuffer, D3D12_RESOURCE_STATE_COMMON);
    return true;
}

} // namespace RawrXD
