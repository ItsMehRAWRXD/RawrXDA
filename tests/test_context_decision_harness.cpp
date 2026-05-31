#include "context_config.h"

#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

constexpr int64_t kTwoGiB = 2LL * 1024LL * 1024LL * 1024LL;

bool setEnvVar(const char* name, const char* value)
{
    if (!name || !*name) {
        return false;
    }

#ifdef _WIN32
    return _putenv_s(name, value ? value : "") == 0;
#else
    if (!value) {
        return unsetenv(name) == 0;
    }
    return setenv(name, value, 1) == 0;
#endif
}

bool unsetEnvVar(const char* name)
{
    if (!name || !*name) {
        return false;
    }

#ifdef _WIN32
    return _putenv_s(name, "") == 0;
#else
    return unsetenv(name) == 0;
#endif
}

bool expectTrue(bool condition, const std::string& label)
{
    if (!condition) {
        std::cerr << "[FAIL] " << label << "\n";
        return false;
    }
    std::cout << "[PASS] " << label << "\n";
    return true;
}

std::string toString(int64_t value)
{
    return std::to_string(static_cast<long long>(value));
}

} // namespace

int main()
{
    using RawrXD::ContextDecision;
    using RawrXD::ContextLimits;
    using RawrXD::ContextResolveHints;

    bool ok = true;

    // Baseline estimate for threshold and no-shrink tests.
    const int32_t requested = ContextLimits::TINY;
    const int64_t estimated = ContextLimits::estimateKVBytes(requested);

    ContextResolveHints hints;
    hints.pressure_threshold = 0.9f;

    // 1) Pressure threshold logic should trigger for budget == estimated (because estimated > budget * 0.9).
    ok = setEnvVar("RAWRXD_KV_BUDGET", toString(estimated).c_str()) && ok;
    ok = unsetEnvVar("RAWRXD_DISABLE_VRAM_PROBE") && ok;
    ContextDecision d1 = RawrXD::ResolveContextDecisionWithHints(requested, hints);
    ok = expectTrue(d1.pressure_detected, "pressure threshold triggers when estimated exceeds thresholded budget") && ok;

    // 2) Adapted must remain false when no actual shrinking occurs.
    ok = expectTrue(!d1.adapted, "adapted remains false when proportional shrink does not reduce effective context") && ok;

    // 3) No adaptation path when budget is comfortably above estimate.
    ok = setEnvVar("RAWRXD_KV_BUDGET", toString(estimated * 2).c_str()) && ok;
    ContextDecision d2 = RawrXD::ResolveContextDecisionWithHints(requested, hints);
    ok = expectTrue(!d2.pressure_detected, "no pressure when budget is above estimated requirement") && ok;
    ok = expectTrue(!d2.adapted, "adapted stays false when no adaptation is needed") && ok;

    // 4) Budget smaller than minimum context footprint should clamp to min context.
    ok = setEnvVar("RAWRXD_KV_BUDGET", "1024") && ok;
    ContextDecision d3 = RawrXD::ResolveContextDecisionWithHints(ContextLimits::LARGE, hints);
    std::cout << "[CTX] requested=" << ContextLimits::LARGE
              << " effective=" << d3.effective
              << " kv_bytes=" << d3.estimated_kv_bytes
              << " kv_budget=" << d3.kv_budget_bytes
              << " pressure=" << (d3.pressure_detected ? 1 : 0)
              << " adapted=" << (d3.adapted ? 1 : 0) << "\n";
    const bool budgetOrMin = (d3.estimated_kv_bytes <= d3.kv_budget_bytes) || (d3.effective == ContextLimits::TINY);
    ok = expectTrue(d3.pressure_detected, "pressure detected under tiny forced budget") && ok;
    ok = expectTrue(d3.adapted, "adapted true when shrink is required under tiny budget") && ok;
    ok = expectTrue(budgetOrMin, "final KV bytes are budget-respecting or clamped to minimum context") && ok;

    // 5) Zero/negative budget values should be treated as unset (probe disabled -> fallback 2 GiB).
    ok = setEnvVar("RAWRXD_DISABLE_VRAM_PROBE", "1") && ok;
    ok = setEnvVar("RAWRXD_KV_BUDGET", "0") && ok;
    ContextDecision d4 = RawrXD::ResolveContextDecisionWithHints(requested, hints);
    ok = expectTrue(d4.kv_budget_bytes == kTwoGiB, "zero budget is treated as unset and falls back to 2 GiB") && ok;

    ok = setEnvVar("RAWRXD_KV_BUDGET", "-1") && ok;
    ContextDecision d5 = RawrXD::ResolveContextDecisionWithHints(requested, hints);
    ok = expectTrue(d5.kv_budget_bytes == kTwoGiB, "negative budget is treated as unset and falls back to 2 GiB") && ok;

    // 6) Fallback to 2 GiB when budget is unset and probe is explicitly disabled.
    ok = unsetEnvVar("RAWRXD_KV_BUDGET") && ok;
    ContextDecision d6 = RawrXD::ResolveContextDecisionWithHints(requested, hints);
    ok = expectTrue(d6.kv_budget_bytes == kTwoGiB, "unset budget falls back to 2 GiB when probe is disabled") && ok;

    // 7) Very large budget should not bypass system/kv safety clamps.
    ok = setEnvVar("RAWRXD_KV_BUDGET", "1099511627776") && ok; // 1 TiB
    ok = unsetEnvVar("RAWRXD_DISABLE_VRAM_PROBE") && ok;
    ContextDecision d7 = RawrXD::ResolveContextDecisionWithHints(ContextLimits::FLAGSHIP, hints);
    const bool safetyBounded = d7.effective <= d7.system_safe_max && d7.effective <= d7.kv_safe_max;
    ok = expectTrue(safetyBounded, "very large budget still respects safety clamps") && ok;

    unsetEnvVar("RAWRXD_KV_BUDGET");
    unsetEnvVar("RAWRXD_DISABLE_VRAM_PROBE");

    if (!ok) {
        std::cerr << "Context decision harness FAILED\n";
        return 1;
    }

    std::cout << "Context decision harness PASSED\n";
    return 0;
}
