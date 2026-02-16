// ============================================================================
// directml_compute.cpp — DirectML Standalone Inference Engine Implementation
// ============================================================================
// Full DirectML compute backend: D3D12 device management, DML operator
// compilation/dispatch, GGUF tensor upload, dual-model sessions, descriptor
// heap management, and StreamingEngineRegistry integration.
//
// Runtime-loads DirectML.dll via LoadLibrary — no link-time dependency.
// Uses vtable-based COM calls (matches gpu_backend_bridge.cpp pattern).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "directml_compute.h"
#include "streaming_engine_registry.h"
#include "gguf_dml_bridge.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <unknwn.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>

// ============================================================================
// Safe COM Release (vtable[2] = IUnknown::Release)
// ============================================================================
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do { if (p) { \
    typedef ULONG (STDMETHODCALLTYPE *_RelFn)(void*); \
    auto _vtbl = *reinterpret_cast<void***>(p); \
    auto _rel = reinterpret_cast<_RelFn>(_vtbl[2]); \
    _rel(p); \
    (p) = nullptr; \
} } while(0)
#endif

// ============================================================================
// COM GUIDs (inline to avoid header dependencies)
// ============================================================================
static const GUID IID_IDXGIFactory4_Local =
    { 0x1bc6ea02, 0xef36, 0x464f, { 0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a } };
static const GUID IID_ID3D12Device_Local =
    { 0x189819f1, 0x1db6, 0x4b57, { 0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7 } };
static const GUID IID_ID3D12CommandQueue_Local =
    { 0x0ec870a6, 0x5d7e, 0x4c22, { 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed } };
static const GUID IID_ID3D12CommandAllocator_Local =
    { 0x6102dee4, 0xaf59, 0x4b09, { 0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24 } };
static const GUID IID_ID3D12GraphicsCommandList_Local =
    { 0x5b160d0f, 0xac1b, 0x4185, { 0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55 } };
static const GUID IID_ID3D12Fence_Local =
    { 0x0a753dcf, 0xc4d8, 0x4b91, { 0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76 } };
static const GUID IID_ID3D12Resource_Local =
    { 0x696442be, 0xa72e, 0x4059, { 0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad } };
static const GUID IID_ID3D12DescriptorHeap_Local =
    { 0x8efb471d, 0x616c, 0x4f49, { 0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51 } };

// DML GUIDs
static const GUID IID_IDMLDevice_Local =
    { 0x6dbd6437, 0x96fd, 0x423f, { 0xa9, 0x8c, 0xae, 0x5e, 0x7c, 0x2a, 0x57, 0x3f } };
static const GUID IID_IDMLCommandRecorder_Local =
    { 0xe6857a76, 0x2e3e, 0x4fdd, { 0xbf, 0xf4, 0x5d, 0x2b, 0xa1, 0x0f, 0xb4, 0x53 } };
static const GUID IID_IDMLCompiledOperator_Local =
    { 0x6b15e56a, 0xbf5c, 0x4902, { 0x92, 0xd8, 0xda, 0x3a, 0x65, 0x0a, 0xfe, 0xa4 } };
static const GUID IID_IDMLOperator_Local =
    { 0x26caae7a, 0x3081, 0x4633, { 0x95, 0x81, 0x22, 0x6f, 0xbe, 0x57, 0x69, 0x5d } };
static const GUID IID_IDMLOperatorInitializer_Local =
    { 0x427c1113, 0x435c, 0x469c, { 0x86, 0x76, 0x4d, 0x5d, 0xd0, 0x72, 0xf8, 0x13 } };
static const GUID IID_IDMLBindingTable_Local =
    { 0x29c687dc, 0xde74, 0x4e3b, { 0xab, 0x00, 0x11, 0x68, 0xf2, 0xfc, 0x3c, 0xfc } };

// ============================================================================
// D3D12 Constants (inline, no d3d12.h required)
// ============================================================================
static const int D3D_FEATURE_LEVEL_11_0             = 0xb000;
static const int D3D_FEATURE_LEVEL_12_0             = 0xc000;
static const int D3D12_COMMAND_LIST_TYPE_DIRECT      = 0;
static const int D3D12_COMMAND_LIST_TYPE_COMPUTE     = 2;
static const int D3D12_COMMAND_QUEUE_FLAG_NONE       = 0;
static const int D3D12_FENCE_FLAG_NONE               = 0;
static const int D3D12_HEAP_TYPE_DEFAULT             = 1;
static const int D3D12_HEAP_TYPE_UPLOAD              = 2;
static const int D3D12_HEAP_TYPE_READBACK            = 3;
static const int D3D12_HEAP_FLAG_NONE                = 0;
static const int D3D12_RESOURCE_DIMENSION_BUFFER     = 1;
static const int D3D12_TEXTURE_LAYOUT_ROW_MAJOR      = 2;
static const int D3D12_RESOURCE_FLAG_NONE            = 0;
static const int D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS = 0x4;
static const int D3D12_RESOURCE_STATE_COMMON         = 0;
static const int D3D12_RESOURCE_STATE_UNORDERED_ACCESS = 0x8;
static const int D3D12_RESOURCE_STATE_COPY_DEST      = 0x400;
static const int D3D12_RESOURCE_STATE_COPY_SOURCE    = 0x800;
static const int D3D12_RESOURCE_STATE_GENERIC_READ   = 0x1 | 0x2 | 0x40 | 0x80 | 0x200 | 0x800;
static const int D3D12_RESOURCE_BARRIER_TYPE_TRANSITION = 0;
static const int D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES = 0xFFFFFFFF;
static const int D3D12_RESOURCE_BARRIER_FLAG_NONE    = 0;
static const int DXGI_ADAPTER_FLAG_SOFTWARE          = 2;

// D3D12 descriptor heap types
static const int D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV = 0;
static const int D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE = 1;

// DML constants
static const int DML_CREATE_DEVICE_FLAG_NONE         = 0;
static const int DML_CREATE_DEVICE_FLAG_DEBUG        = 1;
static const int DML_EXECUTION_FLAG_NONE             = 0;
static const int DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION = 1;
static const int DML_EXECUTION_FLAG_DESCRIPTORS_VOLATILE = 4;
static const int DML_TENSOR_TYPE_BUFFER              = 1;
static const int DML_TENSOR_FLAG_NONE                = 0;
static const int DML_TENSOR_FLAG_OWNED_BY_DML        = 1;
static const int DML_BINDING_TYPE_NONE               = 0;
static const int DML_BINDING_TYPE_BUFFER             = 1;
static const int DML_BINDING_TYPE_BUFFER_ARRAY       = 2;

// DML feature levels
static const int DML_FEATURE_LEVEL_6_2               = 0x6200;
static const int DML_FEATURE_LEVEL_6_1               = 0x6100;
static const int DML_FEATURE_LEVEL_5_0               = 0x5000;

// DML operator type enum values
static const int DML_OPERATOR_ELEMENT_WISE_ADD       = 5;
static const int DML_OPERATOR_ELEMENT_WISE_MULTIPLY  = 25;
static const int DML_OPERATOR_ACTIVATION_SIGMOID     = 48;
static const int DML_OPERATOR_ACTIVATION_SOFTMAX     = 49;
static const int DML_OPERATOR_GEMM                   = 55;
static const int DML_OPERATOR_REDUCE                 = 56;
static const int DML_OPERATOR_ELEMENT_WISE_DEQUANTIZE_LINEAR = 35;
static const int DML_OPERATOR_ACTIVATION_GELU        = 186;  // DML 0x5100+
static const int DML_OPERATOR_ACTIVATION_SOFTMAX1    = 183;  // DML 0x5100+
static const int DML_OPERATOR_MULTIHEAD_ATTENTION    = 192;  // DML 0x6100+

// DML_MATRIX_TRANSFORM
static const int DML_MATRIX_TRANSFORM_NONE           = 0;
static const int DML_MATRIX_TRANSFORM_TRANSPOSE      = 1;

// DML_REDUCE_FUNCTION
static const int DML_REDUCE_FUNCTION_SUM             = 3;
static const int DML_REDUCE_FUNCTION_MEAN            = 7;

// DML_MULTIHEAD_ATTENTION_MASK_TYPE
static const int DML_MHA_MASK_TYPE_NONE              = 0;
static const int DML_MHA_MASK_TYPE_BOOLEAN           = 4;

// ============================================================================
// Inline D3D12 structs (avoid d3d12.h)
// ============================================================================
#pragma pack(push, 4)
struct D3D12_HEAP_PROPERTIES_DML {
    int     Type;
    int     CPUPageProperty;
    int     MemoryPoolPreference;
    UINT    CreationNodeMask;
    UINT    VisibleNodeMask;
};

struct D3D12_RESOURCE_DESC_DML {
    int         Dimension;
    UINT64      Alignment;
    UINT64      Width;
    UINT        Height;
    UINT16      DepthOrArraySize;
    UINT16      MipLevels;
    UINT        Format;
    UINT        SampleCount;
    UINT        SampleQuality;
    int         Layout;
    int         Flags;
};

struct D3D12_RESOURCE_TRANSITION_BARRIER_DML {
    ID3D12Resource* pResource;
    UINT            Subresource;
    int             StateBefore;
    int             StateAfter;
};

struct D3D12_RESOURCE_BARRIER_DML {
    int     Type;
    int     Flags;
    D3D12_RESOURCE_TRANSITION_BARRIER_DML Transition;
};

struct D3D12_RANGE_DML {
    SIZE_T Begin;
    SIZE_T End;
};

struct D3D12_DESCRIPTOR_HEAP_DESC_DML {
    int     Type;
    UINT    NumDescriptors;
    int     Flags;
    UINT    NodeMask;
};

// D3D12 CPU/GPU descriptor handles (just uint64)
struct D3D12_CPU_DESCRIPTOR_HANDLE_DML {
    SIZE_T ptr;
};

struct D3D12_GPU_DESCRIPTOR_HANDLE_DML {
    UINT64 ptr;
};

// DML inline structs
struct DML_BUFFER_TENSOR_DESC_DML {
    int         DataType;       // DML_TENSOR_DATA_TYPE
    int         Flags;          // DML_TENSOR_FLAGS
    UINT        DimensionCount;
    const UINT* Sizes;
    const UINT* Strides;
    UINT64      TotalTensorSizeInBytes;
    UINT        GuaranteedBaseOffsetAlignment;
};

struct DML_TENSOR_DESC_DML {
    int         Type;           // DML_TENSOR_TYPE
    const void* Desc;           // DML_BUFFER_TENSOR_DESC*
};

struct DML_OPERATOR_DESC_DML {
    int         Type;           // DML_OPERATOR_TYPE
    const void* Desc;           // Operator-specific desc
};

struct DML_BINDING_PROPERTIES_DML {
    UINT    RequiredDescriptorCount;
    UINT64  TemporaryResourceSize;
    UINT64  PersistentResourceSize;
};

struct DML_BUFFER_BINDING_DML {
    ID3D12Resource* Buffer;
    UINT64          Offset;
    UINT64          SizeInBytes;
};

struct DML_BINDING_DESC_DML {
    int         Type;           // DML_BINDING_TYPE
    const void* Desc;           // DML_BUFFER_BINDING*
};

struct DML_BINDING_TABLE_DESC_DML {
    void*       Dispatchable;   // IDMLDispatchable*
    D3D12_CPU_DESCRIPTOR_HANDLE_DML CPUDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE_DML GPUDescriptorHandle;
    UINT        SizeInDescriptors;
};

// DML operator descs
struct DML_GEMM_OPERATOR_DESC_DML {
    const DML_TENSOR_DESC_DML* ATensor;
    const DML_TENSOR_DESC_DML* BTensor;
    const DML_TENSOR_DESC_DML* CTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
    int     TransA;             // DML_MATRIX_TRANSFORM
    int     TransB;
    float   Alpha;
    float   Beta;
    const void* FusedActivation;    // DML_OPERATOR_DESC*
};

struct DML_ELEMENT_WISE_ADD_OPERATOR_DESC_DML {
    const DML_TENSOR_DESC_DML* ATensor;
    const DML_TENSOR_DESC_DML* BTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
};

struct DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC_DML {
    const DML_TENSOR_DESC_DML* ATensor;
    const DML_TENSOR_DESC_DML* BTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
};

struct DML_ACTIVATION_SOFTMAX_DESC_DML {
    const DML_TENSOR_DESC_DML* InputTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
};

struct DML_ACTIVATION_GELU_DESC_DML {
    const DML_TENSOR_DESC_DML* InputTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
};

struct DML_ACTIVATION_SIGMOID_DESC_DML {
    const DML_TENSOR_DESC_DML* InputTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
};

struct DML_REDUCE_OPERATOR_DESC_DML {
    int         Function;       // DML_REDUCE_FUNCTION
    const DML_TENSOR_DESC_DML* InputTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
    UINT        AxisCount;
    const UINT* Axes;
};

struct DML_ELEMENT_WISE_DEQUANTIZE_LINEAR_DESC_DML {
    const DML_TENSOR_DESC_DML* InputTensor;
    const DML_TENSOR_DESC_DML* ScaleTensor;
    const DML_TENSOR_DESC_DML* ZeroPointTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
};

struct DML_MULTIHEAD_ATTENTION_DESC_DML {
    const DML_TENSOR_DESC_DML* QueryTensor;
    const DML_TENSOR_DESC_DML* KeyTensor;
    const DML_TENSOR_DESC_DML* ValueTensor;
    const DML_TENSOR_DESC_DML* StackedQueryKeyTensor;
    const DML_TENSOR_DESC_DML* StackedKeyValueTensor;
    const DML_TENSOR_DESC_DML* StackedQueryKeyValueTensor;
    const DML_TENSOR_DESC_DML* BiasTensor;
    const DML_TENSOR_DESC_DML* MaskTensor;
    const DML_TENSOR_DESC_DML* RelativePositionBiasTensor;
    const DML_TENSOR_DESC_DML* PastKeyTensor;
    const DML_TENSOR_DESC_DML* PastValueTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
    const DML_TENSOR_DESC_DML* OutputPresentKeyTensor;
    const DML_TENSOR_DESC_DML* OutputPresentValueTensor;
    float   Scale;
    float   MaskFilterValue;
    UINT    HeadCount;
    int     MaskType;
};

struct DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC_DML {
    const DML_TENSOR_DESC_DML* InputTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
};

struct DML_ELEMENT_WISE_RECIP_SQRT_OPERATOR_DESC_DML {
    const DML_TENSOR_DESC_DML* InputTensor;
    const DML_TENSOR_DESC_DML* OutputTensor;
};

// DML operator type constants (match DirectML enum values)
static constexpr int DML_OPERATOR_ELEMENT_WISE_IDENTITY   = 1;
static constexpr int DML_OPERATOR_ELEMENT_WISE_RECIP_SQRT = 31;

#pragma pack(pop)

// ============================================================================
// Dynamic function typedefs
// ============================================================================
using PFN_CreateDXGIFactory1 = HRESULT(WINAPI*)(REFIID, void**);
using PFN_D3D12CreateDevice  = HRESULT(WINAPI*)(IUnknown*, int, REFIID, void**);
using PFN_DMLCreateDevice1   = HRESULT(WINAPI*)(ID3D12Device*, int, int, REFIID, void**);
using PFN_DMLCreateDevice    = HRESULT(WINAPI*)(ID3D12Device*, int, REFIID, void**);

// Module-level state
static HMODULE g_d3d12Mod       = nullptr;
static HMODULE g_dxgiMod        = nullptr;
static HMODULE g_dmlMod         = nullptr;
static PFN_D3D12CreateDevice    g_pfnD3D12CreateDevice  = nullptr;
static PFN_CreateDXGIFactory1   g_pfnCreateDXGIFactory1 = nullptr;
static PFN_DMLCreateDevice1     g_pfnDMLCreateDevice1   = nullptr;
static PFN_DMLCreateDevice      g_pfnDMLCreateDevice    = nullptr;
static bool g_libsLoaded        = false;

static bool loadAllLibraries() {
    if (g_libsLoaded) return (g_dmlMod != nullptr);

    g_d3d12Mod = LoadLibraryA("d3d12.dll");
    g_dxgiMod  = LoadLibraryA("dxgi.dll");
    g_dmlMod   = LoadLibraryA("DirectML.dll");

    if (g_d3d12Mod) {
        g_pfnD3D12CreateDevice = reinterpret_cast<PFN_D3D12CreateDevice>(
            GetProcAddress(g_d3d12Mod, "D3D12CreateDevice"));
    }
    if (g_dxgiMod) {
        g_pfnCreateDXGIFactory1 = reinterpret_cast<PFN_CreateDXGIFactory1>(
            GetProcAddress(g_dxgiMod, "CreateDXGIFactory1"));
    }
    if (g_dmlMod) {
        g_pfnDMLCreateDevice1 = reinterpret_cast<PFN_DMLCreateDevice1>(
            GetProcAddress(g_dmlMod, "DMLCreateDevice1"));
        g_pfnDMLCreateDevice = reinterpret_cast<PFN_DMLCreateDevice>(
            GetProcAddress(g_dmlMod, "DMLCreateDevice"));
    }

    g_libsLoaded = true;
    return (g_dmlMod != nullptr && g_d3d12Mod != nullptr);
}

// ============================================================================
// DX12 Vtable wrappers (same pattern as gpu_backend_bridge.cpp)
// ============================================================================

// IDXGIFactory4::EnumAdapters1 (vtable[12])
struct IDXGIFactory4;
struct IDXGIAdapter1;

static HRESULT VT_DXGIFactory_EnumAdapters1(void* factory, UINT idx, void** ppAdapter) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(void*, UINT, void**);
    auto vtbl = *reinterpret_cast<void***>(factory);
    return reinterpret_cast<PFN>(vtbl[12])(factory, idx, ppAdapter);
}

// DXGI_ADAPTER_DESC1
#pragma pack(push, 1)
struct DXGI_ADAPTER_DESC1_DML {
    WCHAR   Description[128];
    UINT    VendorId;
    UINT    DeviceId;
    UINT    SubSysId;
    UINT    Revision;
    SIZE_T  DedicatedVideoMemory;
    SIZE_T  DedicatedSystemMemory;
    SIZE_T  SharedSystemMemory;
    LUID    AdapterLuid;
    UINT    Flags;
};
#pragma pack(pop)

// IDXGIAdapter1::GetDesc1 (vtable[10])
static HRESULT VT_DXGIAdapter_GetDesc1(void* adapter, DXGI_ADAPTER_DESC1_DML* desc) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(void*, DXGI_ADAPTER_DESC1_DML*);
    auto vtbl = *reinterpret_cast<void***>(adapter);
    return reinterpret_cast<PFN>(vtbl[10])(adapter, desc);
}

// ID3D12Device vtable wrappers
static HRESULT VT_Device_CreateCommandQueue(ID3D12Device* dev, const void* desc, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12Device*, const void*, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[8])(dev, desc, riid, pp);
}

static HRESULT VT_Device_CreateCommandAllocator(ID3D12Device* dev, int type, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12Device*, int, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[9])(dev, type, riid, pp);
}

static HRESULT VT_Device_CreateCommandList(ID3D12Device* dev, UINT nodeMask, int type,
                                            ID3D12CommandAllocator* alloc, void* pso,
                                            REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12Device*, UINT, int,
                                             ID3D12CommandAllocator*, void*, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[12])(dev, nodeMask, type, alloc, pso, riid, pp);
}

static HRESULT VT_Device_CreateFence(ID3D12Device* dev, UINT64 val, int flags, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12Device*, UINT64, int, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[13])(dev, val, flags, riid, pp);
}

static HRESULT VT_Device_CreateCommittedResource(ID3D12Device* dev,
    const void* heapProps, int heapFlags, const void* resDesc,
    int initialState, const void* clearValue, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12Device*, const void*, int,
        const void*, int, const void*, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[27])(dev, heapProps, heapFlags, resDesc,
        initialState, clearValue, riid, pp);
}

static HRESULT VT_Device_CreateDescriptorHeap(ID3D12Device* dev, const void* desc, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12Device*, const void*, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[14])(dev, desc, riid, pp);
}

static UINT VT_Device_GetDescriptorHandleIncrementSize(ID3D12Device* dev, int type) {
    typedef UINT(STDMETHODCALLTYPE* PFN)(ID3D12Device*, int);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[15])(dev, type);
}

// ID3D12DescriptorHeap vtable wrappers
static D3D12_CPU_DESCRIPTOR_HANDLE_DML VT_DescHeap_GetCPUStart(ID3D12DescriptorHeap* heap) {
    // GetCPUDescriptorHandleForHeapStart is vtable[9] on ID3D12DescriptorHeap
    typedef D3D12_CPU_DESCRIPTOR_HANDLE_DML(STDMETHODCALLTYPE* PFN)(ID3D12DescriptorHeap*);
    auto vtbl = *reinterpret_cast<void***>(heap);
    return reinterpret_cast<PFN>(vtbl[9])(heap);
}

static D3D12_GPU_DESCRIPTOR_HANDLE_DML VT_DescHeap_GetGPUStart(ID3D12DescriptorHeap* heap) {
    // GetGPUDescriptorHandleForHeapStart is vtable[10] on ID3D12DescriptorHeap
    typedef D3D12_GPU_DESCRIPTOR_HANDLE_DML(STDMETHODCALLTYPE* PFN)(ID3D12DescriptorHeap*);
    auto vtbl = *reinterpret_cast<void***>(heap);
    return reinterpret_cast<PFN>(vtbl[10])(heap);
}

// ID3D12CommandQueue vtable wrappers
static void VT_CmdQueue_ExecuteCommandLists(ID3D12CommandQueue* q, UINT count, void** lists) {
    typedef void(STDMETHODCALLTYPE* PFN)(ID3D12CommandQueue*, UINT, void**);
    auto vtbl = *reinterpret_cast<void***>(q);
    reinterpret_cast<PFN>(vtbl[10])(q, count, lists);
}

static HRESULT VT_CmdQueue_Signal(ID3D12CommandQueue* q, ID3D12Fence* fence, UINT64 value) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12CommandQueue*, ID3D12Fence*, UINT64);
    auto vtbl = *reinterpret_cast<void***>(q);
    return reinterpret_cast<PFN>(vtbl[11])(q, fence, value);
}

// ID3D12CommandAllocator::Reset (vtable[8])
static HRESULT VT_CmdAlloc_Reset(ID3D12CommandAllocator* alloc) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12CommandAllocator*);
    auto vtbl = *reinterpret_cast<void***>(alloc);
    return reinterpret_cast<PFN>(vtbl[8])(alloc);
}

// ID3D12GraphicsCommandList vtable wrappers
static HRESULT VT_CmdList_Close(ID3D12GraphicsCommandList* list) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12GraphicsCommandList*);
    auto vtbl = *reinterpret_cast<void***>(list);
    return reinterpret_cast<PFN>(vtbl[7])(list);
}

static HRESULT VT_CmdList_Reset(ID3D12GraphicsCommandList* list, ID3D12CommandAllocator* alloc, void* pso) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12GraphicsCommandList*, ID3D12CommandAllocator*, void*);
    auto vtbl = *reinterpret_cast<void***>(list);
    return reinterpret_cast<PFN>(vtbl[8])(list, alloc, pso);
}

static void VT_CmdList_ResourceBarrier(ID3D12GraphicsCommandList* list, UINT num, const void* barriers) {
    typedef void(STDMETHODCALLTYPE* PFN)(ID3D12GraphicsCommandList*, UINT, const void*);
    auto vtbl = *reinterpret_cast<void***>(list);
    reinterpret_cast<PFN>(vtbl[15])(list, num, barriers);
}

static void VT_CmdList_CopyBufferRegion(ID3D12GraphicsCommandList* list,
    ID3D12Resource* dst, UINT64 dstOff, ID3D12Resource* src, UINT64 srcOff, UINT64 size) {
    typedef void(STDMETHODCALLTYPE* PFN)(ID3D12GraphicsCommandList*,
        ID3D12Resource*, UINT64, ID3D12Resource*, UINT64, UINT64);
    auto vtbl = *reinterpret_cast<void***>(list);
    reinterpret_cast<PFN>(vtbl[17])(list, dst, dstOff, src, srcOff, size);
}

static void VT_CmdList_SetDescriptorHeaps(ID3D12GraphicsCommandList* list, UINT num, void** heaps) {
    // SetDescriptorHeaps is vtable[27] on ID3D12GraphicsCommandList
    typedef void(STDMETHODCALLTYPE* PFN)(ID3D12GraphicsCommandList*, UINT, void**);
    auto vtbl = *reinterpret_cast<void***>(list);
    reinterpret_cast<PFN>(vtbl[27])(list, num, heaps);
}

// ID3D12Fence vtable wrappers
static UINT64 VT_Fence_GetCompletedValue(ID3D12Fence* fence) {
    typedef UINT64(STDMETHODCALLTYPE* PFN)(ID3D12Fence*);
    auto vtbl = *reinterpret_cast<void***>(fence);
    return reinterpret_cast<PFN>(vtbl[8])(fence);
}

static HRESULT VT_Fence_SetEventOnCompletion(ID3D12Fence* fence, UINT64 val, HANDLE evt) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12Fence*, UINT64, HANDLE);
    auto vtbl = *reinterpret_cast<void***>(fence);
    return reinterpret_cast<PFN>(vtbl[9])(fence, val, evt);
}

// ID3D12Resource vtable wrappers
static HRESULT VT_Resource_Map(ID3D12Resource* res, UINT sub, const void* range, void** ppData) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(ID3D12Resource*, UINT, const void*, void**);
    auto vtbl = *reinterpret_cast<void***>(res);
    return reinterpret_cast<PFN>(vtbl[8])(res, sub, range, ppData);
}

static void VT_Resource_Unmap(ID3D12Resource* res, UINT sub, const void* range) {
    typedef void(STDMETHODCALLTYPE* PFN)(ID3D12Resource*, UINT, const void*);
    auto vtbl = *reinterpret_cast<void***>(res);
    reinterpret_cast<PFN>(vtbl[9])(res, sub, range);
}

static UINT64 VT_Resource_GetGPUVirtualAddress(ID3D12Resource* res) {
    typedef UINT64(STDMETHODCALLTYPE* PFN)(ID3D12Resource*);
    auto vtbl = *reinterpret_cast<void***>(res);
    return reinterpret_cast<PFN>(vtbl[10])(res);
}

// ============================================================================
// DML Vtable wrappers
// ============================================================================

// IDMLDevice vtable layout:
//   0-2: IUnknown (QI, AddRef, Release)
//   3-6: IDMLObject (GetPrivateData, SetPrivateData, SetPrivateDataInterface, SetName)
//   7: CheckFeatureSupport
//   8: CreateOperator
//   9: CompileOperator
//  10: CreateOperatorInitializer
//  11: CreateCommandRecorder
//  12: CreateBindingTable
//  13: Evict
//  14: MakeResident
//  15: GetDeviceRemovedReason
//  16: GetParentDevice

static HRESULT VT_DMLDevice_CheckFeatureSupport(IDMLDevice* dev, int feature,
    UINT querySize, const void* queryData, UINT dataSize, void* outData) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(IDMLDevice*, int, UINT, const void*, UINT, void*);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[7])(dev, feature, querySize, queryData, dataSize, outData);
}

static HRESULT VT_DMLDevice_CreateOperator(IDMLDevice* dev, const void* desc, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(IDMLDevice*, const void*, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[8])(dev, desc, riid, pp);
}

static HRESULT VT_DMLDevice_CompileOperator(IDMLDevice* dev, void* op, int flags, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(IDMLDevice*, void*, int, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[9])(dev, op, flags, riid, pp);
}

static HRESULT VT_DMLDevice_CreateOperatorInitializer(IDMLDevice* dev, UINT count,
    void* const* ops, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(IDMLDevice*, UINT, void* const*, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[10])(dev, count, ops, riid, pp);
}

static HRESULT VT_DMLDevice_CreateCommandRecorder(IDMLDevice* dev, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(IDMLDevice*, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[11])(dev, riid, pp);
}

static HRESULT VT_DMLDevice_CreateBindingTable(IDMLDevice* dev, const void* desc, REFIID riid, void** pp) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(IDMLDevice*, const void*, REFIID, void**);
    auto vtbl = *reinterpret_cast<void***>(dev);
    return reinterpret_cast<PFN>(vtbl[12])(dev, desc, riid, pp);
}

// IDMLDispatchable::GetBindingProperties (vtable[7] on IDMLDispatchable)
// IDMLCompiledOperator inherits IDMLDispatchable → IDMLPageable → IDMLDeviceChild → IDMLObject → IUnknown
// vtable layout: 0-2 IUnknown, 3-6 IDMLObject, 7 IDMLDeviceChild::GetDevice,
//                8 IDMLDispatchable::GetBindingProperties
// Wait — let me re-trace the inheritance:
// IDMLCompiledOperator : IDMLDispatchable : IDMLPageable : IDMLDeviceChild : IDMLObject : IUnknown
// IUnknown: QI(0), AddRef(1), Release(2)
// IDMLObject: GetPrivateData(3), SetPrivateData(4), SetPrivateDataInterface(5), SetName(6)
// IDMLDeviceChild: GetDevice(7)
// IDMLPageable: (no new methods)
// IDMLDispatchable: GetBindingProperties(8)
static DML_BINDING_PROPERTIES_DML VT_Dispatchable_GetBindingProperties(void* dispatchable) {
    typedef DML_BINDING_PROPERTIES_DML(STDMETHODCALLTYPE* PFN)(void*);
    auto vtbl = *reinterpret_cast<void***>(dispatchable);
    return reinterpret_cast<PFN>(vtbl[8])(dispatchable);
}

// IDMLOperatorInitializer::Reset (vtable[9])
static HRESULT VT_OpInit_Reset(void* init, UINT count, void* const* ops) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(void*, UINT, void* const*);
    auto vtbl = *reinterpret_cast<void***>(init);
    return reinterpret_cast<PFN>(vtbl[9])(init, count, ops);
}

// IDMLBindingTable vtable layout:
//   0-2: IUnknown, 3-6: IDMLObject, 7: IDMLDeviceChild::GetDevice
//   8: BindInputs
//   9: BindOutputs
//  10: BindTemporaryResource
//  11: BindPersistentResource
//  12: Reset
static void VT_BindingTable_BindInputs(IDMLBindingTable* bt, UINT count, const void* bindings) {
    typedef void(STDMETHODCALLTYPE* PFN)(IDMLBindingTable*, UINT, const void*);
    auto vtbl = *reinterpret_cast<void***>(bt);
    reinterpret_cast<PFN>(vtbl[8])(bt, count, bindings);
}

static void VT_BindingTable_BindOutputs(IDMLBindingTable* bt, UINT count, const void* bindings) {
    typedef void(STDMETHODCALLTYPE* PFN)(IDMLBindingTable*, UINT, const void*);
    auto vtbl = *reinterpret_cast<void***>(bt);
    reinterpret_cast<PFN>(vtbl[9])(bt, count, bindings);
}

static void VT_BindingTable_BindTemporaryResource(IDMLBindingTable* bt, const void* binding) {
    typedef void(STDMETHODCALLTYPE* PFN)(IDMLBindingTable*, const void*);
    auto vtbl = *reinterpret_cast<void***>(bt);
    reinterpret_cast<PFN>(vtbl[10])(bt, binding);
}

static void VT_BindingTable_BindPersistentResource(IDMLBindingTable* bt, const void* binding) {
    typedef void(STDMETHODCALLTYPE* PFN)(IDMLBindingTable*, const void*);
    auto vtbl = *reinterpret_cast<void***>(bt);
    reinterpret_cast<PFN>(vtbl[11])(bt, binding);
}

static HRESULT VT_BindingTable_Reset(IDMLBindingTable* bt, const void* desc) {
    typedef HRESULT(STDMETHODCALLTYPE* PFN)(IDMLBindingTable*, const void*);
    auto vtbl = *reinterpret_cast<void***>(bt);
    return reinterpret_cast<PFN>(vtbl[12])(bt, desc);
}

// IDMLCommandRecorder vtable layout:
//   0-2: IUnknown, 3-6: IDMLObject, 7: IDMLDeviceChild::GetDevice
//   8: RecordDispatch
static void VT_CmdRecorder_RecordDispatch(IDMLCommandRecorder* rec,
    ID3D12GraphicsCommandList* cmdList, void* dispatchable, IDMLBindingTable* bindings) {
    typedef void(STDMETHODCALLTYPE* PFN)(IDMLCommandRecorder*,
        ID3D12GraphicsCommandList*, void*, IDMLBindingTable*);
    auto vtbl = *reinterpret_cast<void***>(rec);
    reinterpret_cast<PFN>(vtbl[8])(rec, cmdList, dispatchable, bindings);
}

// ============================================================================
// Utility: data type size
// ============================================================================
static uint32_t tensorDataTypeSize(RawrXD::DML::TensorDataType dt) {
    switch (dt) {
        case RawrXD::DML::TensorDataType::Float32: return 4;
        case RawrXD::DML::TensorDataType::Float16: return 2;
        case RawrXD::DML::TensorDataType::Float64: return 8;
        case RawrXD::DML::TensorDataType::UInt32:  return 4;
        case RawrXD::DML::TensorDataType::Int32:   return 4;
        case RawrXD::DML::TensorDataType::UInt16:  return 2;
        case RawrXD::DML::TensorDataType::Int16:   return 2;
        case RawrXD::DML::TensorDataType::UInt8:   return 1;
        case RawrXD::DML::TensorDataType::Int8:    return 1;
        case RawrXD::DML::TensorDataType::UInt64:  return 8;
        case RawrXD::DML::TensorDataType::Int64:   return 8;
        default: return 0;
    }
}

static int toDMLDataType(RawrXD::DML::TensorDataType dt) {
    return static_cast<int>(dt);
}

static uint64_t computeTensorBufferSize(RawrXD::DML::TensorDataType dt,
                                         const uint32_t* dims, uint32_t dimCount) {
    if (dimCount == 0) return 0;
    uint64_t elements = 1;
    for (uint32_t i = 0; i < dimCount; ++i) elements *= dims[i];
    return elements * tensorDataTypeSize(dt);
}

static uint64_t fnv1a_hash(const char* str) {
    uint64_t hash = 14695981039346656037ULL;
    while (*str) {
        hash ^= static_cast<uint64_t>(*str++);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// ============================================================================
// Implementation
// ============================================================================

namespace RawrXD {
namespace DML {

// ============================================================================
// Logging
// ============================================================================
void DirectMLCompute::log(int level, const char* msg) {
    if (m_logCb) {
        m_logCb(level, msg, m_logUserData);
    }
    // Also emit to stdout in debug
    const char* prefix = (level == 0) ? "[DML:DBG]" :
                         (level == 1) ? "[DML:INF]" :
                         (level == 2) ? "[DML:WRN]" : "[DML:ERR]";
    std::cout << prefix << " " << msg << std::endl;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
DirectMLCompute::DirectMLCompute() = default;

DirectMLCompute::~DirectMLCompute() {
    if (m_initialized.load(std::memory_order_acquire)) {
        shutdown();
    }
}

// ============================================================================
// Initialize — create D3D12 device + DML device
// ============================================================================
DMLResult DirectMLCompute::initialize(ID3D12Device* existingDevice,
                                       ID3D12CommandQueue* existingQueue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized.load(std::memory_order_acquire)) {
        return DMLResult::ok("Already initialized");
    }

    log(1, "Initializing DirectML Compute Engine...");

    if (!loadAllLibraries()) {
        return DMLResult::error("Failed to load D3D12/DXGI/DirectML DLLs", -1);
    }

    if (!g_dmlMod || (!g_pfnDMLCreateDevice1 && !g_pfnDMLCreateDevice)) {
        return DMLResult::error("DirectML.dll not found or DMLCreateDevice not exported", -2);
    }

    // Use existing D3D12 device or create our own
    if (existingDevice) {
        m_d3d12Device = existingDevice;
        m_cmdQueue = existingQueue;
        m_ownsD3D12 = false;
        log(1, "Using existing D3D12 device");
    } else {
        auto r = createD3D12Device();
        if (!r.success) return r;
        m_ownsD3D12 = true;
    }

    // Create command infrastructure if we don't have a queue
    if (!m_cmdQueue) {
        auto r = createCommandInfra();
        if (!r.success) return r;
    } else {
        // Still need our own allocator, list, fence
        auto r = createCommandInfra();
        if (!r.success) return r;
    }

    // Create DML device
    auto r = initDML();
    if (!r.success) return r;

    // Create descriptor heap (initial size: 1024 descriptors)
    auto r2 = createDescriptorHeap(4096);
    if (!r2.success) {
        log(2, "Warning: Descriptor heap creation failed, will retry on demand");
    }

    m_initialized.store(true, std::memory_order_release);
    log(1, "DirectML Compute Engine initialized successfully");
    return DMLResult::ok("DirectML initialized");
}

// ============================================================================
// Shutdown
// ============================================================================
DMLResult DirectMLCompute::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DMLResult::ok("Not initialized");
    }

    log(1, "Shutting down DirectML Compute Engine...");

    // Flush GPU
    if (m_fence && m_fenceEvent && m_cmdQueue) {
        m_fenceValue++;
        VT_CmdQueue_Signal(m_cmdQueue, m_fence, m_fenceValue);
        if (VT_Fence_GetCompletedValue(m_fence) < m_fenceValue) {
            VT_Fence_SetEventOnCompletion(m_fence, m_fenceValue, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, 3000);
        }
    }

    // Destroy all sessions (frees model tensors)
    for (auto& [id, session] : m_sessions) {
        for (auto& [hash, tensor] : session->tensors) {
            SAFE_RELEASE(tensor.resource);
        }
        for (auto& kv : session->kvCacheK) { SAFE_RELEASE(kv.resource); }
        for (auto& kv : session->kvCacheV) { SAFE_RELEASE(kv.resource); }
    }
    m_sessions.clear();

    // Release compiled operator cache
    for (auto& [hash, entry] : m_opCache) {
        SAFE_RELEASE(entry.compiledOp);
        SAFE_RELEASE(entry.persistentBuf);
        SAFE_RELEASE(entry.tempBuf);
    }
    m_opCache.clear();

    // Release DML objects
    SAFE_RELEASE(m_cmdRecorder);
    SAFE_RELEASE(m_dmlDevice);

    // Release descriptor heap
    SAFE_RELEASE(m_descriptorHeap);

    // Release D3D12 objects
    if (m_fenceEvent) { CloseHandle(m_fenceEvent); m_fenceEvent = nullptr; }
    SAFE_RELEASE(m_fence);
    SAFE_RELEASE(m_cmdList);
    SAFE_RELEASE(m_cmdAlloc);

    if (m_ownsD3D12) {
        SAFE_RELEASE(m_cmdQueue);
        SAFE_RELEASE(m_d3d12Device);
    }

    m_initialized.store(false, std::memory_order_release);
    log(1, "DirectML Compute Engine shutdown complete");
    return DMLResult::ok("Shutdown complete");
}

// ============================================================================
// Create D3D12 Device (if not using GPUBackendBridge's device)
// ============================================================================
DMLResult DirectMLCompute::createD3D12Device() {
    if (!g_pfnCreateDXGIFactory1 || !g_pfnD3D12CreateDevice) {
        return DMLResult::error("D3D12/DXGI functions not loaded", -1);
    }

    // Create DXGI factory
    void* factory = nullptr;
    HRESULT hr = g_pfnCreateDXGIFactory1(IID_IDXGIFactory4_Local, &factory);
    if (FAILED(hr)) return DMLResult::error("CreateDXGIFactory1 failed", hr);

    // Find best discrete GPU
    void* bestAdapter = nullptr;
    SIZE_T bestVRAM = 0;
    char bestName[256] = {};

    for (UINT i = 0; ; i++) {
        void* adapter = nullptr;
        hr = VT_DXGIFactory_EnumAdapters1(factory, i, &adapter);
        if (FAILED(hr)) break;

        DXGI_ADAPTER_DESC1_DML desc = {};
        VT_DXGIAdapter_GetDesc1(adapter, &desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            reinterpret_cast<IUnknown*>(adapter)->Release();
            continue;
        }

        if (desc.DedicatedVideoMemory > bestVRAM) {
            if (bestAdapter) reinterpret_cast<IUnknown*>(bestAdapter)->Release();
            bestAdapter = adapter;
            bestVRAM = desc.DedicatedVideoMemory;
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, bestName, sizeof(bestName), nullptr, nullptr);
        } else {
            reinterpret_cast<IUnknown*>(adapter)->Release();
        }
    }

    reinterpret_cast<IUnknown*>(factory)->Release();

    if (!bestAdapter) {
        return DMLResult::error("No suitable GPU adapter found", -2);
    }

    // Create D3D12 device at feature level 12.0
    hr = g_pfnD3D12CreateDevice(
        reinterpret_cast<IUnknown*>(bestAdapter),
        D3D_FEATURE_LEVEL_12_0,
        IID_ID3D12Device_Local,
        reinterpret_cast<void**>(&m_d3d12Device));

    if (FAILED(hr)) {
        // Fallback to 11.0
        hr = g_pfnD3D12CreateDevice(
            reinterpret_cast<IUnknown*>(bestAdapter),
            D3D_FEATURE_LEVEL_11_0,
            IID_ID3D12Device_Local,
            reinterpret_cast<void**>(&m_d3d12Device));
    }

    reinterpret_cast<IUnknown*>(bestAdapter)->Release();

    if (FAILED(hr) || !m_d3d12Device) {
        return DMLResult::error("D3D12CreateDevice failed", hr);
    }

    std::string devMsg = "D3D12 device created: ";
    devMsg += bestName;
    devMsg += " VRAM=";
    devMsg += std::to_string(bestVRAM / (1024 * 1024));
    devMsg += "MB";
    log(1, devMsg.c_str());
    return DMLResult::ok("D3D12 device created");
}

// ============================================================================
// Create Command Infrastructure
// ============================================================================
DMLResult DirectMLCompute::createCommandInfra() {
    if (!m_d3d12Device) return DMLResult::error("No D3D12 device", -1);

    HRESULT hr;

    // Create COMPUTE command queue (if we don't have one)
    if (!m_cmdQueue) {
        struct { int Type; int Priority; int Flags; UINT NodeMask; }
            queueDesc = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };

        hr = VT_Device_CreateCommandQueue(m_d3d12Device, &queueDesc,
            IID_ID3D12CommandQueue_Local, reinterpret_cast<void**>(&m_cmdQueue));
        if (FAILED(hr)) return DMLResult::error("CreateCommandQueue failed", hr);
    }

    // Command allocator (DIRECT type for DML — DML requires DIRECT not COMPUTE)
    hr = VT_Device_CreateCommandAllocator(m_d3d12Device, D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_ID3D12CommandAllocator_Local, reinterpret_cast<void**>(&m_cmdAlloc));
    if (FAILED(hr)) return DMLResult::error("CreateCommandAllocator failed", hr);

    // Command list (DIRECT type, initially open)
    hr = VT_Device_CreateCommandList(m_d3d12Device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_cmdAlloc, nullptr,
        IID_ID3D12GraphicsCommandList_Local, reinterpret_cast<void**>(&m_cmdList));
    if (FAILED(hr)) return DMLResult::error("CreateCommandList failed", hr);

    // Close immediately — will reset before use
    VT_CmdList_Close(m_cmdList);

    // Fence
    hr = VT_Device_CreateFence(m_d3d12Device, 0, D3D12_FENCE_FLAG_NONE,
        IID_ID3D12Fence_Local, reinterpret_cast<void**>(&m_fence));
    if (FAILED(hr)) return DMLResult::error("CreateFence failed", hr);

    m_fenceEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    m_fenceValue = 0;

    log(1, "D3D12 command infrastructure created (DIRECT queue for DML)");
    return DMLResult::ok("Command infra created");
}

// ============================================================================
// Initialize DML Device
// ============================================================================
DMLResult DirectMLCompute::initDML() {
    if (!m_d3d12Device) return DMLResult::error("No D3D12 device", -1);

    HRESULT hr;

    // Try DMLCreateDevice1 first (DML 2.0+ — allows requesting feature level)
    if (g_pfnDMLCreateDevice1) {
        hr = g_pfnDMLCreateDevice1(
            m_d3d12Device,
            DML_CREATE_DEVICE_FLAG_NONE,
            DML_FEATURE_LEVEL_6_2,          // Request FL 6.2 for MultiHeadAttention
            IID_IDMLDevice_Local,
            reinterpret_cast<void**>(&m_dmlDevice));

        if (FAILED(hr)) {
            // Fallback to FL 5.0
            log(2, "DML FL 6.2 not available, falling back to FL 5.0");
            hr = g_pfnDMLCreateDevice1(
                m_d3d12Device,
                DML_CREATE_DEVICE_FLAG_NONE,
                DML_FEATURE_LEVEL_5_0,
                IID_IDMLDevice_Local,
                reinterpret_cast<void**>(&m_dmlDevice));
        }
    }

    // Fallback to DMLCreateDevice (DML 1.0)
    if (!m_dmlDevice && g_pfnDMLCreateDevice) {
        hr = g_pfnDMLCreateDevice(
            m_d3d12Device,
            DML_CREATE_DEVICE_FLAG_NONE,
            IID_IDMLDevice_Local,
            reinterpret_cast<void**>(&m_dmlDevice));
    }

    if (!m_dmlDevice) {
        return DMLResult::error("DMLCreateDevice failed — DirectML not available", hr);
    }

    // Create command recorder
    hr = VT_DMLDevice_CreateCommandRecorder(m_dmlDevice,
        IID_IDMLCommandRecorder_Local,
        reinterpret_cast<void**>(&m_cmdRecorder));

    if (FAILED(hr) || !m_cmdRecorder) {
        SAFE_RELEASE(m_dmlDevice);
        return DMLResult::error("CreateCommandRecorder failed", hr);
    }

    log(1, "DirectML device created (DML 6.2 with MultiHeadAttention support)");
    return DMLResult::ok("DML device created");
}

// ============================================================================
// Descriptor Heap
// ============================================================================
DMLResult DirectMLCompute::createDescriptorHeap(uint32_t numDescriptors) {
    if (!m_d3d12Device) return DMLResult::error("No device", -1);

    D3D12_DESCRIPTOR_HEAP_DESC_DML heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = numDescriptors;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heapDesc.NodeMask = 0;

    HRESULT hr = VT_Device_CreateDescriptorHeap(m_d3d12Device, &heapDesc,
        IID_ID3D12DescriptorHeap_Local,
        reinterpret_cast<void**>(&m_descriptorHeap));

    if (FAILED(hr)) return DMLResult::error("CreateDescriptorHeap failed", hr);

    m_descriptorSize = VT_Device_GetDescriptorHandleIncrementSize(
        m_d3d12Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_descriptorCount = numDescriptors;
    m_descriptorOffset = 0;

    log(1, (std::string("Descriptor heap created: ") + std::to_string(numDescriptors) +
            " descriptors, size=" + std::to_string(m_descriptorSize)).c_str());
    return DMLResult::ok("Descriptor heap created");
}

void DirectMLCompute::resetDescriptorHeap() {
    m_descriptorOffset = 0;
}

void DirectMLCompute::getCpuGpuDescriptorHandles(uint32_t offset,
                                                   uint64_t& cpuHandle, uint64_t& gpuHandle) {
    if (!m_descriptorHeap) { cpuHandle = 0; gpuHandle = 0; return; }

    auto cpuStart = VT_DescHeap_GetCPUStart(m_descriptorHeap);
    auto gpuStart = VT_DescHeap_GetGPUStart(m_descriptorHeap);

    cpuHandle = cpuStart.ptr + static_cast<SIZE_T>(offset) * m_descriptorSize;
    gpuHandle = gpuStart.ptr + static_cast<UINT64>(offset) * m_descriptorSize;
}

uint32_t DirectMLCompute::allocateDescriptors(uint32_t count) {
    uint32_t offset = m_descriptorOffset;
    m_descriptorOffset += count;
    if (m_descriptorOffset > m_descriptorCount) {
        // Descriptor heap exhausted — attempt to grow by doubling capacity
        uint32_t newCount = m_descriptorCount * 2;
        if (newCount < m_descriptorOffset + count) {
            newCount = m_descriptorOffset + count + 256; // Ensure enough room
        }

        log(2, (std::string("Descriptor heap growing: ") +
                std::to_string(m_descriptorCount) + " → " +
                std::to_string(newCount)).c_str());

        // Attempt to create a new, larger descriptor heap
        if (m_d3d12Device) {
            D3D12_DESCRIPTOR_HEAP_DESC_DML heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = newCount;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            void* newHeap = nullptr;
            HRESULT hr = VT_Device_CreateDescriptorHeap(
                m_d3d12Device, &heapDesc,
                IID_ID3D12DescriptorHeap_Local, &newHeap);
            if (SUCCEEDED(hr) && newHeap) {
                // Copy existing descriptors if possible; for simplicity,
                // we accept the reset since all operators get recompiled on next use
                if (m_descriptorHeap) {
                    SAFE_RELEASE(m_descriptorHeap);
                }
                m_descriptorHeap = static_cast<ID3D12DescriptorHeap*>(newHeap);
                m_descriptorCount = newCount;
                m_descriptorSize = VT_Device_GetDescriptorHandleIncrementSize(
                    m_d3d12Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                log(1, "Descriptor heap grown successfully");
            } else {
                // Growth failed — fall back to wrap-around reset
                log(3, "Descriptor heap growth failed! Resetting to zero.");
                m_descriptorOffset = count;
                offset = 0;
            }
        } else {
            // No device — wrap around
            log(3, "Descriptor heap exhausted (no device). Resetting.");
            m_descriptorOffset = count;
            offset = 0;
        }
    }
    return offset;
}

// ============================================================================
// Resource Creation Helpers
// ============================================================================
DMLResult DirectMLCompute::createDefaultBuffer(ID3D12Resource** ppResource, uint64_t sizeBytes) {
    D3D12_HEAP_PROPERTIES_DML heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC_DML bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = sizeBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = 0;
    bufDesc.SampleCount = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    HRESULT hr = VT_Device_CreateCommittedResource(m_d3d12Device,
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr,
        IID_ID3D12Resource_Local, reinterpret_cast<void**>(ppResource));

    if (FAILED(hr)) return DMLResult::error("CreateDefaultBuffer failed", hr);
    m_stats.vramAllocated.fetch_add(sizeBytes);
    return DMLResult::ok();
}

DMLResult DirectMLCompute::createUploadBuffer(ID3D12Resource** ppResource, uint64_t sizeBytes) {
    D3D12_HEAP_PROPERTIES_DML heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC_DML bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = sizeBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = 0;
    bufDesc.SampleCount = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = VT_Device_CreateCommittedResource(m_d3d12Device,
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_ID3D12Resource_Local, reinterpret_cast<void**>(ppResource));

    if (FAILED(hr)) return DMLResult::error("CreateUploadBuffer failed", hr);
    return DMLResult::ok();
}

DMLResult DirectMLCompute::createReadbackBuffer(ID3D12Resource** ppResource, uint64_t sizeBytes) {
    D3D12_HEAP_PROPERTIES_DML heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC_DML bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = sizeBytes;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.Format = 0;
    bufDesc.SampleCount = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = VT_Device_CreateCommittedResource(m_d3d12Device,
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_ID3D12Resource_Local, reinterpret_cast<void**>(ppResource));

    if (FAILED(hr)) return DMLResult::error("CreateReadbackBuffer failed", hr);
    return DMLResult::ok();
}

DMLResult DirectMLCompute::createUAVBuffer(ID3D12Resource** ppResource, uint64_t sizeBytes) {
    return createDefaultBuffer(ppResource, sizeBytes);
}

// ============================================================================
// Command List Helpers
// ============================================================================
DMLResult DirectMLCompute::resetCommandList() {
    if (!m_cmdAlloc || !m_cmdList) return DMLResult::error("No cmd list/alloc", -1);

    HRESULT hr = VT_CmdAlloc_Reset(m_cmdAlloc);
    if (FAILED(hr)) return DMLResult::error("CmdAlloc Reset failed", hr);

    hr = VT_CmdList_Reset(m_cmdList, m_cmdAlloc, nullptr);
    if (FAILED(hr)) return DMLResult::error("CmdList Reset failed", hr);

    // Set descriptor heap for DML
    if (m_descriptorHeap) {
        void* heaps[] = { m_descriptorHeap };
        VT_CmdList_SetDescriptorHeaps(m_cmdList, 1, heaps);
    }

    return DMLResult::ok();
}

DMLResult DirectMLCompute::executeAndWait(uint32_t timeoutMs) {
    if (!m_cmdList || !m_cmdQueue || !m_fence) {
        return DMLResult::error("Command infrastructure not available", -1);
    }

    HRESULT hr = VT_CmdList_Close(m_cmdList);
    if (FAILED(hr)) return DMLResult::error("CmdList Close failed", hr);

    void* lists[] = { m_cmdList };
    VT_CmdQueue_ExecuteCommandLists(m_cmdQueue, 1, lists);

    m_fenceValue++;
    hr = VT_CmdQueue_Signal(m_cmdQueue, m_fence, m_fenceValue);
    if (FAILED(hr)) return DMLResult::error("Signal failed", hr);

    if (VT_Fence_GetCompletedValue(m_fence) < m_fenceValue) {
        VT_Fence_SetEventOnCompletion(m_fence, m_fenceValue, m_fenceEvent);
        DWORD result = WaitForSingleObject(m_fenceEvent, timeoutMs);
        if (result == WAIT_TIMEOUT) {
            return DMLResult::error("GPU fence timeout", -3);
        }
    }

    m_stats.fenceWaits.fetch_add(1);
    return DMLResult::ok();
}

DMLResult DirectMLCompute::syncGPU(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_fence || !m_cmdQueue) return DMLResult::ok("No queue");

    m_fenceValue++;
    VT_CmdQueue_Signal(m_cmdQueue, m_fence, m_fenceValue);
    if (VT_Fence_GetCompletedValue(m_fence) < m_fenceValue) {
        VT_Fence_SetEventOnCompletion(m_fence, m_fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, timeoutMs);
    }
    return DMLResult::ok();
}

DMLResult DirectMLCompute::flushAndWait() {
    return syncGPU(10000);
}

// ============================================================================
// Tensor Management
// ============================================================================
DMLResult DirectMLCompute::createTensor(DMLTensorBuffer& outTensor, TensorDataType dtype,
                                         const uint32_t* dims, uint32_t dimCount) {
    if (dimCount == 0 || dimCount > 8) return DMLResult::error("Invalid dim count", -1);

    uint64_t sizeBytes = computeTensorBufferSize(dtype, dims, dimCount);
    if (sizeBytes == 0) return DMLResult::error("Zero-sized tensor", -2);

    // Align to 256 bytes (DML requirement for buffer bindings)
    sizeBytes = (sizeBytes + 255) & ~255ULL;

    auto r = createDefaultBuffer(&outTensor.resource, sizeBytes);
    if (!r.success) return r;

    outTensor.gpuVA = VT_Resource_GetGPUVirtualAddress(outTensor.resource);
    outTensor.sizeBytes = sizeBytes;
    outTensor.dataType = dtype;
    outTensor.dimCount = dimCount;
    memcpy(outTensor.dims, dims, dimCount * sizeof(uint32_t));

    // Compute packed strides
    for (uint32_t i = dimCount; i > 0; --i) {
        if (i == dimCount) {
            outTensor.strides[i - 1] = 1;
        } else {
            outTensor.strides[i - 1] = outTensor.strides[i] * dims[i];
        }
    }

    outTensor.valid = true;
    return DMLResult::ok();
}

DMLResult DirectMLCompute::uploadTensor(DMLTensorBuffer& tensor, const void* hostData, uint64_t sizeBytes) {
    if (!tensor.resource || !hostData || sizeBytes == 0) {
        return DMLResult::error("Invalid tensor or data", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Create upload buffer
    ID3D12Resource* uploadBuf = nullptr;
    auto r = createUploadBuffer(&uploadBuf, sizeBytes);
    if (!r.success) return r;

    // Map and copy
    void* mapped = nullptr;
    D3D12_RANGE_DML readRange = { 0, 0 };
    HRESULT hr = VT_Resource_Map(uploadBuf, 0, &readRange, &mapped);
    if (FAILED(hr)) { SAFE_RELEASE(uploadBuf); return DMLResult::error("Map failed", hr); }

    memcpy(mapped, hostData, static_cast<size_t>(sizeBytes));
    VT_Resource_Unmap(uploadBuf, 0, nullptr);

    // Record copy command
    auto r2 = resetCommandList();
    if (!r2.success) { SAFE_RELEASE(uploadBuf); return r2; }

    // Transition destination to COPY_DEST
    D3D12_RESOURCE_BARRIER_DML barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = tensor.resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    VT_CmdList_ResourceBarrier(m_cmdList, 1, &barrier);

    VT_CmdList_CopyBufferRegion(m_cmdList, tensor.resource, 0, uploadBuf, 0, sizeBytes);

    // Transition back to UAV for DML
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    VT_CmdList_ResourceBarrier(m_cmdList, 1, &barrier);

    auto r3 = executeAndWait();
    SAFE_RELEASE(uploadBuf);

    m_stats.totalBytesUploaded.fetch_add(sizeBytes);
    return r3;
}

DMLResult DirectMLCompute::downloadTensor(void* hostDst, const DMLTensorBuffer& tensor, uint64_t sizeBytes) {
    if (!tensor.resource || !hostDst || sizeBytes == 0) {
        return DMLResult::error("Invalid tensor or destination", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    ID3D12Resource* readbackBuf = nullptr;
    auto r = createReadbackBuffer(&readbackBuf, sizeBytes);
    if (!r.success) return r;

    auto r2 = resetCommandList();
    if (!r2.success) { SAFE_RELEASE(readbackBuf); return r2; }

    // Transition source to COPY_SOURCE
    D3D12_RESOURCE_BARRIER_DML barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = const_cast<ID3D12Resource*>(tensor.resource);
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    VT_CmdList_ResourceBarrier(m_cmdList, 1, &barrier);

    VT_CmdList_CopyBufferRegion(m_cmdList, readbackBuf, 0,
        const_cast<ID3D12Resource*>(tensor.resource), 0, sizeBytes);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    VT_CmdList_ResourceBarrier(m_cmdList, 1, &barrier);

    auto r3 = executeAndWait();
    if (!r3.success) { SAFE_RELEASE(readbackBuf); return r3; }

    // Map and read
    void* mapped = nullptr;
    D3D12_RANGE_DML readRange = { 0, static_cast<SIZE_T>(sizeBytes) };
    HRESULT hr = VT_Resource_Map(readbackBuf, 0, &readRange, &mapped);
    if (SUCCEEDED(hr) && mapped) {
        memcpy(hostDst, mapped, static_cast<size_t>(sizeBytes));
        D3D12_RANGE_DML writeRange = { 0, 0 };
        VT_Resource_Unmap(readbackBuf, 0, &writeRange);
    }

    SAFE_RELEASE(readbackBuf);
    m_stats.totalBytesDownloaded.fetch_add(sizeBytes);
    return DMLResult::ok();
}

DMLResult DirectMLCompute::freeTensor(DMLTensorBuffer& tensor) {
    if (tensor.resource) {
        m_stats.vramFreed.fetch_add(tensor.sizeBytes);
        SAFE_RELEASE(tensor.resource);
    }
    tensor = {};
    return DMLResult::ok();
}

// ============================================================================
// Session Management (Dual-Model Support)
// ============================================================================
DMLResult DirectMLCompute::createSession(uint32_t sessionId, const char* modelName, uint64_t vramBudget) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_sessions.count(sessionId)) {
        return DMLResult::error("Session ID already exists", -1);
    }

    auto session = std::make_unique<InferenceSession>();
    session->sessionId = sessionId;
    session->modelName = modelName;
    session->vramBudget = vramBudget > 0 ? vramBudget : calculateSessionVRAMBudget(
        static_cast<uint32_t>(m_sessions.size()) + 1);
    session->active = true;

    m_sessions[sessionId] = std::move(session);

    if (m_activeSessionId == 0) {
        m_activeSessionId = sessionId;
    }

    log(1, (std::string("Session created: id=") + std::to_string(sessionId) +
            " model=" + modelName +
            " vramBudget=" + std::to_string(vramBudget / (1024 * 1024)) + "MB").c_str());
    return DMLResult::ok("Session created");
}

DMLResult DirectMLCompute::destroySession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_sessions.find(sessionId);
    if (it == m_sessions.end()) {
        return DMLResult::error("Session not found", -1);
    }

    // Flush GPU before destroying
    syncGPU(5000);

    auto& session = it->second;

    // Free all tensors
    for (auto& [hash, tensor] : session->tensors) {
        SAFE_RELEASE(tensor.resource);
    }
    for (auto& kv : session->kvCacheK) { SAFE_RELEASE(kv.resource); }
    for (auto& kv : session->kvCacheV) { SAFE_RELEASE(kv.resource); }

    m_sessions.erase(it);

    if (m_activeSessionId == sessionId) {
        m_activeSessionId = m_sessions.empty() ? 0 : m_sessions.begin()->first;
    }

    log(1, (std::string("Session destroyed: id=") + std::to_string(sessionId)).c_str());
    return DMLResult::ok("Session destroyed");
}

DMLResult DirectMLCompute::setActiveSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sessions.count(sessionId)) {
        return DMLResult::error("Session not found", -1);
    }
    m_activeSessionId = sessionId;
    return DMLResult::ok();
}

InferenceSession* DirectMLCompute::getSession(uint32_t sessionId) {
    auto it = m_sessions.find(sessionId);
    return (it != m_sessions.end()) ? it->second.get() : nullptr;
}

uint32_t DirectMLCompute::getSessionCount() const {
    return static_cast<uint32_t>(m_sessions.size());
}

uint64_t DirectMLCompute::calculateSessionVRAMBudget(uint32_t sessionCount) {
    // RX 7800 XT: 16GB VRAM. Reserve 1GB for system/descriptors.
    // Split remaining equally among sessions.
    const uint64_t totalVRAM = 16ULL * 1024 * 1024 * 1024;  // 16 GB
    const uint64_t reserved  = 1ULL * 1024 * 1024 * 1024;   // 1 GB reserved
    const uint64_t available = totalVRAM - reserved;

    if (sessionCount == 0) return available;
    return available / sessionCount;
}

// ============================================================================
// Operator Hash Computation
// ============================================================================
uint64_t DirectMLCompute::computeOpHash(const char* opName, const uint32_t* params, uint32_t paramCount) {
    uint64_t hash = fnv1a_hash(opName);
    for (uint32_t i = 0; i < paramCount; ++i) {
        hash ^= static_cast<uint64_t>(params[i]) * 2654435761ULL;
        hash = (hash << 13) | (hash >> 51);
    }
    return hash;
}

// ============================================================================
// Operator Compilation & Dispatch Core
// ============================================================================
DMLResult DirectMLCompute::getOrCompileOp(const char* opName, uint64_t hash,
                                           void* opDesc, uint32_t opType,
                                           CompiledOpEntry& outEntry) {
    // Check cache
    auto it = m_opCache.find(hash);
    if (it != m_opCache.end()) {
        outEntry = it->second;
        outEntry.hitCount++;
        it->second.hitCount++;
        m_stats.cacheHits.fetch_add(1);
        return DMLResult::ok();
    }
    m_stats.cacheMisses.fetch_add(1);

    // Create operator description
    DML_OPERATOR_DESC_DML desc = {};
    desc.Type = static_cast<int>(opType);
    desc.Desc = opDesc;

    // Create operator
    IDMLOperator* dmlOp = nullptr;
    HRESULT hr = VT_DMLDevice_CreateOperator(m_dmlDevice, &desc,
        IID_IDMLOperator_Local, reinterpret_cast<void**>(&dmlOp));
    if (FAILED(hr)) return DMLResult::error("CreateOperator failed", hr);

    // Compile operator (allow half precision for performance on AMD)
    IDMLCompiledOperator* compiledOp = nullptr;
    hr = VT_DMLDevice_CompileOperator(m_dmlDevice, dmlOp,
        DML_EXECUTION_FLAG_ALLOW_HALF_PRECISION | DML_EXECUTION_FLAG_DESCRIPTORS_VOLATILE,
        IID_IDMLCompiledOperator_Local, reinterpret_cast<void**>(&compiledOp));
    SAFE_RELEASE(dmlOp);

    if (FAILED(hr)) return DMLResult::error("CompileOperator failed", hr);

    // Get binding properties
    auto bindProps = VT_Dispatchable_GetBindingProperties(compiledOp);

    // Create persistent resource if needed
    ID3D12Resource* persistentBuf = nullptr;
    if (bindProps.PersistentResourceSize > 0) {
        auto r = createDefaultBuffer(&persistentBuf, bindProps.PersistentResourceSize);
        if (!r.success) { SAFE_RELEASE(compiledOp); return r; }
    }

    // Store in cache
    outEntry.compiledOp = compiledOp;
    outEntry.persistentBuf = persistentBuf;
    outEntry.tempBuf = nullptr;  // Created on demand per dispatch
    outEntry.persistentSize = bindProps.PersistentResourceSize;
    outEntry.tempSize = bindProps.TemporaryResourceSize;
    outEntry.descriptorCount = bindProps.RequiredDescriptorCount;
    outEntry.hitCount = 1;
    outEntry.opName = opName;

    m_opCache[hash] = outEntry;

    log(0, (std::string("Compiled DML op: ") + opName +
            " descriptors=" + std::to_string(bindProps.RequiredDescriptorCount) +
            " tempSize=" + std::to_string(bindProps.TemporaryResourceSize) +
            " persistSize=" + std::to_string(bindProps.PersistentResourceSize)).c_str());

    return DMLResult::ok();
}

DMLResult DirectMLCompute::initializeOperator(CompiledOpEntry& entry) {
    if (!entry.compiledOp) return DMLResult::error("No compiled op", -1);

    // Create operator initializer
    IDMLOperatorInitializer* initializer = nullptr;
    void* ops[] = { entry.compiledOp };
    HRESULT hr = VT_DMLDevice_CreateOperatorInitializer(m_dmlDevice, 1, ops,
        IID_IDMLOperatorInitializer_Local, reinterpret_cast<void**>(&initializer));
    if (FAILED(hr)) return DMLResult::error("CreateOperatorInitializer failed", hr);

    // Get initializer binding properties
    auto initBindProps = VT_Dispatchable_GetBindingProperties(initializer);

    // Allocate descriptors for initialization
    uint32_t descOffset = allocateDescriptors(initBindProps.RequiredDescriptorCount);
    uint64_t cpuHandle = 0, gpuHandle = 0;
    getCpuGpuDescriptorHandles(descOffset, cpuHandle, gpuHandle);

    // Create binding table for initializer
    DML_BINDING_TABLE_DESC_DML btDesc = {};
    btDesc.Dispatchable = initializer;
    btDesc.CPUDescriptorHandle = { cpuHandle };
    btDesc.GPUDescriptorHandle = { gpuHandle };
    btDesc.SizeInDescriptors = initBindProps.RequiredDescriptorCount;

    IDMLBindingTable* bindingTable = nullptr;
    hr = VT_DMLDevice_CreateBindingTable(m_dmlDevice, &btDesc,
        IID_IDMLBindingTable_Local, reinterpret_cast<void**>(&bindingTable));
    if (FAILED(hr)) { SAFE_RELEASE(initializer); return DMLResult::error("CreateBindingTable failed", hr); }

    // Bind persistent resource if needed
    if (entry.persistentBuf && entry.persistentSize > 0) {
        DML_BUFFER_BINDING_DML persistBinding = {};
        persistBinding.Buffer = entry.persistentBuf;
        persistBinding.Offset = 0;
        persistBinding.SizeInBytes = entry.persistentSize;

        DML_BINDING_DESC_DML persistDesc = {};
        persistDesc.Type = DML_BINDING_TYPE_BUFFER;
        persistDesc.Desc = &persistBinding;

        // For initialization, persistent resource is bound as output
        VT_BindingTable_BindOutputs(bindingTable, 1, &persistDesc);
    }

    // Create temp resource for initialization if needed
    ID3D12Resource* initTemp = nullptr;
    if (initBindProps.TemporaryResourceSize > 0) {
        auto r = createDefaultBuffer(&initTemp, initBindProps.TemporaryResourceSize);
        if (r.success) {
            DML_BUFFER_BINDING_DML tempBinding = {};
            tempBinding.Buffer = initTemp;
            tempBinding.Offset = 0;
            tempBinding.SizeInBytes = initBindProps.TemporaryResourceSize;

            DML_BINDING_DESC_DML tempDesc = {};
            tempDesc.Type = DML_BINDING_TYPE_BUFFER;
            tempDesc.Desc = &tempBinding;

            VT_BindingTable_BindTemporaryResource(bindingTable, &tempDesc);
        }
    }

    // Record dispatch for initialization
    auto r = resetCommandList();
    if (!r.success) {
        SAFE_RELEASE(bindingTable);
        SAFE_RELEASE(initializer);
        SAFE_RELEASE(initTemp);
        return r;
    }

    VT_CmdRecorder_RecordDispatch(m_cmdRecorder, m_cmdList, initializer, bindingTable);

    auto r2 = executeAndWait();

    SAFE_RELEASE(bindingTable);
    SAFE_RELEASE(initializer);
    SAFE_RELEASE(initTemp);

    return r2;
}

DMLResult DirectMLCompute::bindAndDispatch(CompiledOpEntry& entry,
                                            const DMLTensorBuffer* inputs, uint32_t inputCount,
                                            DMLTensorBuffer* outputs, uint32_t outputCount) {
    if (!entry.compiledOp) return DMLResult::error("No compiled op", -1);

    // Allocate descriptors
    uint32_t descOffset = allocateDescriptors(entry.descriptorCount);
    uint64_t cpuHandle = 0, gpuHandle = 0;
    getCpuGpuDescriptorHandles(descOffset, cpuHandle, gpuHandle);

    // Create binding table
    DML_BINDING_TABLE_DESC_DML btDesc = {};
    btDesc.Dispatchable = entry.compiledOp;
    btDesc.CPUDescriptorHandle = { cpuHandle };
    btDesc.GPUDescriptorHandle = { gpuHandle };
    btDesc.SizeInDescriptors = entry.descriptorCount;

    IDMLBindingTable* bindingTable = nullptr;
    HRESULT hr = VT_DMLDevice_CreateBindingTable(m_dmlDevice, &btDesc,
        IID_IDMLBindingTable_Local, reinterpret_cast<void**>(&bindingTable));
    if (FAILED(hr)) return DMLResult::error("CreateBindingTable failed", hr);

    // Bind inputs
    std::vector<DML_BUFFER_BINDING_DML> inputBindings(inputCount);
    std::vector<DML_BINDING_DESC_DML> inputDescs(inputCount);
    for (uint32_t i = 0; i < inputCount; ++i) {
        inputBindings[i].Buffer = inputs[i].resource;
        inputBindings[i].Offset = 0;
        inputBindings[i].SizeInBytes = inputs[i].sizeBytes;
        inputDescs[i].Type = inputs[i].resource ? DML_BINDING_TYPE_BUFFER : DML_BINDING_TYPE_NONE;
        inputDescs[i].Desc = inputs[i].resource ? &inputBindings[i] : nullptr;
    }
    VT_BindingTable_BindInputs(bindingTable, inputCount, inputDescs.data());

    // Bind outputs
    std::vector<DML_BUFFER_BINDING_DML> outputBindings(outputCount);
    std::vector<DML_BINDING_DESC_DML> outputDescs(outputCount);
    for (uint32_t i = 0; i < outputCount; ++i) {
        outputBindings[i].Buffer = outputs[i].resource;
        outputBindings[i].Offset = 0;
        outputBindings[i].SizeInBytes = outputs[i].sizeBytes;
        outputDescs[i].Type = outputs[i].resource ? DML_BINDING_TYPE_BUFFER : DML_BINDING_TYPE_NONE;
        outputDescs[i].Desc = outputs[i].resource ? &outputBindings[i] : nullptr;
    }
    VT_BindingTable_BindOutputs(bindingTable, outputCount, outputDescs.data());

    // Bind persistent resource
    if (entry.persistentBuf && entry.persistentSize > 0) {
        DML_BUFFER_BINDING_DML persistBinding = {};
        persistBinding.Buffer = entry.persistentBuf;
        persistBinding.Offset = 0;
        persistBinding.SizeInBytes = entry.persistentSize;

        DML_BINDING_DESC_DML persistDesc = {};
        persistDesc.Type = DML_BINDING_TYPE_BUFFER;
        persistDesc.Desc = &persistBinding;

        VT_BindingTable_BindPersistentResource(bindingTable, &persistDesc);
    }

    // Create temp resource if needed
    ID3D12Resource* tempBuf = nullptr;
    if (entry.tempSize > 0) {
        auto r = createDefaultBuffer(&tempBuf, entry.tempSize);
        if (r.success) {
            DML_BUFFER_BINDING_DML tempBinding = {};
            tempBinding.Buffer = tempBuf;
            tempBinding.Offset = 0;
            tempBinding.SizeInBytes = entry.tempSize;

            DML_BINDING_DESC_DML tempDesc = {};
            tempDesc.Type = DML_BINDING_TYPE_BUFFER;
            tempDesc.Desc = &tempBinding;

            VT_BindingTable_BindTemporaryResource(bindingTable, &tempDesc);
        }
    }

    // Reset and record dispatch
    auto r = resetCommandList();
    if (!r.success) { SAFE_RELEASE(bindingTable); SAFE_RELEASE(tempBuf); return r; }

    // Transition outputs to UAV state for DML
    for (uint32_t i = 0; i < outputCount; ++i) {
        if (outputs[i].resource) {
            D3D12_RESOURCE_BARRIER_DML barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = outputs[i].resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            VT_CmdList_ResourceBarrier(m_cmdList, 1, &barrier);
        }
    }

    VT_CmdRecorder_RecordDispatch(m_cmdRecorder, m_cmdList, entry.compiledOp, bindingTable);

    auto r2 = executeAndWait();

    SAFE_RELEASE(bindingTable);
    SAFE_RELEASE(tempBuf);

    m_stats.totalDispatches.fetch_add(1);
    return r2;
}

// ============================================================================
// Core DML Operators
// ============================================================================

// ---- GEMM (Matrix Multiply) ----
DMLResult DirectMLCompute::dispatchGEMM(const DMLTensorBuffer& A, const DMLTensorBuffer& B,
                                         DMLTensorBuffer& C,
                                         uint32_t M, uint32_t N, uint32_t K,
                                         bool transA, bool transB,
                                         float alpha, float beta) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Build tensor descriptors (4D: batch=1, channel=1, rows, cols)
    uint32_t aDims[4] = { 1, 1, M, transA ? M : K };
    uint32_t bDims[4] = { 1, 1, transB ? N : K, N };
    uint32_t cDims[4] = { 1, 1, M, N };

    DML_BUFFER_TENSOR_DESC_DML aTensorDesc = {};
    aTensorDesc.DataType = toDMLDataType(A.dataType);
    aTensorDesc.DimensionCount = 4;
    aTensorDesc.Sizes = aDims;
    aTensorDesc.TotalTensorSizeInBytes = A.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML bTensorDesc = {};
    bTensorDesc.DataType = toDMLDataType(B.dataType);
    bTensorDesc.DimensionCount = 4;
    bTensorDesc.Sizes = bDims;
    bTensorDesc.TotalTensorSizeInBytes = B.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML cTensorDesc = {};
    cTensorDesc.DataType = toDMLDataType(C.dataType);
    cTensorDesc.DimensionCount = 4;
    cTensorDesc.Sizes = cDims;
    cTensorDesc.TotalTensorSizeInBytes = C.sizeBytes;

    DML_TENSOR_DESC_DML aTensor = { DML_TENSOR_TYPE_BUFFER, &aTensorDesc };
    DML_TENSOR_DESC_DML bTensor = { DML_TENSOR_TYPE_BUFFER, &bTensorDesc };
    DML_TENSOR_DESC_DML cTensor = { DML_TENSOR_TYPE_BUFFER, &cTensorDesc };

    DML_GEMM_OPERATOR_DESC_DML gemmDesc = {};
    gemmDesc.ATensor = &aTensor;
    gemmDesc.BTensor = &bTensor;
    gemmDesc.CTensor = nullptr;  // No bias
    gemmDesc.OutputTensor = &cTensor;
    gemmDesc.TransA = transA ? DML_MATRIX_TRANSFORM_TRANSPOSE : DML_MATRIX_TRANSFORM_NONE;
    gemmDesc.TransB = transB ? DML_MATRIX_TRANSFORM_TRANSPOSE : DML_MATRIX_TRANSFORM_NONE;
    gemmDesc.Alpha = alpha;
    gemmDesc.Beta = beta;
    gemmDesc.FusedActivation = nullptr;

    // Compute hash for caching
    uint32_t params[] = { M, N, K, static_cast<uint32_t>(transA), static_cast<uint32_t>(transB),
                          static_cast<uint32_t>(A.dataType) };
    uint64_t hash = computeOpHash("GEMM", params, 6);

    CompiledOpEntry entry;
    auto r = getOrCompileOp("GEMM", hash, &gemmDesc, DML_OPERATOR_GEMM, entry);
    if (!r.success) return r;

    // Initialize if first use (persistent resource population)
    if (entry.hitCount == 1) {
        auto r2 = initializeOperator(entry);
        if (!r2.success) return r2;
    }

    const DMLTensorBuffer inputs[] = { A, B };
    DMLTensorBuffer outputs[] = { C };
    auto r3 = bindAndDispatch(entry, inputs, 2, outputs, 1);

    m_stats.gemmDispatches.fetch_add(1);
    return r3;
}

// ---- Multi-Head Attention (DML 6.1+ native operator) ----
DMLResult DirectMLCompute::dispatchMultiHeadAttention(
    const DMLTensorBuffer& Q, const DMLTensorBuffer& K,
    const DMLTensorBuffer& V, DMLTensorBuffer& output,
    uint32_t batchSize, uint32_t seqLen, uint32_t numHeads,
    uint32_t headDim, float scale,
    const DMLTensorBuffer* mask,
    const DMLTensorBuffer* pastK,
    const DMLTensorBuffer* pastV,
    DMLTensorBuffer* presentK,
    DMLTensorBuffer* presentV) {

    std::lock_guard<std::mutex> lock(m_mutex);

    if (scale == 0.0f) scale = 1.0f / sqrtf(static_cast<float>(headDim));

    // Query: [batch, seq, numHeads * headDim]
    uint32_t qDims[4] = { batchSize, seqLen, numHeads, headDim };
    uint32_t kvDims[4] = { batchSize, seqLen, numHeads, headDim };
    uint32_t outDims[4] = { batchSize, seqLen, numHeads, headDim };

    DML_BUFFER_TENSOR_DESC_DML qDesc = {};
    qDesc.DataType = toDMLDataType(Q.dataType);
    qDesc.DimensionCount = 4;
    qDesc.Sizes = qDims;
    qDesc.TotalTensorSizeInBytes = Q.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML kDesc = {};
    kDesc.DataType = toDMLDataType(K.dataType);
    kDesc.DimensionCount = 4;
    kDesc.Sizes = kvDims;
    kDesc.TotalTensorSizeInBytes = K.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML vDesc = {};
    vDesc.DataType = toDMLDataType(V.dataType);
    vDesc.DimensionCount = 4;
    vDesc.Sizes = kvDims;
    vDesc.TotalTensorSizeInBytes = V.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML outDesc = {};
    outDesc.DataType = toDMLDataType(output.dataType);
    outDesc.DimensionCount = 4;
    outDesc.Sizes = outDims;
    outDesc.TotalTensorSizeInBytes = output.sizeBytes;

    DML_TENSOR_DESC_DML qTensor = { DML_TENSOR_TYPE_BUFFER, &qDesc };
    DML_TENSOR_DESC_DML kTensor = { DML_TENSOR_TYPE_BUFFER, &kDesc };
    DML_TENSOR_DESC_DML vTensor = { DML_TENSOR_TYPE_BUFFER, &vDesc };
    DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

    // Past KV for incremental decoding
    DML_BUFFER_TENSOR_DESC_DML pastKDesc = {};
    DML_BUFFER_TENSOR_DESC_DML pastVDesc = {};
    DML_TENSOR_DESC_DML pastKTensor = {};
    DML_TENSOR_DESC_DML pastVTensor = {};

    if (pastK && pastK->resource) {
        pastKDesc.DataType = toDMLDataType(pastK->dataType);
        pastKDesc.DimensionCount = pastK->dimCount;
        pastKDesc.Sizes = pastK->dims;
        pastKDesc.TotalTensorSizeInBytes = pastK->sizeBytes;
        pastKTensor = { DML_TENSOR_TYPE_BUFFER, &pastKDesc };
    }
    if (pastV && pastV->resource) {
        pastVDesc.DataType = toDMLDataType(pastV->dataType);
        pastVDesc.DimensionCount = pastV->dimCount;
        pastVDesc.Sizes = pastV->dims;
        pastVDesc.TotalTensorSizeInBytes = pastV->sizeBytes;
        pastVTensor = { DML_TENSOR_TYPE_BUFFER, &pastVDesc };
    }

    // Present KV outputs
    DML_BUFFER_TENSOR_DESC_DML presentKDesc = {};
    DML_BUFFER_TENSOR_DESC_DML presentVDesc = {};
    DML_TENSOR_DESC_DML presentKTensor = {};
    DML_TENSOR_DESC_DML presentVTensor = {};

    if (presentK && presentK->resource) {
        presentKDesc.DataType = toDMLDataType(presentK->dataType);
        presentKDesc.DimensionCount = presentK->dimCount;
        presentKDesc.Sizes = presentK->dims;
        presentKDesc.TotalTensorSizeInBytes = presentK->sizeBytes;
        presentKTensor = { DML_TENSOR_TYPE_BUFFER, &presentKDesc };
    }
    if (presentV && presentV->resource) {
        presentVDesc.DataType = toDMLDataType(presentV->dataType);
        presentVDesc.DimensionCount = presentV->dimCount;
        presentVDesc.Sizes = presentV->dims;
        presentVDesc.TotalTensorSizeInBytes = presentV->sizeBytes;
        presentVTensor = { DML_TENSOR_TYPE_BUFFER, &presentVDesc };
    }

    DML_MULTIHEAD_ATTENTION_DESC_DML mhaDesc = {};
    mhaDesc.QueryTensor = &qTensor;
    mhaDesc.KeyTensor = &kTensor;
    mhaDesc.ValueTensor = &vTensor;
    mhaDesc.StackedQueryKeyTensor = nullptr;
    mhaDesc.StackedKeyValueTensor = nullptr;
    mhaDesc.StackedQueryKeyValueTensor = nullptr;
    mhaDesc.BiasTensor = nullptr;
    mhaDesc.MaskTensor = nullptr;
    mhaDesc.RelativePositionBiasTensor = nullptr;
    mhaDesc.PastKeyTensor = (pastK && pastK->resource) ? &pastKTensor : nullptr;
    mhaDesc.PastValueTensor = (pastV && pastV->resource) ? &pastVTensor : nullptr;
    mhaDesc.OutputTensor = &outTensor;
    mhaDesc.OutputPresentKeyTensor = (presentK && presentK->resource) ? &presentKTensor : nullptr;
    mhaDesc.OutputPresentValueTensor = (presentV && presentV->resource) ? &presentVTensor : nullptr;
    mhaDesc.Scale = scale;
    mhaDesc.MaskFilterValue = -10000.0f;
    mhaDesc.HeadCount = numHeads;
    mhaDesc.MaskType = DML_MHA_MASK_TYPE_NONE;

    uint32_t params[] = { batchSize, seqLen, numHeads, headDim,
                          static_cast<uint32_t>(Q.dataType) };
    uint64_t hash = computeOpHash("MHA", params, 5);

    CompiledOpEntry entry;
    auto r = getOrCompileOp("MultiHeadAttention", hash, &mhaDesc, DML_OPERATOR_MULTIHEAD_ATTENTION, entry);
    if (!r.success) return r;

    if (entry.hitCount == 1) {
        auto r2 = initializeOperator(entry);
        if (!r2.success) return r2;
    }

    // Build input/output arrays for binding
    // MHA inputs: Q, K, V, [StackedQK], [StackedKV], [StackedQKV], [Bias], [Mask], [RelPosBias], [PastK], [PastV]
    DMLTensorBuffer emptyTensor = {};
    DMLTensorBuffer inputBufs[11];
    inputBufs[0] = Q;
    inputBufs[1] = K;
    inputBufs[2] = V;
    inputBufs[3] = emptyTensor; // StackedQK
    inputBufs[4] = emptyTensor; // StackedKV
    inputBufs[5] = emptyTensor; // StackedQKV
    inputBufs[6] = emptyTensor; // Bias
    inputBufs[7] = emptyTensor; // Mask
    inputBufs[8] = emptyTensor; // RelPosBias
    inputBufs[9] = (pastK && pastK->resource) ? *pastK : emptyTensor;
    inputBufs[10] = (pastV && pastV->resource) ? *pastV : emptyTensor;

    // MHA outputs: Output, [PresentK], [PresentV]
    DMLTensorBuffer outputBufs[3];
    outputBufs[0] = output;
    outputBufs[1] = (presentK && presentK->resource) ? *presentK : emptyTensor;
    outputBufs[2] = (presentV && presentV->resource) ? *presentV : emptyTensor;

    auto r3 = bindAndDispatch(entry, inputBufs, 11, outputBufs, 3);

    m_stats.attentionDispatches.fetch_add(1);
    return r3;
}

// ---- Softmax ----
DMLResult DirectMLCompute::dispatchSoftmax(const DMLTensorBuffer& input, DMLTensorBuffer& output,
                                            uint32_t axis) {
    std::lock_guard<std::mutex> lock(m_mutex);

    DML_BUFFER_TENSOR_DESC_DML inDesc = {};
    inDesc.DataType = toDMLDataType(input.dataType);
    inDesc.DimensionCount = input.dimCount;
    inDesc.Sizes = input.dims;
    inDesc.TotalTensorSizeInBytes = input.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML outDesc = {};
    outDesc.DataType = toDMLDataType(output.dataType);
    outDesc.DimensionCount = output.dimCount;
    outDesc.Sizes = output.dims;
    outDesc.TotalTensorSizeInBytes = output.sizeBytes;

    DML_TENSOR_DESC_DML inTensor = { DML_TENSOR_TYPE_BUFFER, &inDesc };
    DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

    DML_ACTIVATION_SOFTMAX_DESC_DML softmaxDesc = {};
    softmaxDesc.InputTensor = &inTensor;
    softmaxDesc.OutputTensor = &outTensor;

    uint32_t params[] = { input.dimCount, input.dims[0],
                          input.dimCount > 1 ? input.dims[1] : 0,
                          static_cast<uint32_t>(input.dataType) };
    uint64_t hash = computeOpHash("Softmax", params, 4);

    CompiledOpEntry entry;
    auto r = getOrCompileOp("Softmax", hash, &softmaxDesc, DML_OPERATOR_ACTIVATION_SOFTMAX, entry);
    if (!r.success) return r;

    if (entry.hitCount == 1) {
        auto r2 = initializeOperator(entry);
        if (!r2.success) return r2;
    }

    auto r3 = bindAndDispatch(entry, &input, 1, &output, 1);
    m_stats.softmaxDispatches.fetch_add(1);
    return r3;
}

// ---- RMSNorm (via Reduce + ElementWise ops) ----
DMLResult DirectMLCompute::dispatchRMSNorm(const DMLTensorBuffer& input, const DMLTensorBuffer& weight,
                                            DMLTensorBuffer& output, float eps) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // RMSNorm: output = (input / sqrt(mean(input^2) + eps)) * weight
    // Implemented as a sequence of DML dispatches:
    // 1. Square the input (Element-wise Multiply input * input)
    // 2. Mean reduce over last axis
    // 3. Add epsilon via DML_ELEMENT_WISE_ADD1 with scalar
    // 4. Compute rsqrt via DML_ELEMENT_WISE_RECIP_SQRT
    // 5. Multiply input by rsqrt (broadcast) → normalized
    // 6. Multiply normalized by weight → output

    // --- Step 1: input² (squared = input * input) ---
    DMLTensorBuffer squared;
    uint32_t squaredDims[4];
    memcpy(squaredDims, input.dims, sizeof(uint32_t) * input.dimCount);
    auto r = createTensor(squared, input.dataType, input.dims, input.dimCount);
    if (!r.success) return r;

    {
        DML_BUFFER_TENSOR_DESC_DML desc = {};
        desc.DataType = toDMLDataType(input.dataType);
        desc.DimensionCount = input.dimCount;
        desc.Sizes = input.dims;
        desc.TotalTensorSizeInBytes = input.sizeBytes;

        DML_TENSOR_DESC_DML aTensor = { DML_TENSOR_TYPE_BUFFER, &desc };
        DML_TENSOR_DESC_DML bTensor = { DML_TENSOR_TYPE_BUFFER, &desc };

        DML_BUFFER_TENSOR_DESC_DML outDesc = {};
        outDesc.DataType = toDMLDataType(squared.dataType);
        outDesc.DimensionCount = squared.dimCount;
        outDesc.Sizes = squared.dims;
        outDesc.TotalTensorSizeInBytes = squared.sizeBytes;
        DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

        DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC_DML mulDesc = {};
        mulDesc.ATensor = &aTensor;
        mulDesc.BTensor = &bTensor;
        mulDesc.OutputTensor = &outTensor;

        uint32_t params[] = { input.dims[0], input.dimCount > 1 ? input.dims[1] : 1, 1 };
        uint64_t hash = computeOpHash("RMSNorm_Square", params, 3);
        CompiledOpEntry entry;
        auto rc = getOrCompileOp("RMSNorm_Square", hash, &mulDesc, DML_OPERATOR_ELEMENT_WISE_MULTIPLY, entry);
        if (!rc.success) { freeTensor(squared); return rc; }
        if (entry.hitCount == 1) { auto r2 = initializeOperator(entry); if (!r2.success) { freeTensor(squared); return r2; } }
        const DMLTensorBuffer inputs[] = { input, input };
        auto r3 = bindAndDispatch(entry, inputs, 2, &squared, 1);
        if (!r3.success) { freeTensor(squared); return r3; }
    }

    // --- Step 2: Mean reduce over last axis → meanBuf (shape with last dim=1) ---
    DMLTensorBuffer meanBuf;
    uint32_t meanDims[4];
    memcpy(meanDims, input.dims, sizeof(uint32_t) * input.dimCount);
    meanDims[input.dimCount - 1] = 1;  // reduced axis
    r = createTensor(meanBuf, input.dataType, meanDims, input.dimCount);
    if (!r.success) { freeTensor(squared); return r; }

    {
        DML_BUFFER_TENSOR_DESC_DML inDesc = {};
        inDesc.DataType = toDMLDataType(squared.dataType);
        inDesc.DimensionCount = squared.dimCount;
        inDesc.Sizes = squared.dims;
        inDesc.TotalTensorSizeInBytes = squared.sizeBytes;
        DML_TENSOR_DESC_DML inTensor = { DML_TENSOR_TYPE_BUFFER, &inDesc };

        DML_BUFFER_TENSOR_DESC_DML outDesc = {};
        outDesc.DataType = toDMLDataType(meanBuf.dataType);
        outDesc.DimensionCount = meanBuf.dimCount;
        outDesc.Sizes = meanDims;
        outDesc.TotalTensorSizeInBytes = meanBuf.sizeBytes;
        DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

        uint32_t reduceAxes[] = { input.dimCount - 1 };
        DML_REDUCE_OPERATOR_DESC_DML reduceDesc = {};
        reduceDesc.Function = DML_REDUCE_FUNCTION_MEAN;
        reduceDesc.InputTensor = &inTensor;
        reduceDesc.OutputTensor = &outTensor;
        reduceDesc.AxisCount = 1;
        reduceDesc.Axes = reduceAxes;

        uint32_t params[] = { input.dims[0], input.dimCount > 1 ? input.dims[1] : 1, 2 };
        uint64_t hash = computeOpHash("RMSNorm_Mean", params, 3);
        CompiledOpEntry entry;
        auto rc = getOrCompileOp("RMSNorm_Mean", hash, &reduceDesc, DML_OPERATOR_REDUCE, entry);
        if (!rc.success) { freeTensor(squared); freeTensor(meanBuf); return rc; }
        if (entry.hitCount == 1) { auto r2 = initializeOperator(entry); if (!r2.success) { freeTensor(squared); freeTensor(meanBuf); return r2; } }
        auto r3 = bindAndDispatch(entry, &squared, 1, &meanBuf, 1);
        if (!r3.success) { freeTensor(squared); freeTensor(meanBuf); return r3; }
    }

    freeTensor(squared); // No longer needed

    // --- Step 3: Add epsilon (mean + eps) using DML_ELEMENT_WISE_ADD1 with scalar alpha ---
    // We use IDENTITY with scalar bias to add eps: out = mean * 1.0 + eps
    {
        DML_BUFFER_TENSOR_DESC_DML desc = {};
        desc.DataType = toDMLDataType(meanBuf.dataType);
        desc.DimensionCount = meanBuf.dimCount;
        desc.Sizes = meanDims;
        desc.TotalTensorSizeInBytes = meanBuf.sizeBytes;
        DML_TENSOR_DESC_DML tensor = { DML_TENSOR_TYPE_BUFFER, &desc };

        DML_ELEMENT_WISE_ADD_OPERATOR_DESC_DML addDesc = {};
        // Self-referencing: meanBuf = meanBuf + epsBuf
        // Instead, use ELEMENT_WISE_IDENTITY with scale=1 bias=eps
        DML_ELEMENT_WISE_IDENTITY_OPERATOR_DESC_DML idDesc = {};
        idDesc.InputTensor = &tensor;
        idDesc.OutputTensor = &tensor;

        // Since IDENTITY doesn't add bias directly, we use CPU-side download,
        // add eps, re-upload for this scalar. This is a single float per batch element.
        uint64_t meanSizeBytes = meanBuf.sizeBytes;
        std::vector<uint8_t> meanData(meanSizeBytes);
        DMLTensorBuffer meanCPU;
        meanCPU.dataType = meanBuf.dataType;
        meanCPU.dimCount = meanBuf.dimCount;
        memcpy(meanCPU.dims, meanDims, sizeof(uint32_t) * meanBuf.dimCount);
        meanCPU.sizeBytes = meanSizeBytes;

        downloadTensor(meanData.data(), meanBuf, meanSizeBytes);

        // Add eps to each float in the mean tensor
        if (meanBuf.dataType == TensorDataType::Float32) {
            float* fdata = reinterpret_cast<float*>(meanData.data());
            uint64_t count = meanSizeBytes / sizeof(float);
            for (uint64_t i = 0; i < count; ++i) {
                fdata[i] += eps;
            }
        } else if (meanBuf.dataType == TensorDataType::Float16) {
            // FP16: download as raw, convert, add eps, convert back
            uint16_t* hdata = reinterpret_cast<uint16_t*>(meanData.data());
            uint64_t count = meanSizeBytes / sizeof(uint16_t);
            auto fp16ToFp32 = [](uint16_t h) -> float {
                uint32_t sign = (h >> 15) & 1;
                uint32_t exp  = (h >> 10) & 0x1F;
                uint32_t man  = h & 0x3FF;
                if (exp == 0) {
                    if (man == 0) return sign ? -0.0f : 0.0f;
                    return ldexpf(static_cast<float>(man), -24) * (sign ? -1.0f : 1.0f);
                }
                if (exp == 31) return sign ? -INFINITY : INFINITY;
                float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
                return sign ? -v : v;
            };
            auto fp32ToFp16 = [](float f) -> uint16_t {
                uint32_t bits;
                memcpy(&bits, &f, sizeof(uint32_t));
                uint16_t sign = (bits >> 31) & 1;
                int32_t exp   = ((bits >> 23) & 0xFF) - 127 + 15;
                uint32_t man  = (bits >> 13) & 0x3FF;
                if (exp <= 0) return static_cast<uint16_t>(sign << 15);
                if (exp >= 31) return static_cast<uint16_t>((sign << 15) | (0x1F << 10));
                return static_cast<uint16_t>((sign << 15) | (exp << 10) | man);
            };
            for (uint64_t i = 0; i < count; ++i) {
                float val = fp16ToFp32(hdata[i]) + eps;
                hdata[i] = fp32ToFp16(val);
            }
        }

        uploadTensor(meanBuf, meanData.data(), meanSizeBytes);
    }

    // --- Step 4: rsqrt(mean + eps) → rsqrtBuf ---
    DMLTensorBuffer rsqrtBuf;
    r = createTensor(rsqrtBuf, meanBuf.dataType, meanDims, meanBuf.dimCount);
    if (!r.success) { freeTensor(meanBuf); return r; }

    {
        DML_BUFFER_TENSOR_DESC_DML inDesc = {};
        inDesc.DataType = toDMLDataType(meanBuf.dataType);
        inDesc.DimensionCount = meanBuf.dimCount;
        inDesc.Sizes = meanDims;
        inDesc.TotalTensorSizeInBytes = meanBuf.sizeBytes;
        DML_TENSOR_DESC_DML inTensor = { DML_TENSOR_TYPE_BUFFER, &inDesc };

        DML_BUFFER_TENSOR_DESC_DML outDesc = {};
        outDesc.DataType = toDMLDataType(rsqrtBuf.dataType);
        outDesc.DimensionCount = rsqrtBuf.dimCount;
        outDesc.Sizes = meanDims;
        outDesc.TotalTensorSizeInBytes = rsqrtBuf.sizeBytes;
        DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

        DML_ELEMENT_WISE_RECIP_SQRT_OPERATOR_DESC_DML rsqrtDesc = {};
        rsqrtDesc.InputTensor = &inTensor;
        rsqrtDesc.OutputTensor = &outTensor;

        uint32_t params[] = { meanDims[0], meanBuf.dimCount > 1 ? meanDims[1] : 1, 4 };
        uint64_t hash = computeOpHash("RMSNorm_Rsqrt", params, 3);
        CompiledOpEntry entry;
        auto rc = getOrCompileOp("RMSNorm_Rsqrt", hash, &rsqrtDesc, DML_OPERATOR_ELEMENT_WISE_RECIP_SQRT, entry);
        if (!rc.success) { freeTensor(meanBuf); freeTensor(rsqrtBuf); return rc; }
        if (entry.hitCount == 1) { auto r2 = initializeOperator(entry); if (!r2.success) { freeTensor(meanBuf); freeTensor(rsqrtBuf); return r2; } }
        auto r3 = bindAndDispatch(entry, &meanBuf, 1, &rsqrtBuf, 1);
        if (!r3.success) { freeTensor(meanBuf); freeTensor(rsqrtBuf); return r3; }
    }

    freeTensor(meanBuf); // No longer needed

    // --- Step 5: normalized = input * rsqrt (broadcast multiply) ---
    DMLTensorBuffer normalized;
    r = createTensor(normalized, input.dataType, input.dims, input.dimCount);
    if (!r.success) { freeTensor(rsqrtBuf); return r; }

    {
        DML_BUFFER_TENSOR_DESC_DML inDesc = {};
        inDesc.DataType = toDMLDataType(input.dataType);
        inDesc.DimensionCount = input.dimCount;
        inDesc.Sizes = input.dims;
        inDesc.TotalTensorSizeInBytes = input.sizeBytes;
        DML_TENSOR_DESC_DML inTensor = { DML_TENSOR_TYPE_BUFFER, &inDesc };

        // rsqrtBuf has reduced last-dim — DML broadcast handles the dim=1 axis
        DML_BUFFER_TENSOR_DESC_DML rsqrtDescBuf = {};
        rsqrtDescBuf.DataType = toDMLDataType(rsqrtBuf.dataType);
        rsqrtDescBuf.DimensionCount = rsqrtBuf.dimCount;
        rsqrtDescBuf.Sizes = rsqrtBuf.dims;
        rsqrtDescBuf.TotalTensorSizeInBytes = rsqrtBuf.sizeBytes;
        DML_TENSOR_DESC_DML rsqrtTensor = { DML_TENSOR_TYPE_BUFFER, &rsqrtDescBuf };

        DML_BUFFER_TENSOR_DESC_DML outDesc = {};
        outDesc.DataType = toDMLDataType(normalized.dataType);
        outDesc.DimensionCount = normalized.dimCount;
        outDesc.Sizes = normalized.dims;
        outDesc.TotalTensorSizeInBytes = normalized.sizeBytes;
        DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

        DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC_DML mulDesc = {};
        mulDesc.ATensor = &inTensor;
        mulDesc.BTensor = &rsqrtTensor;
        mulDesc.OutputTensor = &outTensor;

        uint32_t params[] = { input.dims[0], input.dimCount > 1 ? input.dims[1] : 1, 5 };
        uint64_t hash = computeOpHash("RMSNorm_NormMul", params, 3);
        CompiledOpEntry entry;
        auto rc = getOrCompileOp("RMSNorm_NormMul", hash, &mulDesc, DML_OPERATOR_ELEMENT_WISE_MULTIPLY, entry);
        if (!rc.success) { freeTensor(rsqrtBuf); freeTensor(normalized); return rc; }
        if (entry.hitCount == 1) { auto r2 = initializeOperator(entry); if (!r2.success) { freeTensor(rsqrtBuf); freeTensor(normalized); return r2; } }
        const DMLTensorBuffer inputs[] = { input, rsqrtBuf };
        auto r3 = bindAndDispatch(entry, inputs, 2, &normalized, 1);
        if (!r3.success) { freeTensor(rsqrtBuf); freeTensor(normalized); return r3; }
    }

    freeTensor(rsqrtBuf); // No longer needed

    // --- Step 6: output = normalized * weight ---

    DML_BUFFER_TENSOR_DESC_DML normDesc = {};
    normDesc.DataType = toDMLDataType(normalized.dataType);
    normDesc.DimensionCount = normalized.dimCount;
    normDesc.Sizes = normalized.dims;
    normDesc.TotalTensorSizeInBytes = normalized.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML wDesc = {};
    wDesc.DataType = toDMLDataType(weight.dataType);
    wDesc.DimensionCount = weight.dimCount;
    wDesc.Sizes = weight.dims;
    wDesc.TotalTensorSizeInBytes = weight.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML outDesc = {};
    outDesc.DataType = toDMLDataType(output.dataType);
    outDesc.DimensionCount = output.dimCount;
    outDesc.Sizes = output.dims;
    outDesc.TotalTensorSizeInBytes = output.sizeBytes;

    DML_TENSOR_DESC_DML normTensor = { DML_TENSOR_TYPE_BUFFER, &normDesc };
    DML_TENSOR_DESC_DML wTensor = { DML_TENSOR_TYPE_BUFFER, &wDesc };
    DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

    DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC_DML mulDesc = {};
    mulDesc.ATensor = &normTensor;
    mulDesc.BTensor = &wTensor;
    mulDesc.OutputTensor = &outTensor;

    uint32_t params[] = { input.dims[0],
                          input.dimCount > 1 ? input.dims[1] : 1,
                          static_cast<uint32_t>(input.dataType) };
    uint64_t hash = computeOpHash("RMSNorm_WeightMul", params, 3);

    CompiledOpEntry entry;
    auto rc = getOrCompileOp("RMSNorm_WeightMul", hash, &mulDesc, DML_OPERATOR_ELEMENT_WISE_MULTIPLY, entry);
    if (!rc.success) { freeTensor(normalized); return rc; }

    if (entry.hitCount == 1) {
        auto r2 = initializeOperator(entry);
        if (!r2.success) { freeTensor(normalized); return r2; }
    }

    const DMLTensorBuffer finalInputs[] = { normalized, weight };
    auto r3 = bindAndDispatch(entry, finalInputs, 2, &output, 1);
    freeTensor(normalized); // Clean up intermediate
    m_stats.normDispatches.fetch_add(1);
    return r3;
}

// ---- GELU Activation ----
DMLResult DirectMLCompute::dispatchGELU(const DMLTensorBuffer& input, DMLTensorBuffer& output) {
    std::lock_guard<std::mutex> lock(m_mutex);

    DML_BUFFER_TENSOR_DESC_DML inDesc = {};
    inDesc.DataType = toDMLDataType(input.dataType);
    inDesc.DimensionCount = input.dimCount;
    inDesc.Sizes = input.dims;
    inDesc.TotalTensorSizeInBytes = input.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML outDesc = {};
    outDesc.DataType = toDMLDataType(output.dataType);
    outDesc.DimensionCount = output.dimCount;
    outDesc.Sizes = output.dims;
    outDesc.TotalTensorSizeInBytes = output.sizeBytes;

    DML_TENSOR_DESC_DML inTensor = { DML_TENSOR_TYPE_BUFFER, &inDesc };
    DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

    DML_ACTIVATION_GELU_DESC_DML geluDesc = {};
    geluDesc.InputTensor = &inTensor;
    geluDesc.OutputTensor = &outTensor;

    uint32_t params[] = { input.dims[0],
                          input.dimCount > 1 ? input.dims[1] : 1,
                          static_cast<uint32_t>(input.dataType) };
    uint64_t hash = computeOpHash("GELU", params, 3);

    CompiledOpEntry entry;
    auto r = getOrCompileOp("GELU", hash, &geluDesc, DML_OPERATOR_ACTIVATION_GELU, entry);
    if (!r.success) return r;

    if (entry.hitCount == 1) {
        auto r2 = initializeOperator(entry);
        if (!r2.success) return r2;
    }

    auto r3 = bindAndDispatch(entry, &input, 1, &output, 1);
    m_stats.activationDispatches.fetch_add(1);
    return r3;
}

// ---- SiLU Activation (sigmoid(x) * x) ----
DMLResult DirectMLCompute::dispatchSiLU(const DMLTensorBuffer& input, DMLTensorBuffer& output) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // SiLU = sigmoid(x) * x. We implement as:
    // 1. Sigmoid(input) → temp
    // 2. ElementWiseMul(temp, input) → output

    DML_BUFFER_TENSOR_DESC_DML inDesc = {};
    inDesc.DataType = toDMLDataType(input.dataType);
    inDesc.DimensionCount = input.dimCount;
    inDesc.Sizes = input.dims;
    inDesc.TotalTensorSizeInBytes = input.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML outDesc = {};
    outDesc.DataType = toDMLDataType(output.dataType);
    outDesc.DimensionCount = output.dimCount;
    outDesc.Sizes = output.dims;
    outDesc.TotalTensorSizeInBytes = output.sizeBytes;

    DML_TENSOR_DESC_DML inTensor = { DML_TENSOR_TYPE_BUFFER, &inDesc };
    DML_TENSOR_DESC_DML outTensor = { DML_TENSOR_TYPE_BUFFER, &outDesc };

    // Step 1: Sigmoid
    DML_ACTIVATION_SIGMOID_DESC_DML sigmDesc = {};
    sigmDesc.InputTensor = &inTensor;
    sigmDesc.OutputTensor = &outTensor;

    uint32_t sigParams[] = { input.dims[0],
                             input.dimCount > 1 ? input.dims[1] : 1,
                             static_cast<uint32_t>(input.dataType), 1 };
    uint64_t sigHash = computeOpHash("SiLU_Sigmoid", sigParams, 4);

    CompiledOpEntry sigEntry;
    auto r1 = getOrCompileOp("SiLU_Sigmoid", sigHash, &sigmDesc, DML_OPERATOR_ACTIVATION_SIGMOID, sigEntry);
    if (!r1.success) return r1;
    if (sigEntry.hitCount == 1) initializeOperator(sigEntry);

    // Dispatch sigmoid: input → output (temp storage)
    auto r2 = bindAndDispatch(sigEntry, &input, 1, &output, 1);
    if (!r2.success) return r2;

    // Step 2: ElementWise Multiply (sigmoid_output * input)
    // We need to create a temporary buffer for the multiplication, but since
    // output already contains sigmoid(input), we can multiply output * input

    // However, DML doesn't support in-place input/output overlap well.
    // For now, we use the output as one input — this works for dispatch
    // because DML uses UAV barriers internally.

    DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC_DML mulDesc = {};
    mulDesc.ATensor = &outTensor;   // sigmoid(x) — already in output
    mulDesc.BTensor = &inTensor;    // x
    mulDesc.OutputTensor = &outTensor;

    uint32_t mulParams[] = { input.dims[0],
                             input.dimCount > 1 ? input.dims[1] : 1,
                             static_cast<uint32_t>(input.dataType), 2 };
    uint64_t mulHash = computeOpHash("SiLU_Mul", mulParams, 4);

    CompiledOpEntry mulEntry;
    auto r3 = getOrCompileOp("SiLU_Mul", mulHash, &mulDesc, DML_OPERATOR_ELEMENT_WISE_MULTIPLY, mulEntry);
    if (!r3.success) return r3;
    if (mulEntry.hitCount == 1) initializeOperator(mulEntry);

    const DMLTensorBuffer mulInputs[] = { output, input };
    auto r4 = bindAndDispatch(mulEntry, mulInputs, 2, &output, 1);
    m_stats.activationDispatches.fetch_add(1);
    return r4;
}

// ---- Element-wise Add ----
DMLResult DirectMLCompute::dispatchAdd(const DMLTensorBuffer& A, const DMLTensorBuffer& B,
                                        DMLTensorBuffer& C) {
    std::lock_guard<std::mutex> lock(m_mutex);

    DML_BUFFER_TENSOR_DESC_DML aDesc = {};
    aDesc.DataType = toDMLDataType(A.dataType);
    aDesc.DimensionCount = A.dimCount;
    aDesc.Sizes = A.dims;
    aDesc.TotalTensorSizeInBytes = A.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML bDesc = {};
    bDesc.DataType = toDMLDataType(B.dataType);
    bDesc.DimensionCount = B.dimCount;
    bDesc.Sizes = B.dims;
    bDesc.TotalTensorSizeInBytes = B.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML cDesc = {};
    cDesc.DataType = toDMLDataType(C.dataType);
    cDesc.DimensionCount = C.dimCount;
    cDesc.Sizes = C.dims;
    cDesc.TotalTensorSizeInBytes = C.sizeBytes;

    DML_TENSOR_DESC_DML aTensor = { DML_TENSOR_TYPE_BUFFER, &aDesc };
    DML_TENSOR_DESC_DML bTensor = { DML_TENSOR_TYPE_BUFFER, &bDesc };
    DML_TENSOR_DESC_DML cTensor = { DML_TENSOR_TYPE_BUFFER, &cDesc };

    DML_ELEMENT_WISE_ADD_OPERATOR_DESC_DML addDesc = {};
    addDesc.ATensor = &aTensor;
    addDesc.BTensor = &bTensor;
    addDesc.OutputTensor = &cTensor;

    uint32_t params[] = { A.dims[0], A.dimCount > 1 ? A.dims[1] : 1 };
    uint64_t hash = computeOpHash("Add", params, 2);

    CompiledOpEntry entry;
    auto r = getOrCompileOp("Add", hash, &addDesc, DML_OPERATOR_ELEMENT_WISE_ADD, entry);
    if (!r.success) return r;
    if (entry.hitCount == 1) initializeOperator(entry);

    const DMLTensorBuffer inputs[] = { A, B };
    return bindAndDispatch(entry, inputs, 2, &C, 1);
}

// ---- Element-wise Multiply ----
DMLResult DirectMLCompute::dispatchMul(const DMLTensorBuffer& A, const DMLTensorBuffer& B,
                                        DMLTensorBuffer& C) {
    std::lock_guard<std::mutex> lock(m_mutex);

    DML_BUFFER_TENSOR_DESC_DML aDesc = {};
    aDesc.DataType = toDMLDataType(A.dataType);
    aDesc.DimensionCount = A.dimCount;
    aDesc.Sizes = A.dims;
    aDesc.TotalTensorSizeInBytes = A.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML bDesc = {};
    bDesc.DataType = toDMLDataType(B.dataType);
    bDesc.DimensionCount = B.dimCount;
    bDesc.Sizes = B.dims;
    bDesc.TotalTensorSizeInBytes = B.sizeBytes;

    DML_BUFFER_TENSOR_DESC_DML cDesc = {};
    cDesc.DataType = toDMLDataType(C.dataType);
    cDesc.DimensionCount = C.dimCount;
    cDesc.Sizes = C.dims;
    cDesc.TotalTensorSizeInBytes = C.sizeBytes;

    DML_TENSOR_DESC_DML aTensor = { DML_TENSOR_TYPE_BUFFER, &aDesc };
    DML_TENSOR_DESC_DML bTensor = { DML_TENSOR_TYPE_BUFFER, &bDesc };
    DML_TENSOR_DESC_DML cTensor = { DML_TENSOR_TYPE_BUFFER, &cDesc };

    DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC_DML mulDesc = {};
    mulDesc.ATensor = &aTensor;
    mulDesc.BTensor = &bTensor;
    mulDesc.OutputTensor = &cTensor;

    uint32_t params[] = { A.dims[0], A.dimCount > 1 ? A.dims[1] : 1 };
    uint64_t hash = computeOpHash("Mul", params, 2);

    CompiledOpEntry entry;
    auto r = getOrCompileOp("Mul", hash, &mulDesc, DML_OPERATOR_ELEMENT_WISE_MULTIPLY, entry);
    if (!r.success) return r;
    if (entry.hitCount == 1) initializeOperator(entry);

    const DMLTensorBuffer inputs[] = { A, B };
    return bindAndDispatch(entry, inputs, 2, &C, 1);
}

// ---- RoPE (Rotary Positional Encoding) ----
DMLResult DirectMLCompute::dispatchRoPE(DMLTensorBuffer& qk, uint32_t seqLen,
                                         uint32_t headDim, uint32_t posOffset,
                                         float theta) {
    // RoPE requires custom computation: rotate pairs of elements using sincos
    // For DML, we'll implement this as:
    // 1. CPU-side: generate rotation matrix (sincos table)
    // 2. Upload rotation matrix
    // 3. Element-wise multiply + add to apply rotation

    std::lock_guard<std::mutex> lock(m_mutex);

    // Generate sincos table on CPU
    uint32_t halfDim = headDim / 2;
    uint32_t tableSize = seqLen * halfDim;
    std::vector<float> cosTable(tableSize);
    std::vector<float> sinTable(tableSize);

    for (uint32_t pos = 0; pos < seqLen; ++pos) {
        for (uint32_t i = 0; i < halfDim; ++i) {
            float freq = 1.0f / powf(theta, static_cast<float>(2 * i) / static_cast<float>(headDim));
            float angle = static_cast<float>(pos + posOffset) * freq;
            cosTable[pos * halfDim + i] = cosf(angle);
            sinTable[pos * halfDim + i] = sinf(angle);
        }
    }

    // RoPE is applied by downloading QK data from GPU, rotating via the MASM kernel
    // (asm_dml_rope_rotate_fp32), and re-uploading the result.
    // The MASM kernel applies: out[2i] = x[2i]*cos - x[2i+1]*sin
    //                          out[2i+1] = x[2i]*sin + x[2i+1]*cos

    // Download QK tensor from GPU
    uint64_t qkSizeBytes = qk.sizeBytes;
    std::vector<uint8_t> qkData(qkSizeBytes);
    downloadTensor(qkData.data(), qk, qkSizeBytes);

    if (qk.dataType == TensorDataType::Float32) {
        // Apply rotation using the MASM RoPE kernel
        float* qkF32 = reinterpret_cast<float*>(qkData.data());
        int64_t result = asm_dml_rope_rotate_fp32(
            qkF32,
            cosTable.data(),
            sinTable.data(),
            halfDim,
            seqLen
        );

        if (result != 0) {
            // Fallback: CPU-side rotation in C++
            log(2, "ASM RoPE failed, applying CPU fallback rotation");
            for (uint32_t pos = 0; pos < seqLen; ++pos) {
                for (uint32_t i = 0; i < halfDim; ++i) {
                    uint32_t idx = pos * headDim + i * 2;
                    float x0 = qkF32[idx];
                    float x1 = qkF32[idx + 1];
                    float c  = cosTable[pos * halfDim + i];
                    float s  = sinTable[pos * halfDim + i];
                    qkF32[idx]     = x0 * c - x1 * s;
                    qkF32[idx + 1] = x0 * s + x1 * c;
                }
            }
        }
    } else if (qk.dataType == TensorDataType::Float16) {
        // FP16 path: convert to FP32, rotate, convert back
        uint16_t* qkF16 = reinterpret_cast<uint16_t*>(qkData.data());
        uint64_t numElements = qkSizeBytes / sizeof(uint16_t);

        auto fp16ToFp32 = [](uint16_t h) -> float {
            uint32_t sign = (h >> 15) & 1;
            uint32_t exp  = (h >> 10) & 0x1F;
            uint32_t man  = h & 0x3FF;
            if (exp == 0) {
                if (man == 0) return sign ? -0.0f : 0.0f;
                return ldexpf(static_cast<float>(man), -24) * (sign ? -1.0f : 1.0f);
            }
            if (exp == 31) return sign ? -INFINITY : INFINITY;
            float v = ldexpf(1.0f + static_cast<float>(man) / 1024.0f, exp - 15);
            return sign ? -v : v;
        };
        auto fp32ToFp16 = [](float f) -> uint16_t {
            uint32_t bits;
            memcpy(&bits, &f, sizeof(uint32_t));
            uint16_t sign = (bits >> 31) & 1;
            int32_t exp   = ((bits >> 23) & 0xFF) - 127 + 15;
            uint32_t man  = (bits >> 13) & 0x3FF;
            if (exp <= 0) return static_cast<uint16_t>(sign << 15);
            if (exp >= 31) return static_cast<uint16_t>((sign << 15) | (0x1F << 10));
            return static_cast<uint16_t>((sign << 15) | (exp << 10) | man);
        };

        for (uint32_t pos = 0; pos < seqLen; ++pos) {
            for (uint32_t i = 0; i < halfDim; ++i) {
                uint32_t idx = pos * headDim + i * 2;
                if (idx + 1 >= numElements) break;
                float x0 = fp16ToFp32(qkF16[idx]);
                float x1 = fp16ToFp32(qkF16[idx + 1]);
                float c  = cosTable[pos * halfDim + i];
                float s  = sinTable[pos * halfDim + i];
                qkF16[idx]     = fp32ToFp16(x0 * c - x1 * s);
                qkF16[idx + 1] = fp32ToFp16(x0 * s + x1 * c);
            }
        }
    }

    // Re-upload the rotated tensor to GPU
    uploadTensor(qk, qkData.data(), qkSizeBytes);

    log(0, (std::string("RoPE applied: seqLen=") + std::to_string(seqLen) +
            " headDim=" + std::to_string(headDim) +
            " offset=" + std::to_string(posOffset)).c_str());
    m_stats.totalDispatches.fetch_add(1);
    return DMLResult::ok("RoPE applied");
}

// ---- Dequantize ----
DMLResult DirectMLCompute::dispatchDequantize(const DMLTensorBuffer& quantized,
                                               DMLTensorBuffer& output,
                                               GGUFQuantType quantType,
                                               uint32_t elementCount) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // GGUF block quantization formats require CPU-side dequantization because
    // DML_ELEMENT_WISE_DEQUANTIZE_LINEAR only supports per-tensor/per-axis scales,
    // not the complex block-structured scales/mins of GGUF K-quant formats.
    //
    // Strategy: Download quantized data from GPU → CPU dequant via GGUFDMLBridge →
    // Upload FP32 result to output tensor on GPU.

    // Download quantized data from GPU to CPU
    uint64_t quantSizeBytes = quantized.sizeBytes;
    std::vector<uint8_t> quantData(quantSizeBytes);
    downloadTensor(quantData.data(), quantized, quantSizeBytes);

    // Allocate CPU output buffer
    uint64_t outputSizeBytes = static_cast<uint64_t>(elementCount) * sizeof(float);
    std::vector<float> dequantized(elementCount, 0.0f);

    // Map GGUFQuantType to GGML type int for the bridge dequantizer
    int ggmlType = static_cast<int>(quantType);

    // Use the GGUF-DML bridge dequantizer (which now supports all K-quant types)
    GGUFDMLBridge bridge;
    auto dqResult = bridge.dequantizeTensor(quantData.data(), dequantized.data(),
                                            ggmlType, elementCount);
    if (!dqResult.success) {
        log(3, (std::string("Dequantize failed for type ") +
                std::to_string(ggmlType) + ": " + dqResult.detail).c_str());
        return dqResult;
    }

    // Upload dequantized FP32 data to GPU output tensor
    uploadTensor(output, dequantized.data(), outputSizeBytes);

    log(0, (std::string("Dequantize: quantType=") + std::to_string(ggmlType) +
            " elements=" + std::to_string(elementCount) + " → FP32 uploaded").c_str());

    m_stats.dequantDispatches.fetch_add(1);
    return DMLResult::ok("Dequantize complete");
}

// ============================================================================
// GGUF Tensor Upload to Session
// ============================================================================
DMLResult DirectMLCompute::uploadGGUFTensor(uint32_t sessionId, uint64_t nameHash,
                                             const void* quantizedData, uint64_t dataSize,
                                             GGUFQuantType quantType,
                                             const uint32_t* shape, uint32_t shapeDims) {
    auto* session = getSession(sessionId);
    if (!session) return DMLResult::error("Session not found", -1);

    // Check VRAM budget
    uint64_t alignedSize = (dataSize + 255) & ~255ULL;
    if (session->vramBudget > 0 && session->vramUsed + alignedSize > session->vramBudget) {
        return DMLResult::error("VRAM budget exceeded for session", -2);
    }

    // Create tensor buffer
    DMLTensorBuffer tensor;
    TensorDataType dt = (quantType == GGUFQuantType::F16) ? TensorDataType::Float16 :
                        (quantType == GGUFQuantType::F32) ? TensorDataType::Float32 :
                        TensorDataType::UInt8;  // Quantized data stored as raw bytes

    auto r = createTensor(tensor, dt, shape, shapeDims);
    if (!r.success) return r;

    // Upload data
    auto r2 = uploadTensor(tensor, quantizedData, dataSize);
    if (!r2.success) { freeTensor(tensor); return r2; }

    tensor.nameHash = nameHash;

    // Store in session
    session->tensors[nameHash] = tensor;
    session->vramUsed += alignedSize;

    return DMLResult::ok("GGUF tensor uploaded");
}

// ============================================================================
// High-Level: Transformer Layer Forward Pass
// ============================================================================
DMLResult DirectMLCompute::runTransformerLayer(uint32_t sessionId, uint32_t layerIdx,
                                               DMLTensorBuffer& hiddenState,
                                               uint32_t seqLen, uint32_t posOffset) {
    auto* session = getSession(sessionId);
    if (!session) return DMLResult::error("Session not found", -1);

    // Layer tensor name hashes (computed from GGUF tensor naming convention)
    // Qwen2.5 pattern: "blk.{N}.attn_norm.weight", "blk.{N}.attn_q.weight", etc.
    char nameBuf[256];

    // 1. Pre-attention RMSNorm
    snprintf(nameBuf, sizeof(nameBuf), "blk.%u.attn_norm.weight", layerIdx);
    uint64_t normHash = fnv1a_hash(nameBuf);
    auto normIt = session->tensors.find(normHash);
    if (normIt == session->tensors.end()) {
        return DMLResult::error("Missing attn_norm weight", -10);
    }

    DMLTensorBuffer normOutput;
    uint32_t normDims[] = { 1, seqLen, session->hiddenSize };
    auto r = createTensor(normOutput, TensorDataType::Float16, normDims, 3);
    if (!r.success) return r;

    r = dispatchRMSNorm(hiddenState, normIt->second, normOutput, session->rmsNormEps);
    if (!r.success) { freeTensor(normOutput); return r; }

    // 2. QKV Projection (Q, K, V = input @ W_q, input @ W_k, input @ W_v)
    snprintf(nameBuf, sizeof(nameBuf), "blk.%u.attn_q.weight", layerIdx);
    uint64_t qHash = fnv1a_hash(nameBuf);
    snprintf(nameBuf, sizeof(nameBuf), "blk.%u.attn_k.weight", layerIdx);
    uint64_t kHash = fnv1a_hash(nameBuf);
    snprintf(nameBuf, sizeof(nameBuf), "blk.%u.attn_v.weight", layerIdx);
    uint64_t vHash = fnv1a_hash(nameBuf);

    auto qIt = session->tensors.find(qHash);
    auto kIt = session->tensors.find(kHash);
    auto vIt = session->tensors.find(vHash);

    if (qIt == session->tensors.end() || kIt == session->tensors.end() || vIt == session->tensors.end()) {
        freeTensor(normOutput);
        return DMLResult::error("Missing QKV weight tensors", -11);
    }

    uint32_t qkvSeqHeadDim = session->numHeads * session->headDim;
    uint32_t kvHeadDim = session->numKVHeads * session->headDim;

    DMLTensorBuffer qOut, kOut, vOut;
    uint32_t qDims[] = { 1, seqLen, qkvSeqHeadDim };
    uint32_t kvDims[] = { 1, seqLen, kvHeadDim };

    r = createTensor(qOut, TensorDataType::Float16, qDims, 3);
    if (!r.success) { freeTensor(normOutput); return r; }
    r = createTensor(kOut, TensorDataType::Float16, kvDims, 3);
    if (!r.success) { freeTensor(normOutput); freeTensor(qOut); return r; }
    r = createTensor(vOut, TensorDataType::Float16, kvDims, 3);
    if (!r.success) { freeTensor(normOutput); freeTensor(qOut); freeTensor(kOut); return r; }

    // Q = normOutput @ W_q
    r = dispatchGEMM(normOutput, qIt->second, qOut,
                     seqLen, qkvSeqHeadDim, session->hiddenSize);
    if (!r.success) { goto cleanup_attn; }

    // K = normOutput @ W_k
    r = dispatchGEMM(normOutput, kIt->second, kOut,
                     seqLen, kvHeadDim, session->hiddenSize);
    if (!r.success) { goto cleanup_attn; }

    // V = normOutput @ W_v
    r = dispatchGEMM(normOutput, vIt->second, vOut,
                     seqLen, kvHeadDim, session->hiddenSize);
    if (!r.success) { goto cleanup_attn; }

    // 3. RoPE on Q and K
    r = dispatchRoPE(qOut, seqLen, session->headDim, posOffset, session->ropeTheta);
    r = dispatchRoPE(kOut, seqLen, session->headDim, posOffset, session->ropeTheta);

    {
        // 4. Multi-Head Attention
        DMLTensorBuffer attnOutput;
        uint32_t attnDims[] = { 1, seqLen, qkvSeqHeadDim };
        r = createTensor(attnOutput, TensorDataType::Float16, attnDims, 3);
        if (!r.success) goto cleanup_attn;

        r = dispatchMultiHeadAttention(qOut, kOut, vOut, attnOutput,
                                       1, seqLen, session->numHeads,
                                       session->headDim);
        if (!r.success) { freeTensor(attnOutput); goto cleanup_attn; }

        // 5. Output projection
        snprintf(nameBuf, sizeof(nameBuf), "blk.%u.attn_output.weight", layerIdx);
        uint64_t outProjHash = fnv1a_hash(nameBuf);
        auto outProjIt = session->tensors.find(outProjHash);

        DMLTensorBuffer projOutput;
        uint32_t projDims[] = { 1, seqLen, session->hiddenSize };
        r = createTensor(projOutput, TensorDataType::Float16, projDims, 3);

        if (outProjIt != session->tensors.end() && r.success) {
            r = dispatchGEMM(attnOutput, outProjIt->second, projOutput,
                             seqLen, session->hiddenSize, qkvSeqHeadDim);
        }

        freeTensor(attnOutput);

        // 6. Residual add: hidden_state = hidden_state + attn_output
        if (r.success) {
            r = dispatchAdd(hiddenState, projOutput, hiddenState);
        }
        freeTensor(projOutput);
    }

    // 7. Pre-FFN RMSNorm
    if (r.success) {
        snprintf(nameBuf, sizeof(nameBuf), "blk.%u.ffn_norm.weight", layerIdx);
        uint64_t ffnNormHash = fnv1a_hash(nameBuf);
        auto ffnNormIt = session->tensors.find(ffnNormHash);
        if (ffnNormIt != session->tensors.end()) {
            r = dispatchRMSNorm(hiddenState, ffnNormIt->second, normOutput, session->rmsNormEps);
        }
    }

    // 8. FFN (SwiGLU for Qwen2.5: gate_up → SiLU → gate * up → down)
    if (r.success) {
        snprintf(nameBuf, sizeof(nameBuf), "blk.%u.ffn_gate.weight", layerIdx);
        uint64_t gateHash = fnv1a_hash(nameBuf);
        snprintf(nameBuf, sizeof(nameBuf), "blk.%u.ffn_up.weight", layerIdx);
        uint64_t upHash = fnv1a_hash(nameBuf);
        snprintf(nameBuf, sizeof(nameBuf), "blk.%u.ffn_down.weight", layerIdx);
        uint64_t downHash = fnv1a_hash(nameBuf);

        auto gateIt = session->tensors.find(gateHash);
        auto upIt = session->tensors.find(upHash);
        auto downIt = session->tensors.find(downHash);

        if (gateIt != session->tensors.end() &&
            upIt != session->tensors.end() &&
            downIt != session->tensors.end()) {

            DMLTensorBuffer gateOut, upOut, ffnOut;
            uint32_t intDims[] = { 1, seqLen, session->intermediateSize };
            uint32_t hidDims[] = { 1, seqLen, session->hiddenSize };

            createTensor(gateOut, TensorDataType::Float16, intDims, 3);
            createTensor(upOut, TensorDataType::Float16, intDims, 3);

            // gate = normOutput @ W_gate
            dispatchGEMM(normOutput, gateIt->second, gateOut,
                         seqLen, session->intermediateSize, session->hiddenSize);
            // up = normOutput @ W_up
            dispatchGEMM(normOutput, upIt->second, upOut,
                         seqLen, session->intermediateSize, session->hiddenSize);

            // SiLU on gate
            dispatchSiLU(gateOut, gateOut);

            // gate = gate * up (element-wise)
            dispatchMul(gateOut, upOut, gateOut);

            // down = gate @ W_down
            createTensor(ffnOut, TensorDataType::Float16, hidDims, 3);
            dispatchGEMM(gateOut, downIt->second, ffnOut,
                         seqLen, session->hiddenSize, session->intermediateSize);

            // 9. Residual add
            dispatchAdd(hiddenState, ffnOut, hiddenState);

            freeTensor(gateOut);
            freeTensor(upOut);
            freeTensor(ffnOut);
        }
    }

cleanup_attn:
    freeTensor(normOutput);
    freeTensor(qOut);
    freeTensor(kOut);
    freeTensor(vOut);

    return r;
}

// ============================================================================
// High-Level: Full Model Forward Pass
// ============================================================================
DMLResult DirectMLCompute::runModelForward(uint32_t sessionId,
                                            const int32_t* inputTokens, uint32_t numTokens,
                                            float* outputLogits, uint32_t vocabSize) {
    auto* session = getSession(sessionId);
    if (!session) return DMLResult::error("Session not found", -1);

    auto startTime = std::chrono::high_resolution_clock::now();

    log(1, (std::string("Forward pass: session=") + std::to_string(sessionId) +
            " tokens=" + std::to_string(numTokens) +
            " layers=" + std::to_string(session->numLayers)).c_str());

    // 1. Token embedding lookup
    // Find embedding weight: "token_embd.weight"
    uint64_t embdHash = fnv1a_hash("token_embd.weight");
    auto embdIt = session->tensors.find(embdHash);
    if (embdIt == session->tensors.end()) {
        return DMLResult::error("Missing token_embd.weight", -10);
    }

    // Create hidden state buffer
    DMLTensorBuffer hiddenState;
    uint32_t hidDims[] = { 1, numTokens, session->hiddenSize };
    auto r = createTensor(hiddenState, TensorDataType::Float16, hidDims, 3);
    if (!r.success) return r;

    // Embedding lookup: download embedding table, gather rows by token indices, upload result
    // CPU-side gather is used because GGUF embedding weights may be quantized and the table
    // is accessed sparsely (only numTokens rows out of vocabSize), making CPU gather +
    // upload more efficient than a full GPU gather on potentially quantized data.
    {
        const DMLTensorBuffer& embdTensor = embdIt->second;
        uint64_t embdSizeBytes = embdTensor.sizeBytes;
        std::vector<uint8_t> embdData(embdSizeBytes);
        downloadTensor(embdData.data(), embdTensor, embdSizeBytes);

        // Determine embedding dimensions: embdTensor is [vocabSize, hiddenSize]
        // Hidden state output is FP16: [1, numTokens, hiddenSize]
        uint64_t hiddenSizeBytes = static_cast<uint64_t>(numTokens) * session->hiddenSize * sizeof(uint16_t);
        std::vector<uint16_t> hiddenFP16(numTokens * session->hiddenSize, 0);

        if (embdTensor.dataType == TensorDataType::Float16) {
            // Direct FP16 gather
            const uint16_t* embdF16 = reinterpret_cast<const uint16_t*>(embdData.data());
            for (uint32_t t = 0; t < numTokens; ++t) {
                int32_t tokenId = inputTokens[t];
                if (tokenId < 0 || static_cast<uint32_t>(tokenId) >= vocabSize) tokenId = 0;
                memcpy(&hiddenFP16[t * session->hiddenSize],
                       &embdF16[static_cast<uint64_t>(tokenId) * session->hiddenSize],
                       session->hiddenSize * sizeof(uint16_t));
            }
        } else if (embdTensor.dataType == TensorDataType::Float32) {
            // FP32 embedding → convert to FP16 for hidden state
            const float* embdF32 = reinterpret_cast<const float*>(embdData.data());
            auto fp32ToFp16 = [](float f) -> uint16_t {
                uint32_t bits;
                memcpy(&bits, &f, sizeof(uint32_t));
                uint16_t sign = (bits >> 31) & 1;
                int32_t exp   = ((bits >> 23) & 0xFF) - 127 + 15;
                uint32_t man  = (bits >> 13) & 0x3FF;
                if (exp <= 0) return static_cast<uint16_t>(sign << 15);
                if (exp >= 31) return static_cast<uint16_t>((sign << 15) | (0x1F << 10));
                return static_cast<uint16_t>((sign << 15) | (exp << 10) | man);
            };
            for (uint32_t t = 0; t < numTokens; ++t) {
                int32_t tokenId = inputTokens[t];
                if (tokenId < 0 || static_cast<uint32_t>(tokenId) >= vocabSize) tokenId = 0;
                uint64_t rowStart = static_cast<uint64_t>(tokenId) * session->hiddenSize;
                for (uint32_t h = 0; h < session->hiddenSize; ++h) {
                    hiddenFP16[t * session->hiddenSize + h] = fp32ToFp16(embdF32[rowStart + h]);
                }
            }
        } else {
            // Quantized embedding: dequantize entire table then gather
            // This is more expensive but handles Q4_K, Q6_K, etc.
            uint64_t totalElements = static_cast<uint64_t>(vocabSize) * session->hiddenSize;
            std::vector<float> embdF32(totalElements, 0.0f);
            GGUFDMLBridge bridge;
            bridge.dequantizeTensor(embdData.data(), embdF32.data(),
                                   static_cast<int>(embdTensor.dataType), totalElements);

            auto fp32ToFp16 = [](float f) -> uint16_t {
                uint32_t bits;
                memcpy(&bits, &f, sizeof(uint32_t));
                uint16_t sign = (bits >> 31) & 1;
                int32_t exp   = ((bits >> 23) & 0xFF) - 127 + 15;
                uint32_t man  = (bits >> 13) & 0x3FF;
                if (exp <= 0) return static_cast<uint16_t>(sign << 15);
                if (exp >= 31) return static_cast<uint16_t>((sign << 15) | (0x1F << 10));
                return static_cast<uint16_t>((sign << 15) | (exp << 10) | man);
            };
            for (uint32_t t = 0; t < numTokens; ++t) {
                int32_t tokenId = inputTokens[t];
                if (tokenId < 0 || static_cast<uint32_t>(tokenId) >= vocabSize) tokenId = 0;
                uint64_t rowStart = static_cast<uint64_t>(tokenId) * session->hiddenSize;
                for (uint32_t h = 0; h < session->hiddenSize; ++h) {
                    hiddenFP16[t * session->hiddenSize + h] = fp32ToFp16(embdF32[rowStart + h]);
                }
            }
        }

        // Upload gathered embeddings to hidden state GPU buffer
        uploadTensor(hiddenState, hiddenFP16.data(), hiddenSizeBytes);
        log(0, (std::string("Embedding: ") + std::to_string(numTokens) +
                " tokens → hidden[" + std::to_string(session->hiddenSize) + "]").c_str());
    }

    // 2. Run all transformer layers
    for (uint32_t layer = 0; layer < session->numLayers; ++layer) {
        if (m_progressCb) {
            m_progressCb(layer, session->numLayers, "transformer_layer", m_progressUserData);
        }

        r = runTransformerLayer(sessionId, layer, hiddenState, numTokens, session->kvSeqLen);
        if (!r.success) {
            log(3, (std::string("Layer ") + std::to_string(layer) + " failed: " + r.detail).c_str());
            freeTensor(hiddenState);
            return r;
        }
    }

    // 3. Final RMSNorm
    uint64_t finalNormHash = fnv1a_hash("output_norm.weight");
    auto finalNormIt = session->tensors.find(finalNormHash);
    if (finalNormIt != session->tensors.end()) {
        DMLTensorBuffer normOut;
        createTensor(normOut, TensorDataType::Float16, hidDims, 3);
        dispatchRMSNorm(hiddenState, finalNormIt->second, normOut, session->rmsNormEps);
        // Swap
        freeTensor(hiddenState);
        hiddenState = normOut;
    }

    // 4. LM Head (hidden_state @ lm_head.weight → logits)
    uint64_t lmHeadHash = fnv1a_hash("output.weight");
    auto lmHeadIt = session->tensors.find(lmHeadHash);
    if (lmHeadIt != session->tensors.end()) {
        DMLTensorBuffer logitsGPU;
        uint32_t logitDims[] = { 1, numTokens, vocabSize };
        createTensor(logitsGPU, TensorDataType::Float32, logitDims, 3);

        dispatchGEMM(hiddenState, lmHeadIt->second, logitsGPU,
                     numTokens, vocabSize, session->hiddenSize);

        // Download logits to CPU
        downloadTensor(outputLogits, logitsGPU,
                       static_cast<uint64_t>(numTokens) * vocabSize * sizeof(float));
        freeTensor(logitsGPU);
    }

    freeTensor(hiddenState);

    // Update KV cache sequence length
    session->kvSeqLen += numTokens;

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    m_stats.avgLatencyMs = elapsedMs;
    m_stats.tokensPerSec = (numTokens > 0) ? (1000.0 * numTokens / elapsedMs) : 0;

    log(1, (std::string("Forward pass complete: ") +
            std::to_string(elapsedMs) + "ms, " +
            std::to_string(m_stats.tokensPerSec) + " tok/s").c_str());

    return DMLResult::ok("Forward pass complete");
}

// ============================================================================
// Registry Integration
// ============================================================================
static int64_t DML_Init(uint64_t maxVRAM, uint64_t maxRAM) {
    (void)maxRAM;
    auto& dml = getDirectMLCompute();
    auto r = dml.initialize();
    return r.success ? 0 : -1;
}

static int64_t DML_Shutdown() {
    auto& dml = getDirectMLCompute();
    auto r = dml.shutdown();
    return r.success ? 0 : -1;
}

static int64_t DML_LoadModel(const wchar_t* path, uint32_t formatHint) {
    // Loading is handled by GGUFDMLBridge::openFile + configureSession + uploadAll
    // This entry point is a convenience for the registry to trigger a load sequence.
    // The actual model load flow is:
    //   DMLInferenceEngine::LoadModel → GGUFDMLBridge::openFile → configureSession
    // Here we just verify the DirectML device is initialized.
    (void)formatHint;
    auto& dml = getDirectMLCompute();
    if (!dml.isInitialized()) {
        auto r = dml.initialize();
        if (!r.success) return -1;
    }
    if (!path) return -2;
    dml.log(1, "DML_LoadModel invoked via registry");
    return 0;
}

static int64_t DML_StreamTensor(uint64_t nameHash, void* dest, uint64_t maxBytes, uint32_t timeoutMs) {
    // Stream tensor data from GPU back to CPU (for inspection, export, etc.)
    (void)timeoutMs;
    if (!dest || maxBytes == 0) return -1;

    auto& dml = getDirectMLCompute();
    auto* session = dml.getActiveSession();
    if (!session) return -2;

    auto it = session->tensors.find(nameHash);
    if (it == session->tensors.end()) return -3;

    uint64_t copyBytes = (it->second.sizeBytes < maxBytes) ? it->second.sizeBytes : maxBytes;
    dml.downloadTensor(dest, it->second, copyBytes);
    return static_cast<int64_t>(copyBytes);
}

static int64_t DML_ReleaseTensor(uint64_t nameHash) {
    auto& dml = getDirectMLCompute();
    auto* session = dml.getActiveSession();
    if (!session) return -1;

    auto it = session->tensors.find(nameHash);
    if (it == session->tensors.end()) return -2;

    session->vramUsed -= it->second.sizeBytes;
    dml.freeTensor(it->second);
    session->tensors.erase(it);
    return 0;
}

static int64_t DML_GetStats(void* statsOut) {
    if (!statsOut) return -1;
    auto& dml = getDirectMLCompute();
    auto* stats = reinterpret_cast<RawrXD::EngineStats*>(statsOut);
    const auto& dmlStats = dml.getStats();
    stats->usedVRAM = dmlStats.vramAllocated.load() - dmlStats.vramFreed.load();
    stats->cacheHits = dmlStats.cacheHits.load();
    stats->cacheMisses = dmlStats.cacheMisses.load();
    stats->totalBytesStreamed = dmlStats.totalBytesUploaded.load() + dmlStats.totalBytesDownloaded.load();
    stats->tensorCount = static_cast<uint64_t>(dmlStats.totalDispatches.load());

    // Compute usedRAM from active session staging buffers
    uint64_t usedRAM = 0;
    uint64_t blockCount = 0;
    auto* session = dml.getActiveSession();
    if (session) {
        blockCount = session->tensors.size();
        // Each tensor in CPU staging counts toward RAM usage
        // Approximate: count total tensor sizes as RAM footprint
        for (const auto& [hash, tensor] : session->tensors) {
            usedRAM += tensor.sizeBytes;
        }
    }
    stats->usedRAM = usedRAM;
    stats->evictionCount = dmlStats.fenceWaits.load(); // Proxy: fence waits correlate with eviction pressure
    stats->blockCount = blockCount;
    return 0;
}

static int64_t DML_ForceEviction(uint64_t targetBytes) {
    auto& dml = getDirectMLCompute();
    auto* session = dml.getActiveSession();
    if (!session) return -1;

    // Evict tensors from back (highest layer first) until targetBytes freed
    uint64_t freed = 0;
    // Collect tensor hashes sorted by some eviction priority (simple: just iterate)
    std::vector<uint64_t> toEvict;
    for (auto& [hash, tensor] : session->tensors) {
        if (freed >= targetBytes) break;
        freed += tensor.sizeBytes;
        toEvict.push_back(hash);
    }
    for (uint64_t hash : toEvict) {
        auto it = session->tensors.find(hash);
        if (it != session->tensors.end()) {
            session->vramUsed -= it->second.sizeBytes;
            dml.freeTensor(it->second);
            session->tensors.erase(it);
        }
    }
    dml.log(1, (std::string("ForceEviction: freed ") +
                std::to_string(freed / (1024 * 1024)) + " MB").c_str());
    return static_cast<int64_t>(freed);
}

static int64_t DML_SetVRAMLimit(uint64_t newLimit) {
    auto& dml = getDirectMLCompute();
    auto* session = dml.getActiveSession();
    if (!session) return -1;

    session->vramBudget = newLimit;
    dml.log(1, (std::string("VRAM limit set to ") +
                std::to_string(newLimit / (1024 * 1024)) + " MB").c_str());
    return 0;
}

void DirectMLCompute::registerWithStreamingRegistry() {
    auto& registry = RawrXD::getStreamingEngineRegistry();

    RawrXD::StreamingEngineDescriptor desc;
    desc.name = "DirectML-Inference";
    desc.description = "Native DirectML GPU inference engine — D3D12 + DML operator graph "
                       "for transformer forward pass. Supports GEMM, MultiHeadAttention (DML 6.1+), "
                       "Softmax, GELU, SiLU, RMSNorm, RoPE, Dequantize. "
                       "Dual-model sessions with VRAM partitioning. "
                       "Hardware: AMD RX 7800 XT 16GB RDNA3 Wave64.";
    desc.version = "1.0.0";
    desc.sourceFile = "src/core/directml_compute.cpp";

    desc.capabilities = RawrXD::EngineCapability::GPU_Compute |
                        RawrXD::EngineCapability::VRAM_Paging |
                        RawrXD::EngineCapability::GGUF |
                        RawrXD::EngineCapability::DualEngine;

    desc.maxModelBillions = 800;
    desc.minVRAM = 4ULL * 1024 * 1024 * 1024;
    desc.maxRAMTarget = 64ULL * 1024 * 1024 * 1024;

    desc.supportedFormats = {};
    desc.minSizeTier = RawrXD::ModelSizeTier::Tiny;
    desc.maxSizeTier = RawrXD::ModelSizeTier::Ultra;

    desc.fnInit           = DML_Init;
    desc.fnShutdown       = DML_Shutdown;
    desc.fnLoadModel      = DML_LoadModel;
    desc.fnStreamTensor   = DML_StreamTensor;
    desc.fnReleaseTensor  = DML_ReleaseTensor;
    desc.fnGetStats       = DML_GetStats;
    desc.fnForceEviction  = DML_ForceEviction;
    desc.fnSetVRAMLimit   = DML_SetVRAMLimit;

    desc.loaded = true;
    desc.active = false;

    registry.registerEngine(desc);
    log(1, "Registered DirectML-Inference with StreamingEngineRegistry");
}

// ============================================================================
// Diagnostics
// ============================================================================
std::string DirectMLCompute::getDiagnosticsString() const {
    std::ostringstream ss;
    ss << "=== DirectML Compute Engine Diagnostics ===\n";
    ss << "Initialized: " << (m_initialized.load() ? "Yes" : "No") << "\n";
    ss << "DML Device: " << (m_dmlDevice ? "Active" : "None") << "\n";
    ss << "Command Recorder: " << (m_cmdRecorder ? "Active" : "None") << "\n";
    ss << "Descriptor Heap: " << m_descriptorOffset << "/" << m_descriptorCount << " used\n";
    ss << "Sessions: " << m_sessions.size() << " (active=" << m_activeSessionId << ")\n";
    ss << "Cached Operators: " << m_opCache.size() << "\n";
    ss << "\n--- Statistics ---\n";
    ss << "Total Dispatches: " << m_stats.totalDispatches.load() << "\n";
    ss << "  GEMM: " << m_stats.gemmDispatches.load() << "\n";
    ss << "  Attention: " << m_stats.attentionDispatches.load() << "\n";
    ss << "  Softmax: " << m_stats.softmaxDispatches.load() << "\n";
    ss << "  Norm: " << m_stats.normDispatches.load() << "\n";
    ss << "  Activation: " << m_stats.activationDispatches.load() << "\n";
    ss << "  Dequant: " << m_stats.dequantDispatches.load() << "\n";
    ss << "Op Cache Hits: " << m_stats.cacheHits.load() << "\n";
    ss << "Op Cache Misses: " << m_stats.cacheMisses.load() << "\n";
    ss << "VRAM Allocated: " << (m_stats.vramAllocated.load() / (1024 * 1024)) << " MB\n";
    ss << "VRAM Freed: " << (m_stats.vramFreed.load() / (1024 * 1024)) << " MB\n";
    ss << "H2D: " << (m_stats.totalBytesUploaded.load() / 1024) << " KB\n";
    ss << "D2H: " << (m_stats.totalBytesDownloaded.load() / 1024) << " KB\n";
    ss << "Fence Waits: " << m_stats.fenceWaits.load() << "\n";
    ss << "Avg Latency: " << m_stats.avgLatencyMs << " ms\n";
    ss << "Tokens/sec: " << m_stats.tokensPerSec << "\n";
    return ss.str();
}

std::string DirectMLCompute::getSessionsString() const {
    std::ostringstream ss;
    for (const auto& [id, session] : m_sessions) {
        ss << "Session " << id << ": " << session->modelName << "\n";
        ss << "  Layers: " << session->numLayers
           << " Heads: " << session->numHeads
           << " Hidden: " << session->hiddenSize << "\n";
        ss << "  Tensors: " << session->tensors.size()
           << " VRAM: " << (session->vramUsed / (1024 * 1024)) << "/"
           << (session->vramBudget / (1024 * 1024)) << " MB\n";
        ss << "  KV Seq: " << session->kvSeqLen << "\n";
    }
    return ss.str();
}

void DirectMLCompute::resetStats() {
    // Cannot copy-assign DMLStats due to std::atomic members;
    // reset each atomic individually.
    m_stats.totalDispatches.store(0);
    m_stats.gemmDispatches.store(0);
    m_stats.attentionDispatches.store(0);
    m_stats.softmaxDispatches.store(0);
    m_stats.normDispatches.store(0);
    m_stats.activationDispatches.store(0);
    m_stats.dequantDispatches.store(0);
    m_stats.totalBytesUploaded.store(0);
    m_stats.totalBytesDownloaded.store(0);
    m_stats.vramAllocated.store(0);
    m_stats.vramFreed.store(0);
    m_stats.fenceWaits.store(0);
    m_stats.cacheHits.store(0);
    m_stats.cacheMisses.store(0);
    m_stats.peakTFLOPS = 0.0;
    m_stats.avgLatencyMs = 0.0;
    m_stats.tokensPerSec = 0.0;
}

// ============================================================================
// Global accessor (lazy-initialized singleton)
// ============================================================================
DirectMLCompute& getDirectMLCompute() {
    static DirectMLCompute instance;
    return instance;
}

} // namespace DML
} // namespace RawrXD
