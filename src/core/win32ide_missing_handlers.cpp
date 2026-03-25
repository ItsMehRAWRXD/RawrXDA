#include "shared_feature_dispatch.h"

#include <atomic>
#include <string>
#include <cstdio>

namespace {
std::atomic<unsigned int> g_win32_beacon_state{0};
std::atomic<unsigned int> g_win32_beacon_full{0};

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

void updateBeacon(bool highPulse) {
    if (highPulse) {
        g_win32_beacon_state.fetch_or(0x2u, std::memory_order_relaxed);
    } else {
        g_win32_beacon_state.fetch_or(0x1u, std::memory_order_relaxed);
    }
    const unsigned int st = g_win32_beacon_state.load(std::memory_order_relaxed);
    if ((st & 0x3u) == 0x3u) {
        g_win32_beacon_full.store(1u, std::memory_order_relaxed);
    }
}
} // namespace

bool isBeaconFullActive() {
    return g_win32_beacon_full.load(std::memory_order_relaxed) != 0u;
}

CommandResult handleBeaconHalfPulse(const CommandContext& ctx) {
    const std::string arg = firstArg(ctx);
    if (arg == "avx2" || arg == "low" || arg == "0") {
        updateBeacon(false);
        ctx.output("[BEACON] AVX2 half-pulse registered.\n");
    } else if (arg == "avx512" || arg == "high" || arg == "1") {
        updateBeacon(true);
        ctx.output("[BEACON] AVX512 half-pulse registered.\n");
    } else {
        ctx.output("[BEACON] Usage: !beacon_half <avx2|avx512>\n");
    }
    return CommandResult::ok("beacon.halfPulse");
}

CommandResult handleBeaconFullBeacon(const CommandContext& ctx) {
    g_win32_beacon_state.store(0x3u, std::memory_order_relaxed);
    g_win32_beacon_full.store(1u, std::memory_order_relaxed);
    ctx.output("[BEACON] Full beacon forced (0x3).\n");
    return CommandResult::ok("beacon.full");
}

CommandResult handleBeaconStatus(const CommandContext& ctx) {
    const unsigned int st = g_win32_beacon_state.load(std::memory_order_relaxed);
    const unsigned int full = g_win32_beacon_full.load(std::memory_order_relaxed);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "[BEACON] State=0x%X, Full=%u\n", st, full);
    ctx.output(buf);
    return CommandResult::ok("beacon.status");
}

CommandResult handlePluginShowPanel(const CommandContext& ctx) {
    if (ctx.isGui) {
        ctx.output("[PLUGIN] Panel request accepted.\n");
    } else {
        ctx.output("[PLUGIN] Panel command available in GUI mode.\n");
    }
    return CommandResult::ok("plugin.showPanel");
}

#define WIN32IDE_MISSING_HANDLER(name, detail)                 \
    CommandResult name(const CommandContext& ctx) {            \
        if (ctx.outputFn) {                                    \
            ctx.output(detail " fallback active.\n");        \
        }                                                      \
        return CommandResult::ok(detail);                      \
    }

WIN32IDE_MISSING_HANDLER(handlePluginLoad, "plugin.load")
WIN32IDE_MISSING_HANDLER(handlePluginUnload, "plugin.unload")
WIN32IDE_MISSING_HANDLER(handlePluginUnloadAll, "plugin.unloadAll")
WIN32IDE_MISSING_HANDLER(handlePluginRefresh, "plugin.refresh")
WIN32IDE_MISSING_HANDLER(handlePluginScanDir, "plugin.scanDir")
WIN32IDE_MISSING_HANDLER(handlePluginShowStatus, "plugin.status")
WIN32IDE_MISSING_HANDLER(handlePluginToggleHotload, "plugin.toggleHotload")
WIN32IDE_MISSING_HANDLER(handlePluginConfigure, "plugin.configure")
WIN32IDE_MISSING_HANDLER(handleUnrealInit, "unreal.init")
WIN32IDE_MISSING_HANDLER(handleUnrealAttach, "unreal.attach")
WIN32IDE_MISSING_HANDLER(handleUnityInit, "unity.init")
WIN32IDE_MISSING_HANDLER(handleUnityAttach, "unity.attach")
WIN32IDE_MISSING_HANDLER(handleRevengDisassemble, "reveng.disassemble")
WIN32IDE_MISSING_HANDLER(handleRevengDecompile, "reveng.decompile")
WIN32IDE_MISSING_HANDLER(handleRevengFindVulnerabilities, "reveng.findVulns")
WIN32IDE_MISSING_HANDLER(handleDiskListDrives, "disk.listDrives")
WIN32IDE_MISSING_HANDLER(handleDiskScanPartitions, "disk.scanPartitions")
WIN32IDE_MISSING_HANDLER(handleGovernorStatus, "governor.status")
WIN32IDE_MISSING_HANDLER(handleGovernorSetPowerLevel, "governor.setPowerLevel")
WIN32IDE_MISSING_HANDLER(handleMarketplaceList, "marketplace.list")
WIN32IDE_MISSING_HANDLER(handleMarketplaceInstall, "marketplace.install")
WIN32IDE_MISSING_HANDLER(handleEmbeddingEncode, "embedding.encode")
WIN32IDE_MISSING_HANDLER(handleVisionAnalyzeImage, "vision.analyze")
WIN32IDE_MISSING_HANDLER(handlePromptClassifyContext, "prompt.classify")

#undef WIN32IDE_MISSING_HANDLER
