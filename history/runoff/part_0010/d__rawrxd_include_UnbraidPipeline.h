/*
 * UnbraidPipeline.h — Pipeline Unbraid Engine C/C++ API
 *
 * Hot-patches pipeline stages out of the execution path under memory pressure.
 * As the model demands more memory, lowest-priority stages are "unbraided"
 * (bypassed via trampoline hot-patch) and their working buffers freed.
 * When pressure drops, stages are re-braided in reverse priority order.
 *
 * Engine architecture:
 *   - Up to 32 pipeline stages, each with an execute_fn and bypass_fn
 *   - RWX trampoline per stage: JMP to execute_fn (braided) or bypass_fn (unbraided)
 *   - Memory pressure = (allocated * 100) / budget
 *   - Unbraiding starts at 75% pressure, aggressive at 90%
 *   - Rebraiding starts below 60% pressure
 *   - Stage priority: lower value = unbraided first, rebraided last
 *
 * Build: Link with RawrXD_UnbraidPipeline.obj
 */

#ifndef RAWRXD_UNBRAID_PIPELINE_H
#define RAWRXD_UNBRAID_PIPELINE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Opaque context handle ─────────────────────────────────────────────── */
typedef void* UnbraidCtx;

/* ─── Stage function signatures ─────────────────────────────────────────── */
/*
 * Each stage is called through its trampoline with:
 *   arg1 (RCX) = input data pointer
 *   arg2 (RDX) = input size
 */
typedef void (*UnbraidStageFn)(void* data, uint64_t size);

/* ─── Statistics structure (128 bytes, mirrors GetStats layout) ─────────── */
#pragma pack(push, 8)
typedef struct UnbraidStats {
    uint64_t tickCount;       /* 0x00 — Total ticks                       */
    uint64_t memBudget;       /* 0x08 — Memory budget (bytes)             */
    uint64_t memAllocated;    /* 0x10 — Currently allocated (bytes)       */
    uint32_t pressurePct;     /* 0x18 — Current memory pressure %         */
    uint32_t stageCount;      /* 0x1C — Registered stages                 */
    uint32_t braidedCount;    /* 0x20 — Currently braided (active)        */
    uint32_t unbraidedCount;  /* 0x24 — Currently unbraided (bypassed)    */
    uint64_t totalUnbraids;   /* 0x28 — Total unbraid operations          */
    uint64_t totalRebraids;   /* 0x30 — Total rebraid operations          */
    uint64_t memFreed;        /* 0x38 — Total bytes freed by unbraiding   */
    uint64_t memRestored;     /* 0x40 — Total bytes restored by rebraiding*/
    uint64_t pipeRuns;        /* 0x48 — Total pipeline executions         */
    uint64_t bypassRuns;      /* 0x50 — Total bypass invocations          */
    uint64_t fullRuns;        /* 0x58 — Total full (braided) invocations  */
    uint64_t patchSwaps;      /* 0x60 — Trampoline hot-swap count         */
    uint64_t _reserved[3];    /* 0x68 — Padding to 128 bytes              */
} UnbraidStats;
#pragma pack(pop)

/* ─── Pressure thresholds (defaults) ────────────────────────────────────── */
#define UNBRAID_PRESSURE_START   75   /* Start unbraiding at 75%           */
#define UNBRAID_PRESSURE_AGGR    90   /* Aggressive unbraiding at 90%      */
#define UNBRAID_REBRAID_BELOW    60   /* Start rebraiding below 60%        */
#define UNBRAID_MAX_STAGES       32

/* ─── API ───────────────────────────────────────────────────────────────── */

/*
 * Unbraid_Init — Create the unbraid engine
 * @param memBudget  Total memory budget in bytes
 * @return Context handle, or NULL on failure
 */
UnbraidCtx __cdecl Unbraid_Init(uint64_t memBudget);

/*
 * Unbraid_Destroy — Destroy engine and free all resources
 * @param ctx  Context handle
 */
void __cdecl Unbraid_Destroy(UnbraidCtx ctx);

/*
 * Unbraid_RegisterStage — Register a pipeline stage
 * @param ctx        Context handle
 * @param executeFn  Function pointer for real execution
 * @param bypassFn   Function pointer for bypass (NOP/passthrough)
 * @param bufSize    Working buffer size (0 = no buffer needed)
 * @param priority   Lower value = unbraided first, rebraided last
 * @return Stage index (0-based), or -1 if pipeline is full
 */
int __cdecl Unbraid_RegisterStage(UnbraidCtx ctx,
                                   UnbraidStageFn executeFn,
                                   UnbraidStageFn bypassFn,
                                   uint64_t bufSize,
                                   uint32_t priority);

/*
 * Unbraid_Tick — Check memory pressure, unbraid/rebraid as needed
 * @param ctx  Context handle
 * @return Current pressure % (0-100)
 *
 * Call this every frame/tick. The engine will:
 *   - At >= 75%: unbraid 1 lowest-priority braided stage per tick
 *   - At >= 90%: unbraid 2 stages per tick (aggressive)
 *   - At <  60%: rebraid 1 highest-priority unbraided stage per tick
 */
int __cdecl Unbraid_Tick(UnbraidCtx ctx);

/*
 * Unbraid_Execute — Run entire pipeline through trampolines
 * @param ctx   Context handle
 * @param data  Input data pointer (passed to each stage)
 * @param size  Input data size
 * @return Number of stages executed (braided + bypassed)
 *
 * Each stage is called in registration order via its trampoline.
 * Braided stages run their execute_fn; unbraided stages run bypass_fn.
 */
int __cdecl Unbraid_Execute(UnbraidCtx ctx, void* data, uint64_t size);

/*
 * Unbraid_SetBudget — Set/change memory budget
 * @param ctx     Context handle
 * @param budget  New budget in bytes
 */
void __cdecl Unbraid_SetBudget(UnbraidCtx ctx, uint64_t budget);

/*
 * Unbraid_AddPressure — Report that bytes have been allocated (model needs more)
 * @param ctx    Context handle
 * @param bytes  Bytes being allocated
 * @return New total allocated
 */
uint64_t __cdecl Unbraid_AddPressure(UnbraidCtx ctx, uint64_t bytes);

/*
 * Unbraid_ReleasePressure — Report that bytes have been freed
 * @param ctx    Context handle
 * @param bytes  Bytes being freed
 * @return New total allocated
 */
uint64_t __cdecl Unbraid_ReleasePressure(UnbraidCtx ctx, uint64_t bytes);

/*
 * Unbraid_GetPressurePct — Get current memory pressure
 * @param ctx  Context handle
 * @return Pressure % (0-100)
 */
int __cdecl Unbraid_GetPressurePct(UnbraidCtx ctx);

/*
 * Unbraid_ForceUnbraid — Force-unbraid a specific stage regardless of pressure
 * @param ctx   Context handle
 * @param idx   Stage index
 * @return 1 on success, 0 on failure
 */
int __cdecl Unbraid_ForceUnbraid(UnbraidCtx ctx, int idx);

/*
 * Unbraid_ForceRebraid — Force-rebraid a specific stage regardless of pressure
 * @param ctx   Context handle
 * @param idx   Stage index
 * @return 1 on success, 0 on failure
 */
int __cdecl Unbraid_ForceRebraid(UnbraidCtx ctx, int idx);

/*
 * Unbraid_GetStats — Read statistics into a 128-byte buffer
 * @param ctx   Context handle
 * @param out   Pointer to UnbraidStats (128 bytes)
 * @return 1 on success, 0 on failure
 */
int __cdecl Unbraid_GetStats(UnbraidCtx ctx, UnbraidStats* out);

/* ─── Inline helpers ────────────────────────────────────────────────────── */

static inline double Unbraid_PressureRatio(UnbraidCtx ctx) {
    return (double)Unbraid_GetPressurePct(ctx) / 100.0;
}

static inline int Unbraid_IsUnderPressure(UnbraidCtx ctx) {
    return Unbraid_GetPressurePct(ctx) >= UNBRAID_PRESSURE_START;
}

static inline int Unbraid_IsCritical(UnbraidCtx ctx) {
    return Unbraid_GetPressurePct(ctx) >= UNBRAID_PRESSURE_AGGR;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* RAWRXD_UNBRAID_PIPELINE_H */
