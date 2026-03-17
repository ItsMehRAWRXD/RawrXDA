// ============================================================================
// dual_agent_session.hpp — Phase 41: Dual-Agent Orchestration Types
// ============================================================================
//
// Shared header defining C++ struct mirrors of the MASM AGENT_CTX,
// TASK_PKT, and SWARM_STATE structures from RawrXD_DualAgent_Orchestrator.asm.
//
// Used by:
//   - tool_server.cpp         (inference HTTP endpoints)
//   - Win32IDE_LocalServer.cpp (IDE HTTP endpoints)
//   - Any future C++ code needing dual-agent access
//
// Rule: These structs MUST match the ASM layout exactly.
//       Any change here MUST be mirrored in the .asm file.
// ============================================================================

#ifndef RAWRXD_DUAL_AGENT_SESSION_HPP
#define RAWRXD_DUAL_AGENT_SESSION_HPP

#include <cstdint>

// =============================================================================
//                    CONSTANTS (must match ASM EQU values)
// =============================================================================

// Agent IDs
constexpr uint32_t AGENT_ID_ARCHITECT   = 0;
constexpr uint32_t AGENT_ID_CODER       = 1;
constexpr uint32_t AGENT_COUNT          = 2;

// Agent state flags
constexpr uint32_t AGENT_STATE_IDLE         = 0;
constexpr uint32_t AGENT_STATE_INFERENCING  = 1;
constexpr uint32_t AGENT_STATE_STALLED      = 2;
constexpr uint32_t AGENT_STATE_SHUTDOWN     = 0xFFFFFFFF;

// Task types
constexpr uint32_t TASK_TYPE_DESIGN_SPEC    = 0;
constexpr uint32_t TASK_TYPE_CODE_GEN       = 1;
constexpr uint32_t TASK_TYPE_REVIEW         = 2;
constexpr uint32_t TASK_TYPE_CONTEXT_PUSH   = 3;

// Swarm status codes
constexpr uint64_t SWARM_OK                 = 0;
constexpr uint64_t SWARM_ERR_ALLOC_FAIL     = 1;
constexpr uint64_t SWARM_ERR_MODEL_FAIL     = 2;
constexpr uint64_t SWARM_ERR_RING_FULL      = 3;
constexpr uint64_t SWARM_ERR_NOT_INIT       = 4;
constexpr uint64_t SWARM_ERR_ALREADY_INIT   = 5;
constexpr uint64_t SWARM_ERR_SHUTDOWN_FAIL  = 6;

// Ring buffer geometry
constexpr uint64_t RING_SIZE                = 0x04000000;   // 64 MB
constexpr uint64_t RING_MASK                = 0x03FFFFFF;    // RING_SIZE - 1

// Task queue geometry
constexpr uint32_t TASK_PKT_SIZE            = 0x100;        // 256 bytes
constexpr uint32_t MAX_TASKS                = 0x100;         // 256 tasks

// =============================================================================
//                    STRUCT MIRRORS (match ASM layout exactly)
// =============================================================================

// AGENT_CTX — 4096 bytes per agent (page-aligned)
// Must match AGENT_CTX STRUCT in RawrXD_DualAgent_Orchestrator.asm
#pragma pack(push, 1)
struct MasmAgentCtx {
    uint64_t agent_id;          // 0:   AGENT_ID_ARCHITECT or AGENT_ID_CODER
    uint32_t model_profile;     // 8:   Model profile index in bridge table
    uint32_t model_tier;        // 12:  MODEL_TIER_* from bridge
    uint32_t state_flags;       // 16:  AGENT_STATE_* (atomic)
    uint32_t _pad0;             // 20:  Alignment
    uint64_t ring_base;         // 24:  Pointer to shared context ring
    uint64_t ring_head;         // 32:  Producer write position (atomic)
    uint64_t ring_tail;         // 40:  Consumer read position (atomic)
    uint64_t queue_base;        // 48:  Pointer to task queue
    uint32_t queue_head;        // 56:  Task queue producer (atomic)
    uint32_t queue_tail;        // 60:  Task queue consumer (atomic)
    uint64_t task_count;        // 64:  Total tasks processed
    uint64_t error_count;       // 72:  Total errors
    uint64_t last_task_ms;      // 80:  Timestamp of last completed task
    uint8_t  zmm_save_area[2048]; // 88: ZMM0-31 save area (not accessed from C++)
    uint8_t  _reserved[1944];   // 2136: Pad to 4096 bytes total
};
#pragma pack(pop)
static_assert(sizeof(MasmAgentCtx) == 4096, "MasmAgentCtx must be 4096 bytes (page-aligned)");

// TASK_PKT — 256 bytes per task packet (cache-line-aligned)
#pragma pack(push, 1)
struct MasmTaskPkt {
    uint64_t task_id;           // 0:   Unique incrementing ID
    uint32_t task_type;         // 8:   TASK_TYPE_*
    uint8_t  priority;          // 12:  0-255 (255 = real-time)
    uint8_t  _pad0[3];         // 13:  Alignment
    uint32_t source_agent;      // 16:  Who submitted this task
    uint32_t target_agent;      // 20:  Who should execute it
    uint64_t data_offset;       // 24:  Offset into context ring
    uint32_t data_length;       // 32:  Length of data in ring
    uint32_t _pad1;             // 36:  Alignment (implicit from dep_mask alignment)
    uint64_t dep_mask;          // 40:  Bitmask: which tasks must complete first
    uint8_t  _reserved[200];    // 48:  Pad to 256 bytes
};
#pragma pack(pop)
static_assert(sizeof(MasmTaskPkt) == 256, "MasmTaskPkt must be 256 bytes");

// SWARM_STATE — global orchestrator state
#pragma pack(push, 1)
struct MasmSwarmState {
    uint32_t initialized;       // 0:   1 if Swarm_Init completed
    uint32_t agent_count;       // 4:   Always 2 for dual-agent
    uint64_t architect_ctx;     // 8:   Pointer to Architect AGENT_CTX
    uint64_t coder_ctx;         // 16:  Pointer to Coder AGENT_CTX
    uint64_t ring_base;         // 24:  Shared context ring base
    uint64_t ring_size;         // 32:  Ring size in bytes (64MB)
    uint64_t total_handoffs;    // 40:  Total Architect→Coder handoffs
    uint64_t total_tasks;       // 48:  Total tasks submitted
    uint32_t lock_flag;         // 56:  Spinlock for init/shutdown
    uint32_t _pad[3];          // 60:  Alignment
};
#pragma pack(pop)
static_assert(sizeof(MasmSwarmState) == 72, "MasmSwarmState must be 72 bytes");

// =============================================================================
//                    ASM FUNCTION DECLARATIONS
// =============================================================================

#ifdef RAWR_HAS_MASM
extern "C" {
    // model_bridge_x64.asm — Model Bridge
    uint64_t ModelBridge_Init();
    uint64_t ModelBridge_GetProfileCount();
    void*    ModelBridge_GetProfile(uint32_t index);
    void*    ModelBridge_GetProfileByName(const char* name);
    uint64_t ModelBridge_ValidateLoad(uint32_t index);
    uint64_t ModelBridge_LoadModel(uint32_t index);
    uint64_t ModelBridge_UnloadModel();
    void*    ModelBridge_GetActiveProfile();
    void*    ModelBridge_GetState();
    uint32_t ModelBridge_EstimateRAM(uint32_t param_count_b, uint32_t quant_bits);
    uint32_t ModelBridge_GetTierForSize(uint32_t param_count_b);
    const char* ModelBridge_GetQuantName(uint32_t quant_type);
    uint64_t ModelBridge_GetCapabilities();

    // model_bridge_x64.asm — Lock primitives (Phase 40)
    void     AcquireBridgeLock();
    void     ReleaseBridgeLock();
    void*    ValidateModelAlignment(void* load_address);
    uint32_t EstimateRAM_Safe(uint32_t param_count_b, uint32_t quant_bits);

    // RawrXD_DualAgent_Orchestrator.asm — Dual-Agent Swarm (Phase 41)
    uint64_t Swarm_Init(uint32_t architect_idx, uint32_t coder_idx);
    uint64_t Swarm_Shutdown();
    void*    Swarm_GetState();
    uint64_t Swarm_GetAgentStatus(uint32_t agent_id);
    uint64_t Swarm_SubmitTask(void* agent_ctx, void* task_pkt);
    uint64_t Swarm_Handoff(void* data, uint32_t length);

    // RawrXD_DualAgent_Orchestrator.asm — Agent worker loops (Phase 41)
    // InferenceCallback signature: int64_t (*)(MasmAgentCtx*, MasmTaskPkt*)
    // Returns bytes produced (>= 0), or -1 on error.
    // For Coder: task_pkt==NULL signals "consume ring buffer data"
    typedef int64_t (*AgentInferenceCallback)(MasmAgentCtx* ctx, MasmTaskPkt* task);

    uint64_t Agent_Architect_Loop(MasmAgentCtx* ctx, AgentInferenceCallback callback);
    uint64_t Agent_Coder_Loop(MasmAgentCtx* ctx, AgentInferenceCallback callback);
}
#endif // RAWR_HAS_MASM

// =============================================================================
//                    HELPER UTILITIES
// =============================================================================

inline const char* GetAgentStateName(uint32_t state) {
    switch (state) {
        case AGENT_STATE_IDLE:          return "idle";
        case AGENT_STATE_INFERENCING:   return "inferencing";
        case AGENT_STATE_STALLED:       return "stalled";
        case AGENT_STATE_SHUTDOWN:      return "shutdown";
        default:                        return "unknown";
    }
}

inline const char* GetTaskTypeName(uint32_t type) {
    switch (type) {
        case TASK_TYPE_DESIGN_SPEC:     return "design_spec";
        case TASK_TYPE_CODE_GEN:        return "code_gen";
        case TASK_TYPE_REVIEW:          return "review";
        case TASK_TYPE_CONTEXT_PUSH:    return "context_push";
        default:                        return "unknown";
    }
}

inline const char* GetSwarmErrorName(uint64_t code) {
    switch (code) {
        case SWARM_OK:                  return "ok";
        case SWARM_ERR_ALLOC_FAIL:      return "alloc_fail";
        case SWARM_ERR_MODEL_FAIL:      return "model_fail";
        case SWARM_ERR_RING_FULL:       return "ring_full";
        case SWARM_ERR_NOT_INIT:        return "not_initialized";
        case SWARM_ERR_ALREADY_INIT:    return "already_initialized";
        case SWARM_ERR_SHUTDOWN_FAIL:   return "shutdown_fail";
        default:                        return "unknown";
    }
}

#endif // RAWRXD_DUAL_AGENT_SESSION_HPP
