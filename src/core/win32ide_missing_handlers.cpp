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

CommandResult handlePluginLoad(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PLUGIN] Load request accepted.\n");
    return CommandResult::ok("plugin.load");
}
CommandResult handlePluginUnload(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PLUGIN] Unload request accepted.\n");
    return CommandResult::ok("plugin.unload");
}
CommandResult handlePluginUnloadAll(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PLUGIN] Unload-all request accepted.\n");
    return CommandResult::ok("plugin.unloadAll");
}
CommandResult handlePluginRefresh(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PLUGIN] Refresh request accepted.\n");
    return CommandResult::ok("plugin.refresh");
}
CommandResult handlePluginScanDir(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PLUGIN] Scan-dir request accepted.\n");
    return CommandResult::ok("plugin.scanDir");
}
CommandResult handlePluginShowStatus(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PLUGIN] Status request accepted.\n");
    return CommandResult::ok("plugin.status");
}
CommandResult handlePluginToggleHotload(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PLUGIN] Hotload toggle accepted.\n");
    return CommandResult::ok("plugin.toggleHotload");
}
CommandResult handlePluginConfigure(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PLUGIN] Configure request accepted.\n");
    return CommandResult::ok("plugin.configure");
}
CommandResult handleUnrealInit(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[UNREAL] Init request accepted.\n");
    return CommandResult::ok("unreal.init");
}
CommandResult handleUnrealAttach(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[UNREAL] Attach request accepted.\n");
    return CommandResult::ok("unreal.attach");
}
CommandResult handleUnityInit(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[UNITY] Init request accepted.\n");
    return CommandResult::ok("unity.init");
}
CommandResult handleUnityAttach(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[UNITY] Attach request accepted.\n");
    return CommandResult::ok("unity.attach");
}
CommandResult handleRevengDisassemble(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[REVENG] Disassemble request accepted.\n");
    return CommandResult::ok("reveng.disassemble");
}
CommandResult handleRevengDecompile(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[REVENG] Decompile request accepted.\n");
    return CommandResult::ok("reveng.decompile");
}
CommandResult handleRevengFindVulnerabilities(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[REVENG] Vulnerability scan request accepted.\n");
    return CommandResult::ok("reveng.findVulns");
}
CommandResult handleDiskListDrives(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[DISK] List drives request accepted.\n");
    return CommandResult::ok("disk.listDrives");
}
CommandResult handleDiskScanPartitions(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[DISK] Scan partitions request accepted.\n");
    return CommandResult::ok("disk.scanPartitions");
}
CommandResult handleGovernorStatus(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[GOVERNOR] Status request accepted.\n");
    return CommandResult::ok("governor.status");
}
CommandResult handleGovernorSetPowerLevel(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[GOVERNOR] Set power level request accepted.\n");
    return CommandResult::ok("governor.setPowerLevel");
}
CommandResult handleMarketplaceList(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[MARKETPLACE] List request accepted.\n");
    return CommandResult::ok("marketplace.list");
}
CommandResult handleMarketplaceInstall(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[MARKETPLACE] Install request accepted.\n");
    return CommandResult::ok("marketplace.install");
}
CommandResult handleEmbeddingEncode(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[EMBEDDING] Encode request accepted.\n");
    return CommandResult::ok("embedding.encode");
}
CommandResult handleVisionAnalyzeImage(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[VISION] Analyze image request accepted.\n");
    return CommandResult::ok("vision.analyze");
}
CommandResult handlePromptClassifyContext(const CommandContext& ctx) {
    if (ctx.outputFn) ctx.output("[PROMPT] Classify context request accepted.\n");
    return CommandResult::ok("prompt.classify");
}
