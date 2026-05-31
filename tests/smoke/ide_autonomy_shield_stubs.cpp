// Test-only stubs for MASM Shield_* entry points referenced by sovereign / Nous
// orchestrator TUes linked into test_ide_model_autonomy (no full agent ASM pack).

#include <cstdint>

extern "C" void Shield_AgentDispatch(const uint8_t* /*plan_hash*/, void* /*context*/) {}

extern "C" uint32_t Shield_VerifyAgentPlan(const uint8_t* /*plan_hash*/)
{
    return 1u;
}

extern "C" void Shield_GoalPush(const uint8_t* /*goal_hash*/) {}

extern "C" void Shield_GoalPop(uint8_t* out_goal_hash)
{
    if (!out_goal_hash)
        return;
    for (int i = 0; i < 32; ++i)
        out_goal_hash[i] = 0;
}
