// =============================================================================
// titan_abi.h — MASM-facing flat ABI for TitanEngine
//
// This boundary intentionally hides C++ classes behind a stable C command
// interface so MASM/control-plane code can treat the inference core as a
// memory-mapped instruction device.
// =============================================================================
#pragma once

#include <stdint.h>

#if defined(_WIN32)
  #if defined(TITAN_ABI_EXPORTS)
    #define TITAN_ABI_API __declspec(dllexport)
  #else
    #define TITAN_ABI_API
  #endif
#else
  #define TITAN_ABI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum TitanOpcode {
    TITAN_OP_CREATE_ENGINE   = 1,
    TITAN_OP_DESTROY_ENGINE  = 2,
    TITAN_OP_SUBMIT_CONTEXT  = 3,
    TITAN_OP_RUN_ONCE        = 4,
    TITAN_OP_READ_RESULTS    = 5,
    TITAN_OP_GET_SHM_VIEW    = 6,
    TITAN_OP_STOP_ENGINE     = 7,
    TITAN_OP_CREATE_RX_KERNEL = 8,
    TITAN_OP_DESTROY_RX_KERNEL = 9,
    TITAN_OP_RX_SUBMIT_CONTEXT = 10,
    TITAN_OP_RX_STEP_ONCE = 11,
    TITAN_OP_RX_GET_CHANNEL = 12,
    TITAN_OP_RX_STOP_KERNEL = 13,
    TITAN_OP_RX_READ_DRAFT_BLOCK = 14,

    // Hot-path compatibility aliases used by MASM control stubs.
    TITAN_OP_RX_SUBMIT = TITAN_OP_RX_SUBMIT_CONTEXT,
    TITAN_OP_RX_STEP = TITAN_OP_RX_STEP_ONCE,
    TITAN_OP_RX_CHANNEL = TITAN_OP_RX_GET_CHANNEL
};

#ifndef OP_RX_SUBMIT
#define OP_RX_SUBMIT TITAN_OP_RX_SUBMIT_CONTEXT
#endif

#ifndef OP_RX_STEP
#define OP_RX_STEP TITAN_OP_RX_STEP_ONCE
#endif

#ifndef OP_RX_CHANNEL
#define OP_RX_CHANNEL TITAN_OP_RX_GET_CHANNEL
#endif

enum TitanStatus {
    TITAN_STATUS_OK                = 0,
    TITAN_STATUS_INVALID_ARGUMENT  = 1,
    TITAN_STATUS_NOT_FOUND         = 2,
    TITAN_STATUS_ENGINE_NOT_READY  = 3,
    TITAN_STATUS_INTERNAL_ERROR    = 255
};

#pragma pack(push, 1)
typedef struct TitanAbiCommand {
    uint32_t opcode;
    uint32_t flags;
    uint64_t handle;
    uint64_t ptr0;
    uint64_t ptr1;
    uint32_t count;
    uint32_t reserved;
} TitanAbiCommand;

typedef struct TitanAbiResponse {
    uint32_t status;
    uint32_t value;
    uint64_t handle;
    uint64_t ptr;
} TitanAbiResponse;
#pragma pack(pop)

// Direct functions (C-callable)
TITAN_ABI_API uint64_t Titan_CreateEngine(const char* model_path, const char* shm_name);
TITAN_ABI_API uint32_t Titan_DestroyEngine(uint64_t handle);
TITAN_ABI_API uint32_t Titan_SubmitContext(uint64_t handle, const uint32_t* tokens, uint32_t token_count);
TITAN_ABI_API uint32_t Titan_RunOnce(uint64_t handle);
TITAN_ABI_API uint32_t Titan_ReadResults(uint64_t handle, uint32_t* out_tokens, uint32_t max_tokens);
TITAN_ABI_API uint32_t Titan_GetSharedView(uint64_t handle, void** out_shm_ptr);
TITAN_ABI_API uint32_t Titan_StopEngine(uint64_t handle);

// Collapsed single-header extension kernel exports.
TITAN_ABI_API uint64_t Titan_CreateRxKernel(const char* map_name);
TITAN_ABI_API uint32_t Titan_DestroyRxKernel(uint64_t handle);
TITAN_ABI_API uint32_t Titan_RxSubmitContext(uint64_t handle, const char* context_buf);
TITAN_ABI_API uint32_t Titan_RxStepOnce(uint64_t handle);
TITAN_ABI_API uint32_t Titan_RxGetChannel(uint64_t handle, void** out_channel_ptr);
TITAN_ABI_API uint32_t Titan_RxStopKernel(uint64_t handle);
TITAN_ABI_API uint32_t Titan_RxReadDraftBlock(uint64_t handle, uint32_t* out_tokens8, float* out_conf8);

// Compatibility export names for hot-path redirect code.
TITAN_ABI_API uint32_t Titan_RxSubmit(uint64_t handle, const char* context_buf);
TITAN_ABI_API uint32_t Titan_RxStep(uint64_t handle);
TITAN_ABI_API uint32_t Titan_GetRxSharedChannel(uint64_t handle, void** out_channel_ptr);

// Flat command entry for MASM control stubs.
// Input/output can be passed in RCX/RDX according to Win64 calling convention.
TITAN_ABI_API uint32_t Titan_ExecuteCommand(const TitanAbiCommand* cmd, TitanAbiResponse* out_resp);

#ifdef __cplusplus
} // extern "C"
#endif
