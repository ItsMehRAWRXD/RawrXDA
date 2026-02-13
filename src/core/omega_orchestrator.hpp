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

#include "model_memory_hotpatch.hpp"    // PatchResult
#include <cstdint>
#include <mutex>

namespace rawrxd {

// ============================================================================
//  Enums
// ============================================================================

/// Omega pipeline task types (phases of autonomous development)
enum class OmegaTaskType : uint32_t {
    Perceive  = 0,    // Ingest requirements
    Plan      = 1,    // Decompose into task DAG
    Architect = 2,    // Select patterns and schemas
    Implement = 3,    // Generate code
    Verify    = 4,    // Test and benchmark
    Deploy    = 5,    // Distribute
    Observe   = 6,    // Monitor in production
    Evolve    = 7     // Self-improve
};

/// Task states
enum class TaskState : uint32_t {
    Pending   = 0,
    Ready     = 1,
    Running   = 2,
    Complete  = 3,
    Failed    = 4,
    Blocked   = 5
};

/// Agent roles (specialization)
enum class AgentRole : uint32_t {
    Planner   = 0,
    Architect = 1,
    Coder     = 2,
    Tester    = 3,
    Deployer  = 4,
    Observer  = 5,
    Evolver   = 6
};

/// Architecture patterns selectable by the orchestrator
enum class ArchPattern : uint32_t {
    Auto        = 0,
    Singleton   = 1,
    Pipeline    = 2,
    EventDriven = 3,
    CSP         = 4    // Communicating Sequential Processes
};

/// Deployment targets
enum class DeployTarget : uint32_t {
    Local   = 0,
    Mesh    = 1,
    Cluster = 2,
    Global  = 3
};

/// World model metric types
enum class WorldMetric : uint32_t {
    CodeUnits   = 0,
    TestUnits   = 1,
    DeployCount = 2,
    ErrorRate   = 3,
    Fitness     = 4
};

// ============================================================================
//  Structures
// ============================================================================

static constexpr uint32_t MAX_TASKS       = 512;
static constexpr uint32_t MAX_AGENTS      = 16;
static constexpr uint32_t MAX_DEPS        = 8;
static constexpr uint32_t SCORE_PERFECT   = 10000;
static constexpr uint32_t SCORE_PASS      = 7000;
static constexpr uint32_t TASK_TYPE_COUNT = 8;

/// A single task in the pipeline DAG
struct OmegaTask {
    uint32_t    taskId;
    OmegaTaskType type;
    TaskState   state;
    uint32_t    priority;
    uint32_t    depCount;
    uint32_t    deps[MAX_DEPS];
    uint64_t    inputHash;
    uint64_t    outputHash;
    uint64_t    startTsc;
    uint64_t    endTsc;
    uint32_t    score;          // Quality in basis points (0-10000)
    int32_t     agentId;
};

/// Autonomous agent worker
struct OmegaAgent {
    uint32_t    agentId;
    AgentRole   role;
    int32_t     currentTask;    // -1 = idle
    uint32_t    tasksCompleted;
    uint64_t    totalScore;
    uint64_t    avgLatencyTsc;
    uint32_t    state;          // 0=idle, 1=busy, 2=paused
    uint32_t    errorCount;
};

/// Pipeline execution result
struct PipelineResult {
    uint32_t    tasksCreated;
    uint32_t    tasksCompleted;
    uint32_t    tasksFailed;
    uint32_t    avgScore;           // Basis points
    uint64_t    totalCycles;
    bool        allPassed;
};

/// Omega orchestrator statistics
struct OmegaStats {
    uint64_t tasksCreated;
    uint64_t tasksCompleted;
    uint64_t tasksFailed;
    uint64_t agentsActive;
    uint64_t pipelinesRun;
    uint64_t requirementsIngested;
    uint64_t codeGenerated;
    uint64_t testsPassed;
    uint64_t deployments;
    uint64_t evolutions;
    uint64_t worldUpdates;
    uint64_t avgScoreBP;
    uint64_t _reserved[4];
};

/// World model snapshot
struct WorldModel {
    uint32_t codeUnits;
    uint32_t testUnits;
    uint32_t deployCount;
    uint32_t errorRate;     // Basis points
    uint32_t fitness;       // Basis points
};

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
