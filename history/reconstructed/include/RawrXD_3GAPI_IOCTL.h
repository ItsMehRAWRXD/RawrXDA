#ifndef RAWRXD_3GAPI_IOCTL_H
#define RAWRXD_3GAPI_IOCTL_H

#include <windows.h>
#include <winioctl.h>

// =============================================================================
// RawrXD 3GAPI IOCTL Interface
// User-mode to kernel-mode communication for hotpatchable tensor engine
// =============================================================================

// Device interface GUID
DEFINE_GUID(GUID_RAWRXD_3GAPI_INTERFACE,
    0x12345678, 0x1234, 0x5678, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0);

// Device name
#define RAWRXD_3GAPI_DEVICE_NAME L"\\Device\\RawrXD_3GAPI"
#define RAWRXD_3GAPI_DOS_NAME    L"\\DosDevices\\RawrXD_3GAPI"

// IOCTL Control Codes
#define RAWRXD_3GAPI_IOCTL_BASE 0x8000

// Initialize tensor engine
#define IOCTL_RAWRXD_INIT_TENSOR_ENGINE \
    CTL_CODE(RAWRXD_3GAPI_IOCTL_BASE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Allocate tensor
#define IOCTL_RAWRXD_ALLOCATE_TENSOR \
    CTL_CODE(RAWRXD_3GAPI_IOCTL_BASE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Hotpatch tensor operation
#define IOCTL_RAWRXD_HOTPATCH_TENSOR \
    CTL_CODE(RAWRXD_3GAPI_IOCTL_BASE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Process tensor batch
#define IOCTL_RAWRXD_PROCESS_BATCH \
    CTL_CODE(RAWRXD_3GAPI_IOCTL_BASE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Set tensor count
#define IOCTL_RAWRXD_SET_TENSOR_COUNT \
    CTL_CODE(RAWRXD_3GAPI_IOCTL_BASE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Get tensor statistics
#define IOCTL_RAWRXD_GET_STATS \
    CTL_CODE(RAWRXD_3GAPI_IOCTL_BASE, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Map sliding door memory
#define IOCTL_RAWRXD_MAP_SLIDING_DOOR \
    CTL_CODE(RAWRXD_3GAPI_IOCTL_BASE, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)

// Unmap sliding door memory
#define IOCTL_RAWRXD_UNMAP_SLIDING_DOOR \
    CTL_CODE(RAWRXD_3GAPI_IOCTL_BASE, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------

#pragma pack(push, 1)

// Tensor descriptor (matches MASM structure)
typedef struct _TENSOR_DESCRIPTOR {
    DWORD   ID;
    DWORD   Type;
    DWORD   UsageFlags;
    DWORD   Status;
    UINT64  SizeBytes;
    UINT64  VirtualAddr;
    UINT64  PhysicalHint;
    UINT64  HotpatchSlotPtr;
    DWORD   RefCount;
    DWORD   ShardIndex;
    BYTE    Reserved[8];
} TENSOR_DESCRIPTOR, *PTENSOR_DESCRIPTOR;

// IOCTL input/output structures
typedef struct _INIT_TENSOR_ENGINE_IN {
    DWORD RequestedTensorCount;
    DWORD MaxMemoryGB;
    BOOL  EnableHotpatching;
    BOOL  EnableSharding;
} INIT_TENSOR_ENGINE_IN, *PINIT_TENSOR_ENGINE_IN;

typedef struct _INIT_TENSOR_ENGINE_OUT {
    DWORD Result;  // 0=Success, 1=OutOfMemory, 2=InvalidConfig
    UINT64 BaseAddr;
    UINT64 TotalBytes;
} INIT_TENSOR_ENGINE_OUT, *PINIT_TENSOR_ENGINE_OUT;

typedef struct _ALLOCATE_TENSOR_IN {
    DWORD TensorID;     // -1 for auto
    DWORD Type;
    UINT64 SizeBytes;
    DWORD UsageFlags;
} ALLOCATE_TENSOR_IN, *PALLOCATE_TENSOR_IN;

typedef struct _ALLOCATE_TENSOR_OUT {
    DWORD Result;  // 0=Success, non-zero=error
    TENSOR_DESCRIPTOR Descriptor;
} ALLOCATE_TENSOR_OUT, *PALLOCATE_TENSOR_OUT;

typedef struct _HOTPATCH_TENSOR_IN {
    UINT64 TensorDescriptorPtr;
    UINT64 NewFunctionPtr;
    DWORD  OperationType;
} HOTPATCH_TENSOR_IN, *PHOTPATCH_TENSOR_IN;

typedef struct _HOTPATCH_TENSOR_OUT {
    DWORD Result;  // 0=Success
} HOTPATCH_TENSOR_OUT, *PHOTPATCH_TENSOR_OUT;

typedef struct _PROCESS_BATCH_IN {
    UINT64 TensorArrayPtr;  // Array of TENSOR_DESCRIPTOR*
    DWORD  Count;
    UINT64 OperationPtr;
} PROCESS_BATCH_IN, *PPROCESS_BATCH_IN;

typedef struct _PROCESS_BATCH_OUT {
    DWORD Result;  // 0=Success
} PROCESS_BATCH_OUT, *PPROCESS_BATCH_OUT;

typedef struct _SET_TENSOR_COUNT_IN {
    DWORD DesiredCount;
} SET_TENSOR_COUNT_IN, *PSET_TENSOR_COUNT_IN;

typedef struct _SET_TENSOR_COUNT_OUT {
    DWORD ActualCount;
} SET_TENSOR_COUNT_OUT, *PSET_TENSOR_COUNT_OUT;

typedef struct _GET_STATS_OUT {
    UINT64 Allocations;
    UINT64 Deallocations;
    UINT64 HotpatchSwaps;
    UINT64 MemoryPressure;
} GET_STATS_OUT, *PGET_STATS_OUT;

typedef struct _MAP_SLIDING_DOOR_IN {
    UINT64 ModelSize;  // Size of model to map
    DWORD  ChunkSize;  // Sliding door chunk size
} MAP_SLIDING_DOOR_IN, *PMAP_SLIDING_DOOR_IN;

typedef struct _MAP_SLIDING_DOOR_OUT {
    DWORD  Result;     // 0=Success
    UINT64 MappedAddr; // User-mode address
    HANDLE SectionHandle;
} MAP_SLIDING_DOOR_OUT, *PMAP_SLIDING_DOOR_OUT;

typedef struct _UNMAP_SLIDING_DOOR_IN {
    UINT64 MappedAddr;
    HANDLE SectionHandle;
} UNMAP_SLIDING_DOOR_IN, *PUNMAP_SLIDING_DOOR_IN;

#pragma pack(pop)

// -----------------------------------------------------------------------------
// Helper Functions
// -----------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Device handle management
HANDLE RawrXD_Open3GAPIDevice(void);
BOOL   RawrXD_Close3GAPIDevice(HANDLE hDevice);

// Tensor engine operations
DWORD RawrXD_InitTensorEngine(
    HANDLE hDevice,
    DWORD requestedCount,
    DWORD maxMemoryGB,
    BOOL enableHotpatching,
    BOOL enableSharding,
    PINIT_TENSOR_ENGINE_OUT pOut
);

DWORD RawrXD_AllocateTensor(
    HANDLE hDevice,
    DWORD tensorID,
    DWORD type,
    UINT64 sizeBytes,
    DWORD usageFlags,
    PALLOCATE_TENSOR_OUT pOut
);

DWORD RawrXD_HotpatchTensor(
    HANDLE hDevice,
    PTENSOR_DESCRIPTOR pTensor,
    PVOID newFunction,
    DWORD operationType
);

DWORD RawrXD_ProcessTensorBatch(
    HANDLE hDevice,
    PTENSOR_DESCRIPTOR* tensorArray,
    DWORD count,
    PVOID operation
);

DWORD RawrXD_SetTensorCount(
    HANDLE hDevice,
    DWORD desiredCount,
    PDWORD pActualCount
);

DWORD RawrXD_GetTensorStats(
    HANDLE hDevice,
    PGET_STATS_OUT pStats
);

// Sliding door memory mapping
DWORD RawrXD_MapSlidingDoor(
    HANDLE hDevice,
    UINT64 modelSize,
    DWORD chunkSize,
    PMAP_SLIDING_DOOR_OUT pOut
);

DWORD RawrXD_UnmapSlidingDoor(
    HANDLE hDevice,
    UINT64 mappedAddr,
    HANDLE sectionHandle
);

#ifdef __cplusplus
}
#endif

#endif // RAWRXD_3GAPI_IOCTL_H