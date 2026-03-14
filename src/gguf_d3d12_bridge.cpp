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
        if (errors) {
            fprintf(stderr, "[D3D12] Shader compile error '%s': %s\n",
                    entry, (const char*)errors->GetBufferPointer());
        }
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
    fusedHeap_.Reset();

    gpu_.psoMatVecQ4.Reset();
    gpu_.psoRMSNorm.Reset();
    gpu_.psoSoftmax.Reset();
    gpu_.psoRoPE.Reset();
    gpu_.psoRoPEFused.Reset();
    gpu_.psoSiLU.Reset();
    gpu_.psoResidualAdd.Reset();
    gpu_.psoMatVecFP32.Reset();
    gpu_.psoGEMM.Reset();
    gpu_.psoElementwiseMul.Reset();
    gpu_.psoKVCacheWrite.Reset();
    gpu_.psoAttentionHead.Reset();
    gpu_.rootSig.Reset();

    shaders_.matVecQ4.Reset();
    shaders_.rmsNorm.Reset();
    shaders_.softmax.Reset();
    shaders_.rope.Reset();
    shaders_.ropeFused.Reset();
    shaders_.silu.Reset();
    shaders_.residualAdd.Reset();
    shaders_.matVecFP32.Reset();
    shaders_.gemm.Reset();
    shaders_.elementwiseMul.Reset();
    shaders_.kvCacheWrite.Reset();
    shaders_.attentionHead.Reset();

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

    shaders_.matVecQ4     = loadBlob(base / "CSMatVecQ4.cso");
    shaders_.rmsNorm      = loadBlob(base / "CSRMSNorm.cso");
    shaders_.softmax      = loadBlob(base / "CSSoftmax.cso");
    shaders_.rope         = loadBlob(base / "CSRoPE.cso");
        shaders_.ropeFused    = loadBlob(base / "CSRoPE_Fused.cso");
    shaders_.silu         = loadBlob(base / "CSSiLU.cso");
    shaders_.residualAdd  = loadBlob(base / "CSResidualAdd.cso");
    shaders_.matVecFP32   = loadBlob(base / "CSMatVecFP32.cso");
    shaders_.gemm         = loadBlob(base / "CSGemm.cso");
    shaders_.elementwiseMul = loadBlob(base / "CSElementwiseMul.cso");
    shaders_.kvCacheWrite = loadBlob(base / "CSKVCacheWrite.cso");
    shaders_.attentionHead = loadBlob(base / "CSAttentionHead.cso");

    // if (!shaders_.matVecQ4) return false;

    return buildRootSignatureAndPSO();
}

bool GGUFD3D12Bridge::CompileShadersFromHLSL(const std::wstring& hlslPath) {
    shaders_.matVecQ4     = compileFromFile(hlslPath, "CSMatVecQ4", "cs_5_1");
    shaders_.rmsNorm      = compileFromFile(hlslPath, "CSRMSNorm", "cs_5_1");
    shaders_.softmax      = compileFromFile(hlslPath, "CSSoftmax", "cs_5_1");
    shaders_.rope         = compileFromFile(hlslPath, "CSRoPE", "cs_5_1");
        shaders_.ropeFused    = compileFromFile(hlslPath, "CSRoPE_Fused", "cs_6_0");
    shaders_.silu         = compileFromFile(hlslPath, "CSSiLU", "cs_5_1");
    shaders_.residualAdd  = compileFromFile(hlslPath, "CSResidualAdd", "cs_5_1");
    shaders_.matVecFP32   = compileFromFile(hlslPath, "CSMatVecFP32", "cs_5_1");
    shaders_.gemm         = compileFromFile(hlslPath, "CSGemm", "cs_5_1");
    if (!shaders_.gemm) {
        std::wstring gemmPath(hlslPath);
        size_t lastSlash = gemmPath.find_last_of(L"/\\");
        if (lastSlash != std::wstring::npos) {
            gemmPath = gemmPath.substr(0, lastSlash + 1) + L"cs_gemm.hlsl";
            shaders_.gemm = compileFromFile(gemmPath, "main", "cs_5_1");
        }
    }
    shaders_.elementwiseMul = compileFromFile(hlslPath, "CSElementwiseMul", "cs_5_1");
    shaders_.kvCacheWrite = compileFromFile(hlslPath, "CSKVCacheWrite", "cs_5_1");
    shaders_.attentionHead = compileFromFile(hlslPath, "CSAttentionHead", "cs_5_1");

    // if (!shaders_.matVecQ4) return false;

    return buildRootSignatureAndPSO();
}

bool GGUFD3D12Bridge::buildRootSignatureAndPSO() {
    if (!device_) return false;

    // ── Root signature (shared by all kernels) ─────────────────────────────
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 4; // t0, t1, t2, t3       // t0, t1, t2
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE uavRange{};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 2;       // u0, u1
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

    // ── Helper lambda: create PSO from shader blob ─────────────────────────
    auto createPSO = [&](ID3DBlob* blob,
                         Microsoft::WRL::ComPtr<ID3D12PipelineState>& outPSO) -> bool {
        if (!blob) return false;  // optional shader not found
        D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = gpu_.rootSig.Get();
        pso.CS.pShaderBytecode = blob->GetBufferPointer();
        pso.CS.BytecodeLength = blob->GetBufferSize();
        return SUCCEEDED(device_->CreateComputePipelineState(&pso, IID_PPV_ARGS(&outPSO)));
    };

    // MatVecQ4 is required
    // if (!createPSO(shaders_.matVecQ4.Get(), gpu_.psoMatVecQ4)) return false;

    // Phase D: all other PSOs (optional — graceful if shader missing)
    createPSO(shaders_.rmsNorm.Get(), gpu_.psoRMSNorm);
    createPSO(shaders_.softmax.Get(), gpu_.psoSoftmax);
    createPSO(shaders_.rope.Get(), gpu_.psoRoPE);
        createPSO(shaders_.ropeFused.Get(), gpu_.psoRoPEFused);
    createPSO(shaders_.silu.Get(), gpu_.psoSiLU);
    createPSO(shaders_.residualAdd.Get(), gpu_.psoResidualAdd);
    createPSO(shaders_.matVecFP32.Get(), gpu_.psoMatVecFP32);
    createPSO(shaders_.gemm.Get(), gpu_.psoGEMM);
    createPSO(shaders_.elementwiseMul.Get(), gpu_.psoElementwiseMul);
    createPSO(shaders_.kvCacheWrite.Get(), gpu_.psoKVCacheWrite);
    createPSO(shaders_.attentionHead.Get(), gpu_.psoAttentionHead);

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

void GGUFD3D12Bridge::insertUAVBarrier() {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = nullptr; // global UAV barrier
    list_->ResourceBarrier(1, &b);
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

// ── Phase D: Descriptor setup helper ───────────────────────────────────────────
// Creates a shader-visible descriptor heap with 6 descriptors (t0,t1,t2,t3,u0,u1)
// and populates them. Pass nullptr for unused slots.
bool GGUFD3D12Bridge::setupDescriptorsForDispatch(
    ID3D12Resource* matrix, uint32_t matrixElements,
    ID3D12Resource* vec, uint32_t vecElements,
    ID3D12Resource* gamma, uint32_t gammaElements,
    ID3D12Resource* cossin, uint32_t cossinElements,
    ID3D12Resource* inout, uint32_t inoutElements,
    ID3D12Resource* out, uint32_t outElements) {

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 6;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    if (FAILED(device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap))))
        return false;

    const UINT inc = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto cpu = heap->GetCPUDescriptorHandleForHeapStart();

    // t0: ByteAddressBuffer (matrix / raw data)
    if (matrix) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = DXGI_FORMAT_R32_TYPELESS;
        srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        srv.Buffer.NumElements = matrixElements;
        device_->CreateShaderResourceView(matrix, &srv, cpu);
    } else {
        D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv{};
        nullSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullSrv.Format = DXGI_FORMAT_R32_TYPELESS;
        nullSrv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        nullSrv.Buffer.NumElements = 1;
        device_->CreateShaderResourceView(nullptr, &nullSrv, cpu);
    }

    // t1: StructuredBuffer<float> (input vector)
    cpu.ptr += inc;
    if (vec) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.Buffer.NumElements = vecElements;
        srv.Buffer.StructureByteStride = sizeof(float);
        device_->CreateShaderResourceView(vec, &srv, cpu);
    } else {
        // Null SRV needs a valid desc
        D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv{};
        nullSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullSrv.Format = DXGI_FORMAT_R32_FLOAT;
        nullSrv.Buffer.NumElements = 1;
        device_->CreateShaderResourceView(nullptr, &nullSrv, cpu);
    }

    // t2: StructuredBuffer<float> (gamma weights for RMSNorm)
    cpu.ptr += inc;
    if (gamma) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.Buffer.NumElements = gammaElements;
        srv.Buffer.StructureByteStride = sizeof(float);
        device_->CreateShaderResourceView(gamma, &srv, cpu);
    } else {
        D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv{};
        nullSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullSrv.Format = DXGI_FORMAT_R32_FLOAT;
        nullSrv.Buffer.NumElements = 1;
        device_->CreateShaderResourceView(nullptr, &nullSrv, cpu);
    }

    
    // t3: StructuredBuffer<float2> (cossin tables for RoPE)
    cpu.ptr += inc;
    if (cossin) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.Buffer.NumElements = cossinElements;
        srv.Buffer.StructureByteStride = sizeof(float) * 2;
        device_->CreateShaderResourceView(cossin, &srv, cpu);
    } else {
        D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv{};
        nullSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullSrv.Format = DXGI_FORMAT_R32G32_FLOAT;
        nullSrv.Buffer.NumElements = 1;
        device_->CreateShaderResourceView(nullptr, &nullSrv, cpu);
    }

    // u0: RWStructuredBuffer<float> (in-place: RMSNorm, Softmax, RoPE, SiLU)
    cpu.ptr += inc;
    if (inout) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav.Format = DXGI_FORMAT_UNKNOWN;
        uav.Buffer.NumElements = inoutElements;
        uav.Buffer.StructureByteStride = sizeof(float);
        device_->CreateUnorderedAccessView(inout, nullptr, &uav, cpu);
    } else {
        D3D12_UNORDERED_ACCESS_VIEW_DESC nullUav{};
        nullUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        nullUav.Format = DXGI_FORMAT_R32_FLOAT;
        nullUav.Buffer.NumElements = 1;
        nullUav.Buffer.StructureByteStride = 0;
        device_->CreateUnorderedAccessView(nullptr, nullptr, &nullUav, cpu);
    }

    // u1: RWStructuredBuffer<float> (output for MatVec)
    cpu.ptr += inc;
    if (out) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uav.Format = DXGI_FORMAT_UNKNOWN;
        uav.Buffer.NumElements = outElements;
        uav.Buffer.StructureByteStride = sizeof(float);
        device_->CreateUnorderedAccessView(out, nullptr, &uav, cpu);
    } else {
        D3D12_UNORDERED_ACCESS_VIEW_DESC nullUav{};
        nullUav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        nullUav.Format = DXGI_FORMAT_R32_FLOAT;
        nullUav.Buffer.NumElements = 1;
        nullUav.Buffer.StructureByteStride = 0;
        device_->CreateUnorderedAccessView(nullptr, nullptr, &nullUav, cpu);
    }

    ID3D12DescriptorHeap* heaps[] = { heap.Get() };
    list_->SetDescriptorHeaps(1, heaps);
    list_->SetComputeRootSignature(gpu_.rootSig.Get());

    auto gpuBase = heap->GetGPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = gpuBase;
    D3D12_GPU_DESCRIPTOR_HANDLE uavGpu = gpuBase;
    uavGpu.ptr += (4ull * inc);

    list_->SetComputeRootDescriptorTable(1, srvGpu);
    list_->SetComputeRootDescriptorTable(2, uavGpu);

    // Keep heap alive by storing in fusedHeap_ (overwritten each dispatch in non-fused mode)
    fusedHeap_ = heap;

    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
// UPLOAD / BUFFER POOL
// ════════════════════════════════════════════════════════════════════════════════

bool GGUFD3D12Bridge::AllocateBuffer(uint64_t sizeBytes,
                                     Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer) {
    if (!device_ || sizeBytes == 0) return false;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = sizeBytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(device_->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&outBuffer)))) {
        return false;
    }

    state_[outBuffer.Get()].state = D3D12_RESOURCE_STATE_COMMON;
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
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    Microsoft::WRL::ComPtr<ID3D12Resource> gpu;
    if (FAILED(device_->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr,
            IID_PPV_ARGS(&gpu)))) {
        return false;
    }

    // Upload heap: must NOT have UAV flag
    D3D12_RESOURCE_DESC uploadDesc = desc;
    uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3D12Resource> upload;
    if (FAILED(device_->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&upload)))) {
        return false;
    }

    void* mapped = nullptr;
    D3D12_RANGE rr{0, 0};
    if (FAILED(upload->Map(0, &rr, &mapped))) return false;
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

// ════════════════════════════════════════════════════════════════════════════════
// PER-OP DISPATCH (Phase C compatible, each does reset→record→execute→wait)
// ════════════════════════════════════════════════════════════════════════════════

bool GGUFD3D12Bridge::DispatchMatVecQ4(ID3D12Resource* matrixBuffer,
                                       ID3D12Resource* vectorBuffer,
                                       ID3D12Resource* outputBuffer,
                                       uint32_t rows,
                                       uint32_t cols) {
    if (!device_ || !gpu_.rootSig || !gpu_.psoMatVecQ4 || !allocator_ || !list_ || !queue_) return false;
    if (!matrixBuffer || !vectorBuffer || !outputBuffer || rows == 0 || cols == 0) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoMatVecQ4.Get()))) return false;

    transition(list_.Get(), matrixBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), vectorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        matrixBuffer, (uint32_t)(matrixBuffer->GetDesc().Width / 4ull),
        vectorBuffer, cols,
        vectorBuffer, cols,  // placeholder gamma
            nullptr, 0,        // t3 cossin
        outputBuffer, rows,  // u0 placeholder
        outputBuffer, rows); // u1 output

    MatVecConstants c{};
    c.cbRows = rows;  c.cbCols = cols;  c.cbBlocksPerRow = cols / 32u;
    c.cbDim = cols;  c.cbPosition = 0;  c.cbThetaBase = 10000;
    c.cbEps = 1e-5f;  c.cbPad = 0;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((rows + 255u) / 256u, 1, 1);

    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_COMMON);
    return executeAndWait();
}

bool GGUFD3D12Bridge::DispatchRMSNorm(ID3D12Resource* inoutBuffer,
                                      ID3D12Resource* gammaBuffer,
                                      uint32_t dim, float eps) {
    if (!gpu_.psoRMSNorm || !inoutBuffer || !gammaBuffer || dim == 0) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoRMSNorm.Get()))) return false;

    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), gammaBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        nullptr, 0,        // t0 unused
        nullptr, 0,        // t1 unused
        gammaBuffer, dim,  // t2 gamma
            nullptr, 0,        // t3 cossin
        inoutBuffer, dim,  // u0 in-place
        nullptr, 0);       // u1 unused

    MatVecConstants c{};
    c.cbDim = dim;  c.cbEps = eps;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch(1, 1, 1); // single group, 1024 threads
    return executeAndWait();
}

bool GGUFD3D12Bridge::DispatchSoftmax(ID3D12Resource* inoutBuffer, uint32_t dim) {
    if (!gpu_.psoSoftmax || !inoutBuffer || dim == 0) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoSoftmax.Get()))) return false;

    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
        inoutBuffer, dim, nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch(1, 1, 1);
    return executeAndWait();
}

bool GGUFD3D12Bridge::DispatchRoPE(ID3D12Resource* inoutBuffer,
                                   uint32_t dim, uint32_t position,
                                   uint32_t thetaBase) {
    if (!gpu_.psoRoPE || !inoutBuffer || dim == 0) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoRoPE.Get()))) return false;

    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
        inoutBuffer, dim, nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;  c.cbPosition = position;  c.cbThetaBase = thetaBase;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((dim / 2 + 255) / 256, 1, 1);
    return executeAndWait();
}

bool GGUFD3D12Bridge::DispatchSiLU(ID3D12Resource* inoutBuffer, uint32_t dim) {
    if (!gpu_.psoSiLU || !inoutBuffer || dim == 0) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoSiLU.Get()))) return false;

    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
        inoutBuffer, dim, nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((dim + 255) / 256, 1, 1);
    return executeAndWait();
}

bool GGUFD3D12Bridge::DispatchResidualAdd(ID3D12Resource* inoutBuffer,
                                          ID3D12Resource* residualBuffer,
                                          uint32_t dim) {
    if (!gpu_.psoResidualAdd || !inoutBuffer || !residualBuffer || dim == 0) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoResidualAdd.Get()))) return false;

    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), residualBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        nullptr, 0,
        residualBuffer, dim,  // t1 = residual vector
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        inoutBuffer, dim,     // u0 in-place
        nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((dim + 255) / 256, 1, 1);
    return executeAndWait();
}

bool GGUFD3D12Bridge::DispatchMatVecFP32(ID3D12Resource* matrixBuffer,
                                         ID3D12Resource* vectorBuffer,
                                         ID3D12Resource* outputBuffer,
                                         uint32_t rows, uint32_t cols) {
    if (!gpu_.psoMatVecFP32 || !matrixBuffer || !vectorBuffer || !outputBuffer || rows == 0 || cols == 0)
        return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoMatVecFP32.Get()))) return false;

    transition(list_.Get(), matrixBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), vectorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        matrixBuffer, (uint32_t)(matrixBuffer->GetDesc().Width / 4ull),
        vectorBuffer, cols,
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        nullptr, 0,
        outputBuffer, rows);

    MatVecConstants c{};
    c.cbRows = rows;  c.cbCols = cols;  c.cbBlocksPerRow = cols / 32u;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((rows + 255u) / 256u, 1, 1);
    return executeAndWait();
}

bool GGUFD3D12Bridge::DispatchElementwiseMul(ID3D12Resource* aBuffer,
                                             ID3D12Resource* bBuffer,
                                             ID3D12Resource* outputBuffer,
                                             uint32_t dim) {
    if (!gpu_.psoElementwiseMul || !aBuffer || !bBuffer || !outputBuffer || dim == 0)
        return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoElementwiseMul.Get()))) return false;

    transition(list_.Get(), aBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), bBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0,
        bBuffer, dim,
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        aBuffer, dim,
        outputBuffer, dim);

    MatVecConstants c{};
    c.cbDim = dim;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((dim + 255u) / 256u, 1, 1);
    return executeAndWait();
}

// ════════════════════════════════════════════════════════════════════════════════
// FUSED DISPATCH PIPELINE (Phase D)
// Record N kernel dispatches → single execute → single fence wait
// ════════════════════════════════════════════════════════════════════════════════

bool GGUFD3D12Bridge::BeginFusedDispatch() {
    if (!device_ || !allocator_ || !list_) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), nullptr))) return false;

    fusedRecording_ = true;
    fusedOpsRecorded_ = 0;
    return true;
}

// Record helpers: set PSO + descriptors + constants + dispatch, then UAV barrier

bool GGUFD3D12Bridge::RecordMatVecQ4(ID3D12Resource* matrixBuffer,
                                     ID3D12Resource* vectorBuffer,
                                     ID3D12Resource* outputBuffer,
                                     uint32_t rows, uint32_t cols) {
    if (!fusedRecording_ || !gpu_.psoMatVecQ4) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoMatVecQ4.Get());

    transition(list_.Get(), matrixBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), vectorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        matrixBuffer, (uint32_t)(matrixBuffer->GetDesc().Width / 4ull),
        vectorBuffer, cols,
        vectorBuffer, cols,
            nullptr, 0,        // t3 cossin
        outputBuffer, rows,
        outputBuffer, rows);

    MatVecConstants c{};
    c.cbRows = rows;  c.cbCols = cols;  c.cbBlocksPerRow = cols / 32u;
    c.cbDim = cols;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((rows + 255u) / 256u, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::RecordRMSNorm(ID3D12Resource* inoutBuffer,
                                    ID3D12Resource* gammaBuffer,
                                    uint32_t dim, float eps) {
    if (!fusedRecording_ || !gpu_.psoRMSNorm) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoRMSNorm.Get());

    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), gammaBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        nullptr, 0, nullptr, 0,
        gammaBuffer, dim,
        inoutBuffer, dim,
            nullptr, 0,        // t3 cossin
        nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;  c.cbEps = eps;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch(1, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::RecordSoftmax(ID3D12Resource* inoutBuffer, uint32_t dim) {
    if (!fusedRecording_ || !gpu_.psoSoftmax) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoSoftmax.Get());
    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
        inoutBuffer, dim, nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch(1, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::RecordRoPE(ID3D12Resource* inoutBuffer, uint32_t dim,
                                 uint32_t position, uint32_t thetaBase) {
    if (!fusedRecording_ || !gpu_.psoRoPE) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoRoPE.Get());
    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
        inoutBuffer, dim, nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;  c.cbPosition = position;  c.cbThetaBase = thetaBase;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((dim / 2 + 255) / 256, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::RecordSiLU(ID3D12Resource* inoutBuffer, uint32_t dim) {
    if (!fusedRecording_ || !gpu_.psoSiLU) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoSiLU.Get());
    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0, nullptr, 0, nullptr, 0, nullptr, 0,
        inoutBuffer, dim, nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((dim + 255) / 256, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::RecordResidualAdd(ID3D12Resource* inoutBuffer,
                                        ID3D12Resource* residualBuffer,
                                        uint32_t dim) {
    if (!fusedRecording_ || !gpu_.psoResidualAdd) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoResidualAdd.Get());
    transition(list_.Get(), inoutBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), residualBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        nullptr, 0,
        residualBuffer, dim,
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        inoutBuffer, dim,
        nullptr, 0);

    MatVecConstants c{};
    c.cbDim = dim;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((dim + 255) / 256, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::RecordMatVecFP32(ID3D12Resource* matrixBuffer,
                                       ID3D12Resource* vectorBuffer,
                                       ID3D12Resource* outputBuffer,
                                       uint32_t rows, uint32_t cols) {
    if (!fusedRecording_ || !gpu_.psoMatVecFP32) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoMatVecFP32.Get());
    transition(list_.Get(), matrixBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), vectorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        matrixBuffer, (uint32_t)(matrixBuffer->GetDesc().Width / 4ull),
        vectorBuffer, cols,
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        nullptr, 0,
        outputBuffer, rows);

    MatVecConstants c{};
    c.cbRows = rows;  c.cbCols = cols;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((rows + 255u) / 256u, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::RecordElementwiseMul(ID3D12Resource* aBuffer,
                                           ID3D12Resource* bBuffer,
                                           ID3D12Resource* outputBuffer,
                                           uint32_t dim) {
    if (!fusedRecording_ || !gpu_.psoElementwiseMul) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoElementwiseMul.Get());
    transition(list_.Get(), aBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), bBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0,
        bBuffer, dim,
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        aBuffer, dim,
        outputBuffer, dim);

    MatVecConstants c{};
    c.cbDim = dim;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((dim + 255u) / 256u, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::FlushAndWait() {
    if (!fusedRecording_) return false;
    fusedRecording_ = false;

    if (fusedOpsRecorded_ == 0) {
        list_->Close();
        return true;
    }

    return executeAndWait();
}

// ════════════════════════════════════════════════════════════════════════════════
// PHASE E: GPU-RESIDENT KV CACHE
// ════════════════════════════════════════════════════════════════════════════════

bool GGUFD3D12Bridge::AllocateKVCache(uint32_t maxSeqLen, uint32_t nHeads,
                                      uint32_t headDim,
                                      Microsoft::WRL::ComPtr<ID3D12Resource>& outKVBuffer) {
    // Layout: [maxSeqLen positions] × [nHeads heads] × [2 (K+V)] × [headDim floats]
    uint64_t totalFloats = (uint64_t)maxSeqLen * nHeads * 2 * headDim;
    uint64_t sizeBytes = totalFloats * sizeof(float);
    return AllocateBuffer(sizeBytes, outKVBuffer);
}

bool GGUFD3D12Bridge::DispatchKVCacheWrite(ID3D12Resource* kvBuffer,
                                           ID3D12Resource* vecBuffer,
                                           uint32_t pos, uint32_t headIdx, bool isValue,
                                           uint32_t nHeads, uint32_t headDim) {
    if (!gpu_.psoKVCacheWrite || !kvBuffer || !vecBuffer) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoKVCacheWrite.Get()))) return false;

    transition(list_.Get(), kvBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), vecBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        nullptr, 0,
        vecBuffer, headDim,  // t1 = vector to write
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        kvBuffer, (uint32_t)(kvBuffer->GetDesc().Width / sizeof(float)),  // u0 = KV cache
        nullptr, 0);

    MatVecConstants c{};
    c.cbCols = headDim;
    c.cbBlocksPerRow = headIdx * 2 + (isValue ? 1 : 0);
    c.cbDim = nHeads;
    c.cbPosition = pos;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((headDim + 255) / 256, 1, 1);
    return executeAndWait();
}

bool GGUFD3D12Bridge::DispatchAttentionHead(ID3D12Resource* kvBuffer,
                                            ID3D12Resource* queryBuffer,
                                            ID3D12Resource* outputBuffer,
                                            uint32_t seqLen, uint32_t headIdx,
                                            uint32_t nHeads, uint32_t headDim) {
    if (!gpu_.psoAttentionHead || !kvBuffer || !queryBuffer || !outputBuffer) return false;
    if (seqLen == 0 || seqLen > 1024) return false; // thread group limit

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoAttentionHead.Get()))) return false;

    transition(list_.Get(), kvBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), queryBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0,
        queryBuffer, headDim,  // t1 = query vector
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        kvBuffer, (uint32_t)(kvBuffer->GetDesc().Width / sizeof(float)),  // u0 = KV cache
        outputBuffer, headDim);  // u1 = output

    MatVecConstants c{};
    c.cbRows = seqLen;
    c.cbCols = headDim;
    c.cbBlocksPerRow = headIdx;
    c.cbDim = nHeads;
    c.cbEps = 1.0f / sqrtf((float)headDim); // attention scale
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch(1, 1, 1); // single thread group, 1024 threads
    return executeAndWait();
}

// ── Phase E: Fused Record variants ─────────────────────────────────────────

bool GGUFD3D12Bridge::RecordKVCacheWrite(ID3D12Resource* kvBuffer,
                                         ID3D12Resource* vecBuffer,
                                         uint32_t pos, uint32_t headIdx, bool isValue,
                                         uint32_t nHeads, uint32_t headDim) {
    if (!fusedRecording_ || !gpu_.psoKVCacheWrite) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoKVCacheWrite.Get());
    transition(list_.Get(), kvBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), vecBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        nullptr, 0,
        vecBuffer, headDim,
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        kvBuffer, (uint32_t)(kvBuffer->GetDesc().Width / sizeof(float)),
        nullptr, 0);

    MatVecConstants c{};
    c.cbCols = headDim;
    c.cbBlocksPerRow = headIdx * 2 + (isValue ? 1 : 0);
    c.cbDim = nHeads;
    c.cbPosition = pos;
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch((headDim + 255) / 256, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::RecordAttentionHead(ID3D12Resource* kvBuffer,
                                          ID3D12Resource* queryBuffer,
                                          ID3D12Resource* outputBuffer,
                                          uint32_t seqLen, uint32_t headIdx,
                                          uint32_t nHeads, uint32_t headDim) {
    if (!fusedRecording_ || !gpu_.psoAttentionHead) return false;
    if (seqLen == 0 || seqLen > 1024) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoAttentionHead.Get());
    transition(list_.Get(), kvBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), queryBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), outputBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        nullptr, 0,
        queryBuffer, headDim,
        nullptr, 0,
            nullptr, 0,        // t3 cossin
        kvBuffer, (uint32_t)(kvBuffer->GetDesc().Width / sizeof(float)),
        outputBuffer, headDim);

    MatVecConstants c{};
    c.cbRows = seqLen;
    c.cbCols = headDim;
    c.cbBlocksPerRow = headIdx;
    c.cbDim = nHeads;
    c.cbEps = 1.0f / sqrtf((float)headDim);
    list_->SetComputeRoot32BitConstants(0, 8, &c, 0);

    list_->Dispatch(1, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
// READBACK
// ════════════════════════════════════════════════════════════════════════════════

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
            &readbackHeap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
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
    if (FAILED(readback->Map(0, &rr, &mapped))) return false;

    std::memcpy(outBytes, mapped, (size_t)sizeBytes);
    D3D12_RANGE wr{0, 0};
    readback->Unmap(0, &wr);

    state_[gpuBuffer].state = D3D12_RESOURCE_STATE_COPY_SOURCE;
    return true;
}


bool GGUFD3D12Bridge::DispatchRoPEFused(ID3D12Resource* q_buffer,
                                        ID3D12Resource* k_buffer,
                                        ID3D12Resource* cossin_buffer,
                                        uint32_t seq_len,
                                        uint32_t head_dim,
                                        uint32_t num_heads) {
    if (!gpu_.psoRoPEFused || !q_buffer || !k_buffer || !cossin_buffer) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoRoPEFused.Get()))) return false;

    transition(list_.Get(), q_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), k_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), cossin_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        q_buffer, seq_len * num_heads * head_dim,      // t0: Q SRV
        k_buffer, seq_len * num_heads * head_dim,      // t1: K SRV  
        nullptr, 0,                                    // t2: unused
        cossin_buffer, seq_len * (head_dim / 2),       // t3: cossin SRV
        q_buffer, seq_len * num_heads * head_dim,      // u0: Q UAV
        k_buffer, seq_len * num_heads * head_dim       // u1: K UAV
    );

    struct RoPEConstants {
        uint32_t seq_len;
        uint32_t head_dim;
        uint32_t num_heads;
        float theta;
    } constants = { seq_len, head_dim, num_heads, 10000.0f };

    list_->SetComputeRoot32BitConstants(0, 4, &constants, 0);

    uint32_t total_threads = seq_len * num_heads * (head_dim / 2);
    uint32_t groups = (total_threads + 63) / 64;
    if (groups == 0) groups = 1;

    list_->Dispatch(groups, 1, 1);
    return executeAndWait();
}

bool GGUFD3D12Bridge::RecordRoPEFused(ID3D12Resource* q_buffer,
                                      ID3D12Resource* k_buffer,
                                      ID3D12Resource* cossin_buffer,
                                      uint32_t seq_len,
                                      uint32_t head_dim,
                                      uint32_t num_heads) {
    if (!fusedRecording_ || !gpu_.psoRoPEFused || !q_buffer || !k_buffer || !cossin_buffer) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoRoPEFused.Get());

    transition(list_.Get(), q_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), k_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), cossin_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        q_buffer, seq_len * num_heads * head_dim,
        k_buffer, seq_len * num_heads * head_dim,
        nullptr, 0,
        cossin_buffer, seq_len * (head_dim / 2),
        q_buffer, seq_len * num_heads * head_dim,
        k_buffer, seq_len * num_heads * head_dim
    );

    struct RoPEConstants {
        uint32_t seq_len;
        uint32_t head_dim;
        uint32_t num_heads;
        float theta;
    } constants = { seq_len, head_dim, num_heads, 10000.0f };

    list_->SetComputeRoot32BitConstants(0, 4, &constants, 0);

    uint32_t total_threads = seq_len * num_heads * (head_dim / 2);
    uint32_t groups = (total_threads + 63) / 64;
    if (groups == 0) groups = 1;

    list_->Dispatch(groups, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

bool GGUFD3D12Bridge::DispatchCSRoPE_Fused(ID3D12Resource* q_buffer,
                                           ID3D12Resource* k_buffer,
                                           ID3D12Resource* cossin_buffer,
                                           uint32_t seq_pos,
                                           uint32_t head_dim,
                                           uint32_t num_heads) {
    return DispatchRoPEFused(q_buffer, k_buffer, cossin_buffer, seq_pos, head_dim, num_heads);
}

bool GGUFD3D12Bridge::DispatchGEMM(ID3D12Resource* bufferA,
                                   ID3D12Resource* bufferB,
                                   ID3D12Resource* bufferC,
                                   uint32_t M, uint32_t K, uint32_t N) {
    if (!gpu_.psoGEMM || !bufferA || !bufferB || !bufferC || M == 0 || K == 0 || N == 0)
        return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoGEMM.Get()))) return false;

    transition(list_.Get(), bufferA, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), bufferB, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transition(list_.Get(), bufferC, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    setupDescriptorsForDispatch(
        bufferA, (uint32_t)(bufferA->GetDesc().Width / 4ull),
        bufferB, (uint32_t)(bufferB->GetDesc().Width / 4ull),
        nullptr, 0,
        nullptr, 0,
        bufferC, (uint32_t)(bufferC->GetDesc().Width / 4ull),
        nullptr, 0);

    struct GemmConstants {
        uint32_t M;
        uint32_t K;
        uint32_t N;
        uint32_t unused;
    } c{};
    c.M = M;
    c.K = K;
    c.N = N;

    list_->SetComputeRoot32BitConstants(0, 4, &c, 0);

    // TILE_SIZE is 16. Threads per group: (16, 16, 1)
    uint32_t dispatchX = (N + 15) / 16;
    uint32_t dispatchY = (M + 15) / 16;

    list_->Dispatch(dispatchX, dispatchY, 1);
    return executeAndWait();
}

} // namespace RawrXD

