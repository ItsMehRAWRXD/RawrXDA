#include "../agentic/AgentToolHandlers.h"
#include "agent_fallback_tool_router.hpp"
#include "shared_feature_dispatch.h"


#include <atomic>
#include <cstdio>
#include <string>


namespace
{
std::atomic<unsigned int> g_win32_beacon_state{0};
std::atomic<unsigned int> g_win32_beacon_full{0};

std::string firstArg(const CommandContext& ctx)
{
    if (!ctx.args || !ctx.args[0])
    {
        return std::string();
    }
    const char* s = ctx.args;
    while (*s == ' ' || *s == '\t')
    {
        ++s;
    }
    const char* e = s;
    while (*e != '\0' && *e != ' ' && *e != '\t')
    {
        ++e;
    }
    return std::string(s, static_cast<size_t>(e - s));
}

void updateBeacon(bool highPulse)
{
    if (highPulse)
    {
        g_win32_beacon_state.fetch_or(0x2u, std::memory_order_relaxed);
    }
    else
    {
        g_win32_beacon_state.fetch_or(0x1u, std::memory_order_relaxed);
    }
    const unsigned int st = g_win32_beacon_state.load(std::memory_order_relaxed);
    if ((st & 0x3u) == 0x3u)
    {
        g_win32_beacon_full.store(1u, std::memory_order_relaxed);
    }
}
}  // namespace

bool isBeaconFullActive()
{
    return g_win32_beacon_full.load(std::memory_order_relaxed) != 0u;
}

CommandResult handleBeaconHalfPulse(const CommandContext& ctx)
{
    const std::string arg = firstArg(ctx);
    if (arg == "avx2" || arg == "low" || arg == "0")
    {
        updateBeacon(false);
        ctx.output("[BEACON] AVX2 half-pulse registered.\n");
    }
    else if (arg == "avx512" || arg == "high" || arg == "1")
    {
        updateBeacon(true);
        ctx.output("[BEACON] AVX512 half-pulse registered.\n");
    }
    else
    {
        ctx.output("[BEACON] Usage: !beacon_half <avx2|avx512>\n");
    }
    return CommandResult::ok("beacon.halfPulse");
}

CommandResult handleBeaconFullBeacon(const CommandContext& ctx)
{
    g_win32_beacon_state.store(0x3u, std::memory_order_relaxed);
    g_win32_beacon_full.store(1u, std::memory_order_relaxed);
    ctx.output("[BEACON] Full beacon forced (0x3).\n");
    return CommandResult::ok("beacon.full");
}

CommandResult handleBeaconStatus(const CommandContext& ctx)
{
    const unsigned int st = g_win32_beacon_state.load(std::memory_order_relaxed);
    const unsigned int full = g_win32_beacon_full.load(std::memory_order_relaxed);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "[BEACON] State=0x%X, Full=%u\n", st, full);
    ctx.output(buf);
    return CommandResult::ok("beacon.status");
}

CommandResult handlePluginShowPanel(const CommandContext& ctx)
{
    if (ctx.isGui)
    {
        ctx.output("[PLUGIN] Panel request accepted.\n");
    }
    else
    {
        ctx.output("[PLUGIN] Panel command available in GUI mode.\n");
    }
    return CommandResult::ok("plugin.showPanel");
}

CommandResult runWin32MissingFallback(const CommandContext& ctx, const char* handlerName, const char* detail)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    std::string tool;
    RawrXD::Core::ResolveFallbackToolRoute(handlerName ? handlerName : "", ctx.args, args, tool);
    if (tool.empty())
    {
        tool = "load_rules";
    }
    auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
    if (!tool.empty() && handlers.HasTool(tool))
    {
        const auto result = handlers.Execute(tool, args);
        if (ctx.outputFn)
        {
            const std::string payload = std::string(detail) + " backend route result: " + result.toJson().dump() + "\n";
            ctx.output(payload.c_str());
        }
        if (result.isSuccess())
        {
            return CommandResult::ok(detail);
        }
    }
    if (ctx.outputFn)
    {
        ctx.output(detail);
        ctx.output(" fallback active.\n");
    }
    if (ctx.isGui && ctx.hwnd != nullptr)
    {
        if (ctx.outputFn)
        {
            ctx.output(" gui fallback unavailable: explicit GUI dispatch path is not wired.\n");
        }
        return CommandResult::error("gui_dispatch_unwired");
    }
    return CommandResult::error("no_backend_route");
}

CommandResult executeConcreteWin32Tool(const CommandContext& ctx, const char* handlerName, const std::string& toolName,
                                       nlohmann::json args, const char* successTag)
{
    auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
    if (!handlers.HasTool(toolName))
    {
        if (ctx.outputFn)
        {
            const std::string payload =
                std::string(successTag) + " concrete route tool unavailable: " + toolName + "\n";
            ctx.output(payload.c_str());
        }
        return CommandResult::error("concrete_tool_unavailable");
    }

    const auto result = handlers.Execute(toolName, args);
    if (ctx.outputFn)
    {
        const std::string payload =
            std::string(successTag) + " concrete backend route result: " + result.toJson().dump() + "\n";
        ctx.output(payload.c_str());
    }
    if (result.isSuccess())
    {
        return CommandResult::ok(successTag);
    }
    if (ctx.outputFn)
    {
        const std::string payload =
            std::string(successTag) + " concrete route failed for handler " + (handlerName ? handlerName : "") + "\n";
        ctx.output(payload.c_str());
    }
    return CommandResult::error("concrete_backend_route_failed");
}

CommandResult handlePluginLoad(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePluginLoad");
    }
    return executeConcreteWin32Tool(ctx, "handlePluginLoad", "plan_tasks", args, "plugin.load");
}
CommandResult handlePluginUnload(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePluginUnload");
    }
    return executeConcreteWin32Tool(ctx, "handlePluginUnload", "plan_tasks", args, "plugin.unload");
}
CommandResult handlePluginUnloadAll(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePluginUnloadAll");
    }
    return executeConcreteWin32Tool(ctx, "handlePluginUnloadAll", "plan_tasks", args, "plugin.unloadAll");
}
CommandResult handlePluginRefresh(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePluginRefresh");
    }
    return executeConcreteWin32Tool(ctx, "handlePluginRefresh", "plan_tasks", args, "plugin.refresh");
}
CommandResult handlePluginScanDir(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePluginScanDir");
    }
    return executeConcreteWin32Tool(ctx, "handlePluginScanDir", "plan_tasks", args, "plugin.scanDir");
}
CommandResult handlePluginShowStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePluginShowStatus");
    }
    return executeConcreteWin32Tool(ctx, "handlePluginShowStatus", "plan_tasks", args, "plugin.status");
}
CommandResult handlePluginToggleHotload(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePluginToggleHotload");
    }
    return executeConcreteWin32Tool(ctx, "handlePluginToggleHotload", "plan_tasks", args, "plugin.toggleHotload");
}

CommandResult handlePluginConfigure(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePluginConfigure");
    }
    return executeConcreteWin32Tool(ctx, "handlePluginConfigure", "plan_tasks", args, "plugin.configure");
}
CommandResult handleUnrealInit(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleUnrealInit");
    }
    return executeConcreteWin32Tool(ctx, "handleUnrealInit", "plan_tasks", args, "unreal.init");
}
CommandResult handleUnrealAttach(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleUnrealAttach");
    }
    return executeConcreteWin32Tool(ctx, "handleUnrealAttach", "plan_tasks", args, "unreal.attach");
}
CommandResult handleUnityInit(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleUnityInit");
    }
    return executeConcreteWin32Tool(ctx, "handleUnityInit", "plan_tasks", args, "unity.init");
}
CommandResult handleUnityAttach(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleUnityAttach");
    }
    return executeConcreteWin32Tool(ctx, "handleUnityAttach", "plan_tasks", args, "unity.attach");
}
CommandResult handleRevengDisassemble(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRevengDisassemble");
    }
    return executeConcreteWin32Tool(ctx, "handleRevengDisassemble", "plan_tasks", args, "reveng.disassemble");
}
CommandResult handleRevengDecompile(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRevengDecompile");
    }
    return executeConcreteWin32Tool(ctx, "handleRevengDecompile", "plan_tasks", args, "reveng.decompile");
}
CommandResult handleRevengFindVulnerabilities(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRevengFindVulnerabilities");
    }
    return executeConcreteWin32Tool(ctx, "handleRevengFindVulnerabilities", "plan_tasks", args, "reveng.findVulns");
}
CommandResult handleDiskListDrives(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleDiskListDrives");
    }
    return executeConcreteWin32Tool(ctx, "handleDiskListDrives", "plan_tasks", args, "disk.listDrives");
}
CommandResult handleDiskScanPartitions(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleDiskScanPartitions");
    }
    return executeConcreteWin32Tool(ctx, "handleDiskScanPartitions", "plan_tasks", args, "disk.scanPartitions");
}
CommandResult handleGovernorStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleGovernorStatus");
    }
    return executeConcreteWin32Tool(ctx, "handleGovernorStatus", "plan_tasks", args, "governor.status");
}
CommandResult handleGovernorSetPowerLevel(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleGovernorSetPowerLevel");
    }
    return executeConcreteWin32Tool(ctx, "handleGovernorSetPowerLevel", "plan_tasks", args, "governor.setPowerLevel");
}
CommandResult handleMarketplaceList(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleMarketplaceList");
    }
    return executeConcreteWin32Tool(ctx, "handleMarketplaceList", "plan_tasks", args, "marketplace.list");
}
CommandResult handleMarketplaceInstall(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleMarketplaceInstall");
    }
    return executeConcreteWin32Tool(ctx, "handleMarketplaceInstall", "plan_tasks", args, "marketplace.install");
}
CommandResult handleEmbeddingEncode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleEmbeddingEncode");
    }
    return executeConcreteWin32Tool(ctx, "handleEmbeddingEncode", "plan_tasks", args, "embedding.encode");
}
CommandResult handleVisionAnalyzeImage(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVisionAnalyzeImage");
    }
    return executeConcreteWin32Tool(ctx, "handleVisionAnalyzeImage", "plan_tasks", args, "vision.analyze");
}
CommandResult handlePromptClassifyContext(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handlePromptClassifyContext");
    }
    return executeConcreteWin32Tool(ctx, "handlePromptClassifyContext", "plan_tasks", args, "prompt.classify");
}
