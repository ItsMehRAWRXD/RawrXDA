// ============================================================================
// omega_orchestrator.hpp — Phase Ω: The Omega Orchestrator
// ============================================================================
//
// C++ bridge to the RawrXD_OmegaOrchestrator.asm kernel.
// The system that gathers requirements, architects solutions, implements,
// verifies, deploys, optimizes, and self-improves. RawrXD becomes The Last
// Tool — a self-sustaining software entity.
//
// Pipeline: PERCEIVE → PLAN → ARCHITECT → IMPLEMENT → VERIFY →
//           DEPLOY → OBSERVE → EVOLVE
//
// Pattern: singleton + PatchResult error model + RAWR_HAS_MASM guards
// ============================================================================
#pragma once

#include "omega_orchestrator_types.hpp"
#include "model_memory_hotpatch.hpp"    // PatchResult
#include <mutex>

namespace rawrxd {

// ============================================================================
//  ASM Kernel Externs
// ============================================================================
#ifdef RAWR_HAS_MASM
extern "C" {
    int      asm_omega_init();
    uint64_t asm_omega_ingest_requirement(const char* text, int length);
    int      asm_omega_plan_decompose(uint64_t reqHash, uint32_t* taskIds, int maxTasks);
    int      asm_omega_architect_select(int taskId, int patternHint);
    uint64_t asm_omega_implement_generate(int taskId);
    int      asm_omega_verify_test(int taskId);
    int      asm_omega_deploy_distribute(int taskId, int target);
    int      asm_omega_observe_monitor(int taskId);
    int      asm_omega_evolve_improve(int taskId, int mutationRate);
    int      asm_omega_execute_pipeline();
    int      asm_omega_agent_spawn(int role);
    int      asm_omega_agent_step(int agentId);
    int      asm_omega_world_model_update(int metricType, int value);
    void*    asm_omega_get_stats();
    int      asm_omega_shutdown();
}
#endif

// ============================================================================
//  OmegaOrchestrator — Singleton C++ Bridge
// ============================================================================
class OmegaOrchestrator {
public:
    static OmegaOrchestrator& instance();

    /// Initialize the Omega Orchestrator
    PatchResult initialize();

    /// Shutdown the Omega Orchestrator
    PatchResult shutdown();

    /// Ingest a requirement (text description)
    /// Returns requirement hash for tracking
    uint64_t ingestRequirement(const char* text, uint32_t length);

    /// Decompose requirement into pipeline task DAG
    PatchResult planDecompose(uint64_t reqHash, uint32_t* taskIds,
                              uint32_t maxTasks, uint32_t& tasksCreated);

    /// Select architecture pattern for a task
    ArchPattern architectSelect(uint32_t taskId, ArchPattern hint = ArchPattern::Auto);

    /// Generate code for a task
    uint64_t implementGenerate(uint32_t taskId);

    /// Verify/test a task's output
    uint32_t verifyTest(uint32_t taskId);

    /// Deploy task output
    PatchResult deployDistribute(uint32_t taskId, DeployTarget target);

    /// Monitor deployed task
    uint32_t observeMonitor(uint32_t taskId);

    /// Trigger self-improvement evolution
    uint32_t evolveImprove(uint32_t taskId, uint32_t mutationRate);

    /// Execute full pipeline (all tasks in order)
    PipelineResult executePipeline();

    /// Run full autonomous cycle: ingest → plan → execute → evolve
    PipelineResult runAutonomousCycle(const char* requirement, uint32_t length);

    /// Spawn an autonomous agent
    int32_t spawnAgent(AgentRole role);

    /// Step one agent (execute one work unit)
    int32_t stepAgent(int32_t agentId);

    /// Update world model metric
    uint32_t updateWorldModel(WorldMetric metric, uint32_t value);

    /// Get current world model
    WorldModel getWorldModel() const;

    /// Get statistics
    OmegaStats getStats() const;

    /// Is active?
    bool isActive() const { return m_active; }

    /// Diagnostics dump
    void dumpDiagnostics() const;

private:
    OmegaOrchestrator() = default;
    ~OmegaOrchestrator() = default;
    OmegaOrchestrator(const OmegaOrchestrator&) = delete;
    OmegaOrchestrator& operator=(const OmegaOrchestrator&) = delete;

    mutable std::mutex  m_mutex;
    bool                m_active = false;

    // Software fallback state
    OmegaStats          m_stats = {};
    WorldModel          m_world = {};
    uint32_t            m_nextTaskId = 0;
    uint32_t            m_nextAgentId = 0;
};

} // namespace rawrxd
