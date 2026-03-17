/**
 * @file rawrxd_subsystem_api.cpp
 * @brief Implementation of the agent-callable subsystem registry
 *
 * Wraps CLI mode procedures into the SubsystemRegistry dispatch table.
 * Each mode handler calls the corresponding ASM procedure (linked via
 * extern "C") or the C++ equivalent, captures latency, and returns
 * a structured SubsystemResult.
 *
 * NO exceptions. NO std::function. NO Qt.
 * Contract: CLI_CONTRACT_v1.0.md
 */

#include "rawrxd_subsystem_api.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <sddl.h>
#include <accctrl.h>
#include <aclapi.h>
#else
#include <time.h>
#endif

// ============================================================
// ASM Procedure Linkage
// ============================================================
// These are the actual mode procedures from RawrXD_IDE_unified.asm.
// When linked as a unified binary, they're available directly.
// When used as a library, they're resolved at link time.

extern "C" {
    void CompileMode(void);
    void EncryptMode(void);
    void InjectMode(void);
    void PrivilegeEscalationMode(void);  // Renamed from UACBypassMode
    void PersistenceMode(void);
    void SideloadMode(void);
    void AVScanMode(void);
    void EntropyMode(void);
    void StubGenMode(void);
    void TraceEngineMode(void);
    void AgenticMode(void);
    void BasicBlockCovMode(void);
    void CovFusionMode(void);
    void DynTraceMode(void);
    void AgentTraceMode(void);
    void GapFuzzMode(void);
    void IntelPTMode(void);
    void DiffCovMode(void);

    // DiskRecoveryAgent exports (RawrXD_DiskRecoveryAgent.asm)
    int  DiskRecovery_FindDrive(void);
    void* DiskRecovery_Init(int driveNum);
    int  DiskRecovery_ExtractKey(void* ctx);
    void DiskRecovery_Run(void* ctx);
    void DiskRecovery_Abort(void* ctx);
    void DiskRecovery_Cleanup(void* ctx);
    void DiskRecovery_GetStats(void* ctx, uint64_t* outGood, uint64_t* outBad, uint64_t* outCurrent, uint64_t* outTotal);
}

// ---- Library Module Linkage (C ABI) ----
// These resolve to MASM .obj when RAWR_HAS_MASM=1, else to stub .cpp
#include "analyzer_distiller.h"
#include "streaming_orchestrator.h"
