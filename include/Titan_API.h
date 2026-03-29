#pragma once

#include <stdint.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#define TITAN_ALIGN(x) __declspec(align(x))
#else
#define TITAN_ALIGN(x) __attribute__((aligned(x)))
#endif

#ifndef TITAN_API
#define TITAN_API
#endif

/* Ring buffer configuration */
#ifdef __cplusplus
inline constexpr uint32_t TITAN_RING_BUFFER_SIZE   = 64u * 1024u * 1024u;
inline constexpr uint32_t TITAN_RING_SEGMENT_SIZE  = 4u  * 1024u * 1024u;
inline constexpr uint32_t TITAN_RING_SEGMENT_COUNT = TITAN_RING_BUFFER_SIZE / TITAN_RING_SEGMENT_SIZE;
#else
#define TITAN_RING_BUFFER_SIZE        (64u * 1024u * 1024u)
#define TITAN_RING_SEGMENT_SIZE       (4u  * 1024u * 1024u)
#define TITAN_RING_SEGMENT_COUNT      (TITAN_RING_BUFFER_SIZE / TITAN_RING_SEGMENT_SIZE)
#endif

/* Engine types */
typedef enum TitanEngineType {
    TITAN_ENGINE_COMPUTE = 0,
    TITAN_ENGINE_COPY    = 1,
    TITAN_ENGINE_DMA     = 2,
    TITAN_ENGINE_COUNT   = 3
} TitanEngineType;

/* Backpressure levels */
typedef enum TitanBackpressureLevel {
    TITAN_BACKPRESSURE_NONE     = 0,
    TITAN_BACKPRESSURE_LIGHT    = 1,
    TITAN_BACKPRESSURE_MODERATE = 2,
    TITAN_BACKPRESSURE_SEVERE   = 3,
    TITAN_BACKPRESSURE_CRITICAL = 4
} TitanBackpressureLevel;

/* Patch states */
typedef enum TitanPatchState {
    TITAN_PATCH_FREE        = 0,
    TITAN_PATCH_FILLING     = 1,
    TITAN_PATCH_READY       = 2,
    TITAN_PATCH_IN_FLIGHT   = 3,
    TITAN_PATCH_COMPLETE    = 4,
    TITAN_PATCH_ERROR       = 5
} TitanPatchState;

/* Patch flags */
#define TITAN_PATCH_NON_TEMPORAL   0x00000001u
#define TITAN_PATCH_PREFETCH_HINT  0x00000002u
#define TITAN_PATCH_HIGH_PRIORITY  0x00000004u
#define TITAN_PATCH_WAIT_FOR_IDLE  0x00000008u

/* Error codes */
#define TITAN_SUCCESS                 0x00000000u
#define TITAN_ERROR_INVALID_PARAM     0x80070057u
#define TITAN_ERROR_OUT_OF_MEMORY     0x8007000Eu
#define TITAN_ERROR_TIMEOUT           0x800705B4u
#define TITAN_ERROR_VULKAN            0x80040001u
#define TITAN_ERROR_DIRECTSTORAGE     0x80040002u
#define TITAN_ERROR_ENGINE_BUSY       0x80040003u
#define TITAN_ERROR_PATCH_CONFLICT    0x80040004u

struct TitanMemoryPatch;
typedef void (WINAPI *TitanPatchCallback)(void *callbackContext, struct TitanMemoryPatch *patch);

typedef struct TITAN_ALIGN(4096) TitanRingBuffer {
    void    *HostBase;
    void    *DeviceBase;
    uint64_t Size;

    uint64_t WriteHead;
    uint64_t ReadTail;
    uint64_t CommitHead;

    uint64_t SegmentMask;
    uint64_t SegmentLocks[TITAN_RING_SEGMENT_COUNT];

    uint32_t BackpressureLevel;
    uint64_t TargetFillLevel;
    uint64_t MaxFillLevel;

    uint64_t TotalBytesWritten;
    uint64_t TotalBytesRead;
    uint64_t TotalWraps;
    uint64_t StallCount;

    HANDLE  DataAvailable;
    HANDLE  SpaceAvailable;
    SRWLOCK BufferLock;
} TitanRingBuffer;

typedef struct TITAN_ALIGN(64) TitanEngineContext {
    uint32_t EngineType;
    uint32_t EngineId;
    uint32_t Priority;

    void    *VkQueue;
    void    *VkCommandPool;
    void    *VkCommandBuffer;
    void    *VkFence;

    uint32_t IsActive;
    struct TitanMemoryPatch *CurrentPatch;

    uint64_t PatchesProcessed;
    uint64_t BytesTransferred;
    uint64_t TotalLatencyNs;

    HANDLE  WorkerThread;
    HANDLE  ShutdownEvent;

    SRWLOCK EngineLock;
} TitanEngineContext;

typedef struct TITAN_ALIGN(64) TitanMemoryPatch {
    uint64_t PatchId;
    uint64_t StreamId;

    void    *HostAddress;
    void    *DeviceAddress;
    uint64_t Size;

    uint32_t State;
    uint32_t EngineType;

    uint64_t SubmitTime;
    uint64_t StartTime;
    uint64_t CompleteTime;

    struct TitanMemoryPatch *DependsOnPatch;
    struct TitanMemoryPatch *DependentPatch;

    HANDLE  CompletionEvent;
    TitanPatchCallback Callback;
    void    *CallbackContext;
    struct TitanMemoryPatch *NextPatch;

    uint32_t Flags;
    uint32_t Reserved;
} TitanMemoryPatch;

typedef struct TITAN_ALIGN(64) TitanStreamContext {
    uint64_t StreamId;
    uint32_t StreamType;
    uint32_t Priority;

    TitanRingBuffer *RingBuffer;

    uint32_t PrimaryEngine;
    uint32_t FallbackEngine;

    TitanMemoryPatch *PendingPatches;
    TitanMemoryPatch *InFlightPatches;
    TitanMemoryPatch *CompletedPatches;

    uint32_t MaxInFlight;
    uint32_t CurrentInFlight;

    uint64_t TotalPatches;
    uint64_t CompletedCount;
    uint64_t ErrorCount;
    uint64_t AvgLatencyNs;

    uint32_t IsActive;
    uint32_t ShutdownRequested;

    SRWLOCK StreamLock;
} TitanStreamContext;

typedef struct TITAN_ALIGN(4096) TitanOrchestrator {
    uint32_t Magic;
    uint32_t Version;

    uint32_t Flags;
    uint32_t Reserved;

    TitanRingBuffer    RingBuffer;
    TitanEngineContext Engines[TITAN_ENGINE_COUNT];
    void              *Streams[64];

    TitanMemoryPatch  *PatchPool;
    uint64_t           PatchPoolSize;
    TitanMemoryPatch  *FreePatchList;
    SRWLOCK            PatchLock;

    void *VkInstance;
    void *VkPhysicalDevice;
    void *VkDevice;

    void *DSFactory;
    void *DSQueue;

    HANDLE  OrchestratorThread;
    HANDLE  ShutdownEvent;
    uint32_t IsRunning;

    uint64_t TotalBytesStreamed;
    uint64_t TotalPatches;
    uint64_t BackpressureEvents;

    uint32_t Initialized;
    uint8_t  Reserved2[4096];
} TitanOrchestrator;

TITAN_API int32_t Titan_Initialize(void **handle, uint32_t flags);
TITAN_API int32_t Titan_Shutdown(void *handle);

#ifdef __cplusplus
}
#endif
