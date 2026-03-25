#include "shared_feature_dispatch.h"

#include <atomic>
#include <string>
#include <cstdio>

namespace {
std::atomic<unsigned int> g_gold_beacon_state{0};
std::atomic<unsigned int> g_gold_full_beacon{0};

std::string firstArg(const CommandContext& ctx) {
    if (!ctx.args || !ctx.args[0]) {
        return std::string();
    }
    const char* s = ctx.args;
    while (*s == ' ' || *s == '\t') {
        ++s;
    }
    const char* e = s;
    while (*e != '\0' && *e != ' ' && *e != '\t') {
        ++e;
    }
    return std::string(s, static_cast<size_t>(e - s));
}

void updateBeaconState(bool avx512Pulse) {
    if (avx512Pulse) {
        g_gold_beacon_state.fetch_or(0x2u, std::memory_order_relaxed);
    } else {
        g_gold_beacon_state.fetch_or(0x1u, std::memory_order_relaxed);
    }
    const unsigned int state = g_gold_beacon_state.load(std::memory_order_relaxed);
    if ((state & 0x3u) == 0x3u) {
        g_gold_full_beacon.store(1u, std::memory_order_relaxed);
    }
}
} // namespace

CommandResult handleBeaconHalfPulse(const CommandContext& ctx) {
    const std::string arg = firstArg(ctx);
    bool accepted = false;

    if (arg == "avx2" || arg == "low" || arg == "0") {
        updateBeaconState(false);
        ctx.output("[BEACON] AVX2 half-pulse registered.\n");
        accepted = true;
    } else if (arg == "avx512" || arg == "high" || arg == "1") {
        updateBeaconState(true);
        ctx.output("[BEACON] AVX512 half-pulse registered.\n");
        accepted = true;
    } else {
        ctx.output("[BEACON] Usage: !beacon_half <avx2|avx512>\n");
    }

    if (accepted && g_gold_full_beacon.load(std::memory_order_relaxed) != 0u) {
        ctx.output("[BEACON] Full beacon acquired - ready for full inference mode\n");
    }

    return CommandResult::ok("beacon.halfPulse");
}

CommandResult handleBeaconFullBeacon(const CommandContext& ctx) {
    g_gold_beacon_state.store(0x3u, std::memory_order_relaxed);
    g_gold_full_beacon.store(1u, std::memory_order_relaxed);
    ctx.output("[BEACON] Full beacon forced (0x3). AVX512 path engaged.\n");
    return CommandResult::ok("beacon.full");
}

CommandResult handleBeaconStatus(const CommandContext& ctx) {
    const unsigned int state = g_gold_beacon_state.load(std::memory_order_relaxed);
    const unsigned int full = g_gold_full_beacon.load(std::memory_order_relaxed);

    char buf[160];
    std::snprintf(buf, sizeof(buf), "[BEACON] State=0x%X, Full=%u\n", state, full);
    ctx.output(buf);

    if (full == 1u) {
        ctx.output("[BEACON] Full beacon active: high-throughput inference mode\n");
    } else {
        if (state == 0u) ctx.output("[BEACON] Idle (no half-pulses seen).\n");
        if (state == 1u) ctx.output("[BEACON] AVX2 half only (Half-Beat mode).\n");
        if (state == 2u) ctx.output("[BEACON] AVX512 half only (Half-Beat mode).\n");
    }

    return CommandResult::ok("beacon.status");
}
