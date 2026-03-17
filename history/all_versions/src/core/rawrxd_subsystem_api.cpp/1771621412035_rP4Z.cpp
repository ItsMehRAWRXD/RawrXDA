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

// ============================================================
// Timing Helper
// ============================================================

static uint64_t GetTimestampMs() {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
#endif
}

// ============================================================
// Switch Name Table
// ============================================================

static const char* g_switchNames[] = {
    nullptr,            // index 0 unused
    "-compile",         // 1
    "-encrypt",         // 2
    "-inject",          // 3
    "-elevate",         // 4  (renamed from -uac)
    "-persist",         // 5
    "-sideload",        // 6
    "-avscan",          // 7
    "-entropy",         // 8
    "-stubgen",         // 9
    "-trace",           // 10
    "-agent",           // 11
    "-bbcov",           // 12
    "-covfuse",         // 13
    "-dyntrace",        // 14
    "-agenttrace",      // 15
    "-gapfuzz",         // 16
    "-intelpt",         // 17
    "-diffcov",         // 18
    "analyzer",         // 19  (library module, not CLI mode)
    "streaming",        // 20  (library module, not CLI mode)
    "vulkan",           // 21  (library module, not CLI mode)
    "-diskrecovery"     // 22  (library module, disk recovery agent)
};

// ============================================================
// SubsystemRegistry Constructor
// ============================================================

SubsystemRegistry::SubsystemRegistry()
    : m_eventCallback(nullptr)
    , m_eventUserData(nullptr) {

    // Zero-initialize all entries
    memset(m_modes, 0, sizeof(m_modes));

    // Register all mode handlers
    struct Registration {
        SubsystemId id;
        const char* sw;
        ModeHandler handler;
    };

    Registration regs[] = {
        { SubsystemId::Compile,               "-compile",    &SubsystemRegistry::handleCompile },
        { SubsystemId::Encrypt,               "-encrypt",    &SubsystemRegistry::handleEncrypt },
        { SubsystemId::Inject,                "-inject",     &SubsystemRegistry::handleInject },
        { SubsystemId::PrivilegeEscalation,   "-elevate",    &SubsystemRegistry::handlePrivilegeEscalation },
        { SubsystemId::Persist,               "-persist",    &SubsystemRegistry::handlePersist },
        { SubsystemId::Sideload,              "-sideload",   &SubsystemRegistry::handleSideload },
        { SubsystemId::AVScan,                "-avscan",     &SubsystemRegistry::handleAVScan },
        { SubsystemId::Entropy,               "-entropy",    &SubsystemRegistry::handleEntropy },
        { SubsystemId::StubGen,               "-stubgen",    &SubsystemRegistry::handleStubGen },
        { SubsystemId::Trace,                 "-trace",      &SubsystemRegistry::handleTrace },
        { SubsystemId::Agent,                 "-agent",      &SubsystemRegistry::handleAgent },
        { SubsystemId::BBCov,                 "-bbcov",      &SubsystemRegistry::handleBBCov },
        { SubsystemId::CovFusion,             "-covfuse",    &SubsystemRegistry::handleCovFusion },
        { SubsystemId::DynTrace,              "-dyntrace",   &SubsystemRegistry::handleDynTrace },
        { SubsystemId::AgentTrace,            "-agenttrace", &SubsystemRegistry::handleAgentTrace },
        { SubsystemId::GapFuzz,               "-gapfuzz",    &SubsystemRegistry::handleGapFuzz },
        { SubsystemId::IntelPT,               "-intelpt",    &SubsystemRegistry::handleIntelPT },
        { SubsystemId::DiffCov,               "-diffcov",    &SubsystemRegistry::handleDiffCov },
        { SubsystemId::AnalyzerDistiller,     "analyzer",    &SubsystemRegistry::handleAnalyzerDistiller },
        { SubsystemId::StreamingOrchestrator, "streaming",   &SubsystemRegistry::handleStreamingOrchestrator },
        { SubsystemId::VulkanKernel,          "vulkan",      &SubsystemRegistry::handleVulkanKernel },
        { SubsystemId::DiskRecovery,          "-diskrecovery", &SubsystemRegistry::handleDiskRecovery },
    };

    for (const auto& r : regs) {
        int idx = static_cast<int>(r.id) - 1;
        if (idx >= 0 && idx < static_cast<int>(SubsystemId::_Count)) {
            m_modes[idx].id = r.id;
            m_modes[idx].switchName = r.sw;
            m_modes[idx].handler = r.handler;
            m_modes[idx].stats = {};
        }
    }
}
