// ============================================================================
// omega_orchestrator_types.hpp — Omega enums/stats (no Win32, no PatchResult)
// ============================================================================
// Shared by omega_orchestrator.hpp and omega_asm_native_kernel.cpp.
// ============================================================================
#pragma once

#include <cstdint>

namespace rawrxd {

enum class OmegaTaskType : uint32_t {
    Perceive  = 0,
    Plan      = 1,
    Architect = 2,
    Implement = 3,
    Verify    = 4,
    Deploy    = 5,
    Observe   = 6,
    Evolve    = 7
};

enum class TaskState : uint32_t {
    Pending   = 0,
    Ready     = 1,
    Running   = 2,
    Complete  = 3,
    Failed    = 4,
    Blocked   = 5
};

enum class AgentRole : uint32_t {
    Planner   = 0,
    Architect = 1,
    Coder     = 2,
    Tester    = 3,
    Deployer  = 4,
    Observer  = 5,
    Evolver   = 6
};

enum class ArchPattern : uint32_t {
    Auto        = 0,
    Singleton   = 1,
    Pipeline    = 2,
    EventDriven = 3,
    CSP         = 4
};

enum class DeployTarget : uint32_t {
    Local   = 0,
    Mesh    = 1,
    Cluster = 2,
    Global  = 3
};

enum class WorldMetric : uint32_t {
    CodeUnits   = 0,
    TestUnits   = 1,
    DeployCount = 2,
    ErrorRate   = 3,
    Fitness     = 4
};

static constexpr uint32_t MAX_TASKS       = 512;
static constexpr uint32_t MAX_AGENTS      = 16;
static constexpr uint32_t MAX_DEPS        = 8;
static constexpr uint32_t SCORE_PERFECT   = 10000;
static constexpr uint32_t SCORE_PASS      = 7000;
static constexpr uint32_t TASK_TYPE_COUNT = 8;

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
    uint32_t    score;
    int32_t     agentId;
};

struct OmegaAgent {
    uint32_t    agentId;
    AgentRole   role;
    int32_t     currentTask;
    uint32_t    tasksCompleted;
    uint64_t    totalScore;
    uint64_t    avgLatencyTsc;
    uint32_t    state;
    uint32_t    errorCount;
};

struct PipelineResult {
    uint32_t    tasksCreated;
    uint32_t    tasksCompleted;
    uint32_t    tasksFailed;
    uint32_t    avgScore;
    uint64_t    totalCycles;
    bool        allPassed;
};

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

struct WorldModel {
    uint32_t codeUnits;
    uint32_t testUnits;
    uint32_t deployCount;
    uint32_t errorRate;
    uint32_t fitness;
};

} // namespace rawrxd
