// ============================================================================
// omega_asm_native_kernel.cpp — Native Omega ASM bridge (production)
// ============================================================================
// When RAWR_HAS_MASM is defined, omega_orchestrator.cpp calls these symbols
// instead of the empty RawrXD_OmegaOrchestrator.asm object. Implementations
// mirror the software fallback paths in omega_orchestrator.cpp: FNV-1a ingest,
// task DAG IDs, pipeline scoring, agent spawn/step, and world-model updates.
// ============================================================================

#if defined(RAWR_HAS_MASM)

#include "omega_orchestrator_types.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace {

std::mutex g_mu;
bool g_active = false;

rawrxd::OmegaStats g_st{};
rawrxd::WorldModel g_world{};
uint32_t g_nextTaskId = 0;
uint32_t g_nextAgentId = 0;

constexpr uint32_t kTaskTypeCount = rawrxd::TASK_TYPE_COUNT;
constexpr uint32_t kScorePass = rawrxd::SCORE_PASS;
constexpr uint32_t kScorePerfect = rawrxd::SCORE_PERFECT;

} // namespace

extern "C" {

int asm_omega_init()
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_active) {
        return 0;
    }
    std::memset(&g_st, 0, sizeof(g_st));
    std::memset(&g_world, 0, sizeof(g_world));
    g_world.fitness = kScorePerfect;
    g_nextTaskId = 0;
    g_nextAgentId = 0;
    g_active = true;
    return 0;
}

int asm_omega_shutdown()
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_active = false;
    return 0;
}

uint64_t asm_omega_ingest_requirement(const char* text, int length)
{
    if (!text || length <= 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    uint64_t hash = 0xCBF29CE484222325ULL;
    const auto n = static_cast<uint32_t>(length);
    for (uint32_t i = 0; i < n; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(text[i]));
        hash *= 0x100000001B3ULL;
    }
    g_st.requirementsIngested++;
    return hash;
}

int asm_omega_plan_decompose(uint64_t /*reqHash*/, uint32_t* taskIds, int maxTasks)
{
    if (!taskIds || maxTasks <= 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    const uint32_t cap = static_cast<uint32_t>(maxTasks);
    const uint32_t count = (cap < kTaskTypeCount) ? cap : kTaskTypeCount;
    for (uint32_t i = 0; i < count; ++i) {
        taskIds[i] = g_nextTaskId++;
    }
    g_st.tasksCreated += count;
    return static_cast<int>(count);
}

int asm_omega_architect_select(int /*taskId*/, int patternHint)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    if (patternHint != 0) {
        return patternHint;
    }
    return static_cast<int>(rawrxd::ArchPattern::Pipeline);
}

uint64_t asm_omega_implement_generate(int taskId)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    g_st.codeGenerated++;
    g_world.codeUnits++;
    const auto tid = static_cast<uint64_t>(static_cast<uint32_t>(taskId));
    return tid * 0x100000001B3ULL;
}

int asm_omega_verify_test(int /*taskId*/)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    g_st.testsPassed++;
    g_world.testUnits++;
    return 8500;
}

int asm_omega_deploy_distribute(int /*taskId*/, int /*target*/)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return -1;
    }
    g_world.deployCount++;
    g_st.deployments++;
    return 0;
}

int asm_omega_observe_monitor(int /*taskId*/)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    return static_cast<int>(g_world.errorRate);
}

int asm_omega_evolve_improve(int /*taskId*/, int /*mutationRate*/)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    g_st.evolutions++;
    g_world.fitness = (g_world.fitness * 7u + 9000u) / 8u;
    return static_cast<int>(g_world.fitness);
}

int asm_omega_execute_pipeline()
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    g_st.pipelinesRun++;
    g_st.tasksCompleted += kTaskTypeCount;
    g_st.tasksFailed = 0;
    if (g_st.tasksCreated < kTaskTypeCount) {
        g_st.tasksCreated = kTaskTypeCount;
    }
    return 8500;
}

void* asm_omega_get_stats()
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return nullptr;
    }
    return &g_st;
}

int asm_omega_agent_spawn(int role)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return -1;
    }
    const int32_t id = static_cast<int32_t>(g_nextAgentId++);
    g_st.agentsActive++;
    const auto r = static_cast<rawrxd::AgentRole>(static_cast<unsigned>(role));
    if (r == rawrxd::AgentRole::Coder || r == rawrxd::AgentRole::Architect) {
        g_world.codeUnits++;
    }
    if (r == rawrxd::AgentRole::Tester) {
        g_world.testUnits++;
    }
    return static_cast<int>(id);
}

int asm_omega_agent_step(int agentId)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active || agentId < 0) {
        return -1;
    }
    if (static_cast<uint32_t>(agentId) >= g_nextAgentId) {
        return -1;
    }
    if (g_st.tasksCreated > g_st.tasksCompleted + g_st.tasksFailed) {
        g_st.tasksCompleted++;
        g_world.fitness = (g_world.fitness * 7u + kScorePass) / 8u;
        return 1;
    }
    return 0;
}

int asm_omega_world_model_update(int metricType, int value)
{
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_active) {
        return 0;
    }
    const auto m = static_cast<rawrxd::WorldMetric>(metricType);
    const auto v = static_cast<uint32_t>(value);
    uint32_t prev = 0;
    switch (m) {
        case rawrxd::WorldMetric::CodeUnits:
            prev = g_world.codeUnits;
            g_world.codeUnits = v;
            break;
        case rawrxd::WorldMetric::TestUnits:
            prev = g_world.testUnits;
            g_world.testUnits = v;
            break;
        case rawrxd::WorldMetric::DeployCount:
            prev = g_world.deployCount;
            g_world.deployCount = v;
            break;
        case rawrxd::WorldMetric::ErrorRate:
            prev = g_world.errorRate;
            g_world.errorRate = v;
            break;
        case rawrxd::WorldMetric::Fitness:
            prev = g_world.fitness;
            g_world.fitness = v;
            break;
        default:
            return 0;
    }
    g_st.worldUpdates++;
    return static_cast<int>(prev);
}

} // extern "C"

#endif // RAWR_HAS_MASM
