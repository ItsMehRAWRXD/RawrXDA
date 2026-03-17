#pragma once
// ============================================================================
// Phase 48: The Final Crucible — Unified Stress-Test Harness
// ============================================================================
//
// Chains three "barrel" tests into a single orchestrated gauntlet:
//
//   Barrel 1 — Shadow Patch:
//     Hotpatch a running Unreal PIE session with an AI-generated,
//     SSA-optimized algorithm from Phase 15's StaticAnalysisEngine.
//
//   Barrel 2 — Cluster Hammer:
//     Fire a distributed pipeline DAG that benchmarks Flash Attention
//     kernels across networked compute nodes via the SwarmCoordinator.
//
//   Barrel 3 — Semantic Index:
//     Build a full cross-reference database for a massive codebase
//     using Phase 16's SemanticCodeIntelligence type-inference engine.
//
// No exceptions.  No std::function.  Function pointer callbacks only.
// ============================================================================

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <chrono>

// Subsystem headers
#include "../core/model_memory_hotpatch.hpp"
#include "../core/byte_level_hotpatcher.hpp"
#include "../core/unified_hotpatch_manager.hpp"
#include "../core/static_analysis_engine.hpp"
#include "../core/semantic_code_intelligence.hpp"
#include "../core/distributed_pipeline_orchestrator.hpp"
#include "../core/flash_attention.h"
#include "../core/swarm_coordinator.h"
#include "game_engine_manager.h"

namespace RawrXD {
namespace Crucible {

// ============================================================================
// Crucible Test IDs — Each barrel contains multiple sub-stages
// ============================================================================
enum class CrucibleBarrel : int {
    ShadowPatch    = 0,   // Barrel 1
    ClusterHammer  = 1,   // Barrel 2
    SemanticIndex  = 2,   // Barrel 3
    Count          = 3
};

enum class CrucibleStage : int {
    // Barrel 1: Shadow Patch stages
    SP_AcquireTarget           = 0,    // Find function in running PIE
    SP_DisassembleTarget       = 1,    // Extract instruction stream
    SP_BuildCFG                = 2,    // Build Control Flow Graph
    SP_ConvertToSSA            = 3,    // Transform to SSA form
    SP_RunOptimizations        = 4,    // Constant prop + DCE + CSE
    SP_GeneratePatchPayload    = 5,    // Convert optimized IR → bytes
    SP_ArmHotpatch             = 6,    // Arm the memory hotpatch
    SP_ApplyShadowPatch        = 7,    // Atomic live-patch into PIE

    // Barrel 2: Cluster Hammer stages
    CH_InitPipeline            = 8,    // Initialize DAG scheduler
    CH_DiscoverNodes           = 9,    // Enumerate swarm compute nodes
    CH_AllocateFlashBuffers    = 10,   // Allocate Q/K/V/O matrices
    CH_BuildBenchmarkDAG       = 11,   // Build multi-stage benchmark DAG
    CH_DistributeTasks         = 12,   // Submit tasks to work-stealing pool
    CH_ExecuteForward          = 13,   // Run Flash Attention forward passes
    CH_CollectMetrics          = 14,   // Aggregate TFLOPS/latency/throughput
    CH_ValidateOutputs         = 15,   // Compare against reference outputs

    // Barrel 3: Semantic Index stages
    SI_ScanSourceTree          = 16,   // Recursively scan source directory
    SI_ParseCompilationUnits   = 17,   // Parse headers & source files
    SI_BuildSymbolTable        = 18,   // Register all symbols
    SI_ResolveTypes            = 19,   // Run type inference
    SI_BuildCallGraph          = 20,   // Construct callgraph edges
    SI_ComputeCrossReferences  = 21,   // Link definitions ↔ usages
    SI_SerializeIndex          = 22,   // Write xref DB to disk
    SI_ValidateIndex           = 23,   // Self-check consistency

    StageCount                 = 24
};

// ============================================================================
// Stage Result (extends PatchResult pattern)
// ============================================================================
struct CrucibleStageResult {
    bool          success      = false;
    const char*   detail       = "";
    int           errorCode    = 0;
    double        durationMs   = 0.0;
    CrucibleStage stage        = CrucibleStage::SP_AcquireTarget;
    uint64_t      itemsProcessed = 0;

    static CrucibleStageResult pass(CrucibleStage s, const char* msg, double ms = 0.0,
                                     uint64_t items = 0) {
        return {true, msg, 0, ms, s, items};
    }
    static CrucibleStageResult fail(CrucibleStage s, const char* msg, int code = -1) {
        return {false, msg, code, 0.0, s, 0};
    }
};

// ============================================================================
// Barrel Summary — Results for one barrel (8 stages)
// ============================================================================
struct CrucibleBarrelSummary {
    CrucibleBarrel barrel        = CrucibleBarrel::ShadowPatch;
    const char*    barrelName    = "";
    bool           allPassed     = false;
    int            stagesPassed  = 0;
    int            stagesFailed  = 0;
    double         totalMs       = 0.0;
    uint64_t       totalItems    = 0;

    CrucibleStageResult stages[8] = {};
    int stageCount = 0;
};

// ============================================================================
// Full Crucible Summary — All 3 barrels combined
// ============================================================================
static constexpr int CRUCIBLE_STAGE_COUNT = static_cast<int>(CrucibleStage::StageCount);

struct CrucibleSummary {
    CrucibleBarrelSummary barrels[3]              = {};
    CrucibleStageResult   allStages[CRUCIBLE_STAGE_COUNT] = {};
    int     totalStages    = CRUCIBLE_STAGE_COUNT;
    int     passed         = 0;
    int     failed         = 0;
    double  totalMs        = 0.0;
    bool    allPassed      = false;

    // Flash Attention benchmark specifics
    double  flashAttnTFLOPS   = 0.0;
    double  flashAttnLatencyMs = 0.0;
    int     flashAttnHeads     = 0;
    int     flashAttnSeqLen    = 0;

    // Semantic Index specifics
    uint64_t symbolsIndexed    = 0;
    uint64_t crossRefsBuilt    = 0;
    uint64_t filesScanned      = 0;
    uint64_t callEdgesFound    = 0;

    // Shadow Patch specifics
    uint64_t ssaInstructionsOptimized = 0;
    uint64_t bytesPatched             = 0;
    uint64_t cfgBlocksAnalyzed        = 0;
};

// ============================================================================
// Crucible Configuration
// ============================================================================
struct CrucibleConfig {
    // Barrel 1: Shadow Patch
    std::string  targetProjectPath;    // Unreal project path (or empty for synthetic)
    std::string  targetFunctionName;   // Function to hotpatch (or auto-select)
    bool         useSyntheticTarget = true;  // Use synthetic function if no UE project

    // Barrel 2: Cluster Hammer
    int          flashAttnHeadDim   = 64;
    int          flashAttnHeads     = 32;
    int          flashAttnSeqLen    = 2048;
    int          flashAttnBatch     = 1;
    int          benchmarkIterations = 10;
    bool         useDistributedNodes = false;  // true = swarm, false = local only

    // Barrel 3: Semantic Index
    std::string  sourceTreePath;       // Directory to index (or cwd)
    int          maxFilesToIndex  = 10000;
    bool         indexHeadersOnly = false;
    std::vector<std::string> fileExtensions = {".h", ".hpp", ".cpp", ".c"};

    // General
    bool         abortOnFirstFailure = false;
    bool         verbose             = true;
    int          timeoutPerStageMs   = 30000;
};

// ============================================================================
// Callbacks
// ============================================================================
typedef void (*CrucibleProgressCallback)(CrucibleStage stage, float progress,
                                          const char* detail, void* userData);
typedef void (*CrucibleStageCompleteCallback)(const CrucibleStageResult* result,
                                               void* userData);
typedef void (*CrucibleCompleteCallback)(const CrucibleSummary* summary,
                                          void* userData);

// ============================================================================
// CrucibleEngine — The Orchestrator
// ============================================================================
class CrucibleEngine {
public:
    CrucibleEngine();
    ~CrucibleEngine();

    // ── Lifecycle ──
    bool initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(); }

    // ── Configuration ──
    void setConfig(const CrucibleConfig& config);
    CrucibleConfig getConfig() const;

    // ── Run the Crucible ──
    CrucibleSummary runAll();                               // Run all 3 barrels
    CrucibleBarrelSummary runBarrel(CrucibleBarrel barrel);  // Run one barrel
    CrucibleStageResult runStage(CrucibleStage stage);       // Run one stage

    // ── Async ──
    bool runAllAsync();
    bool isRunning() const { return m_running.load(); }
    float getProgress() const { return m_progress.load(); }
    void cancel();

    // ── Results ──
    CrucibleSummary getLastSummary() const;
    std::string getReport() const;            // Human-readable text report
    std::string toJSON() const;               // Machine-readable JSON

    // ── Callbacks ──
    void setProgressCallback(CrucibleProgressCallback cb, void* ud)
        { m_progressCb = cb; m_progressUd = ud; }
    void setStageCompleteCallback(CrucibleStageCompleteCallback cb, void* ud)
        { m_stageCompleteCb = cb; m_stageCompleteUd = ud; }
    void setCompleteCallback(CrucibleCompleteCallback cb, void* ud)
        { m_completeCb = cb; m_completeUd = ud; }

    // ── Helpers ──
    static const char* barrelName(CrucibleBarrel b);
    static const char* stageName(CrucibleStage s);

private:
    // ── Barrel Runners ──
    CrucibleBarrelSummary runShadowPatch();
    CrucibleBarrelSummary runClusterHammer();
    CrucibleBarrelSummary runSemanticIndex();

    // ── Barrel 1: Shadow Patch stages ──
    CrucibleStageResult stage_SP_AcquireTarget();
    CrucibleStageResult stage_SP_DisassembleTarget();
    CrucibleStageResult stage_SP_BuildCFG();
    CrucibleStageResult stage_SP_ConvertToSSA();
    CrucibleStageResult stage_SP_RunOptimizations();
    CrucibleStageResult stage_SP_GeneratePatchPayload();
    CrucibleStageResult stage_SP_ArmHotpatch();
    CrucibleStageResult stage_SP_ApplyShadowPatch();

    // ── Barrel 2: Cluster Hammer stages ──
    CrucibleStageResult stage_CH_InitPipeline();
    CrucibleStageResult stage_CH_DiscoverNodes();
    CrucibleStageResult stage_CH_AllocateFlashBuffers();
    CrucibleStageResult stage_CH_BuildBenchmarkDAG();
    CrucibleStageResult stage_CH_DistributeTasks();
    CrucibleStageResult stage_CH_ExecuteForward();
    CrucibleStageResult stage_CH_CollectMetrics();
    CrucibleStageResult stage_CH_ValidateOutputs();

    // ── Barrel 3: Semantic Index stages ──
    CrucibleStageResult stage_SI_ScanSourceTree();
    CrucibleStageResult stage_SI_ParseCompilationUnits();
    CrucibleStageResult stage_SI_BuildSymbolTable();
    CrucibleStageResult stage_SI_ResolveTypes();
    CrucibleStageResult stage_SI_BuildCallGraph();
    CrucibleStageResult stage_SI_ComputeCrossReferences();
    CrucibleStageResult stage_SI_SerializeIndex();
    CrucibleStageResult stage_SI_ValidateIndex();

    // ── Timing helper ──
    struct StageTimer {
        std::chrono::steady_clock::time_point start;
        void begin() { start = std::chrono::steady_clock::now(); }
        double elapsedMs() const {
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
        }
    };

    void reportProgress(CrucibleStage stage, float progress, const char* detail);
    void recordStageResult(const CrucibleStageResult& result);
    CrucibleBarrelSummary summarizeBarrel(CrucibleBarrel barrel,
                                           const CrucibleStageResult* stages,
                                           int count) const;

    // ── Shadow Patch working state ──
    void*    m_targetFunctionAddr = nullptr;
    uint64_t m_targetFunctionId   = 0;
    size_t   m_targetFunctionSize = 0;
    std::vector<uint8_t> m_patchPayload;
    std::vector<uint8_t> m_originalBytes;
    MemoryPatchEntry      m_patchEntry = {};

    // ── Cluster Hammer working state ──
    std::vector<float> m_flashQ, m_flashK, m_flashV, m_flashO, m_flashRef;
    uint64_t m_benchmarkTaskIds[16] = {};
    int      m_benchmarkTaskCount    = 0;
    double   m_benchmarkTFLOPS       = 0.0;
    double   m_benchmarkLatencyMs    = 0.0;

    // ── Semantic Index working state ──
    std::vector<std::string> m_sourceFiles;
    uint64_t m_symbolCount    = 0;
    uint64_t m_crossRefCount  = 0;
    uint64_t m_callEdgeCount  = 0;

    // ── Engine state ──
    std::atomic<bool>  m_initialized{false};
    std::atomic<bool>  m_running{false};
    std::atomic<bool>  m_cancelled{false};
    std::atomic<float> m_progress{0.0f};
    mutable std::mutex m_mutex;

    CrucibleConfig  m_config;
    CrucibleSummary m_lastSummary;

    // Callbacks
    CrucibleProgressCallback       m_progressCb      = nullptr;
    void*                          m_progressUd       = nullptr;
    CrucibleStageCompleteCallback  m_stageCompleteCb  = nullptr;
    void*                          m_stageCompleteUd  = nullptr;
    CrucibleCompleteCallback       m_completeCb       = nullptr;
    void*                          m_completeUd       = nullptr;
};

// ============================================================================
// Global Crucible Engine
// ============================================================================
extern CrucibleEngine* g_crucibleEngine;

} // namespace Crucible
} // namespace RawrXD
