#include "../agentic/AgentToolHandlers.h"
#include "agent_fallback_tool_router.hpp"
#include "fallback_route_metrics.hpp"
#include "feature_handlers.h"


#include <nlohmann/json.hpp>

#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace
{
CommandResult executeConcreteTool(const CommandContext& ctx, const char* handlerName, const std::string& toolName,
                                  nlohmann::json args)
{
    auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
    if (!handlers.HasTool(toolName))
    {
        return CommandResult::error("link_gap_tool_unavailable");
    }

    auto result = handlers.Execute(toolName, args);
    if (ctx.outputFn != nullptr)
    {
        const std::string msg = std::string("[SSOT link-gap] concrete backend route -> ") +
                                (handlerName ? handlerName : "") + " using " + toolName + "\n";
        ctx.output(msg.c_str());
        const std::string payload = result.toJson().dump(2) + "\n";
        ctx.output(payload.c_str());
    }

    if (result.isSuccess())
    {
        RawrXD::Core::RecordFallbackRoute("linkerGap", handlerName ? handlerName : "", toolName, true, false);
        return CommandResult::ok("concrete_backend_route");
    }
    RawrXD::Core::RecordFallbackRoute("linkerGap", handlerName ? handlerName : "", toolName, false, false);
    return CommandResult::error("concrete_backend_route_failed");
}

bool tryHeadlessAgentToolRoute(const CommandContext& ctx, const char* name, CommandResult& outResult)
{
    const std::string handlerName = name ? name : "";
    std::string toolName;
    nlohmann::json args = nlohmann::json::object();
    if (ctx.args && ctx.args[0] != '\0')
    {
        try
        {
            args = nlohmann::json::parse(ctx.args);
            if (!args.is_object())
            {
                args = nlohmann::json::object();
            }
        }
        catch (...)
        {
            args = nlohmann::json::object();
            args["query"] = std::string(ctx.args);
        }
    }
    const bool routedByShared = RawrXD::Core::ResolveFallbackToolRoute(handlerName, ctx.args, args, toolName);

    if (!routedByShared && (handlerName == "handleHelpSearch" || handlerName == "handleEditFindNext" ||
                            handlerName == "handleEditFindPrev"))
    {
        toolName = "search_code";
        if (!args.contains("query"))
            args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("TODO");
        args["max_results"] = 50;
    }
    else if (!routedByShared && handlerName == "handleToolsTerminal")
    {
        toolName = "run_shell";
        if (!(ctx.args && ctx.args[0]))
        {
            outResult = CommandResult::error("tools.terminal requires a command argument");
            return true;
        }
        args["command"] = std::string(ctx.args);
    }
    else if (!routedByShared && (handlerName == "handleViewTerminal" || handlerName == "handleViewToggleTerminal" ||
                                 handlerName == "handleTerminalSplitCode"))
    {
        toolName = "list_dir";
        args["path"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string(".");
    }
    else if (!routedByShared && (handlerName.find("handleAutonomy") == 0 || handlerName.find("handleTelemetry") == 0))
    {
        if (handlerName.find("Status") != std::string::npos || handlerName.find("Dashboard") != std::string::npos ||
            handlerName.find("Snapshot") != std::string::npos || handlerName.find("Export") != std::string::npos)
        {
            toolName = "load_rules";
        }
        else
        {
            toolName = "plan_tasks";
            if (!args.contains("goal"))
                args["goal"] = std::string("Autonomy/telemetry linker-gap fallback for ") + handlerName;
        }
    }
    else if (!routedByShared && handlerName.find("handleSubagent") == 0)
    {
        if (handlerName.find("Todo") != std::string::npos)
        {
            toolName = "plan_tasks";
            args["task"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Subagent TODO operation");
        }
        else
        {
            toolName = "plan_code_exploration";
            args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                                     : std::string("Subagent linker-gap fallback exploration");
        }
    }
    else if (!routedByShared && handlerName.find("handleRouter") == 0)
    {
        if (handlerName.find("Capabilities") != std::string::npos)
        {
            toolName = "load_rules";
        }
        else
        {
            toolName = "optimize_tool_selection";
            args["task"] =
                (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Router linker-gap fallback");
        }
    }
    else if (!routedByShared && (handlerName == "handleDecompFindRefs" || handlerName == "handleDecompGotoDef" ||
                                 handlerName == "handleDecompGotoAddr" || handlerName == "handleDecompRenameVar"))
    {
        toolName = "search_code";
        if (!args.contains("query"))
        {
            args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("decomp symbol lookup");
        }
    }
    else if (!routedByShared && (handlerName == "handleDecompCopyLine" || handlerName == "handleDecompCopyAll"))
    {
        toolName = "read_file";
        if (!args.contains("path"))
        {
            args["path"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string(".");
        }
    }
    else if (!routedByShared && handlerName == "handleHotpatchByteSearch")
    {
        toolName = "search_files";
        if (!args.contains("query"))
        {
            args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("hotpatch byte pattern");
        }
    }
    else if (!routedByShared && handlerName == "handleHotpatchProxyValidate")
    {
        toolName = "load_rules";
    }
    else if (!routedByShared && handlerName.find("handleAudit") == 0)
    {
        toolName = "load_rules";
    }
    else if (!routedByShared && handlerName.find("handleView") == 0)
    {
        toolName = "list_dir";
        args["path"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string(".");
    }
    else if (!routedByShared && handlerName.find("handleTools") == 0)
    {
        toolName = "run_shell";
        args["command"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("dir");
    }
    else
    {
        return false;
    }

    auto result = RawrXD::Agent::AgentToolHandlers::Instance().Execute(toolName, args);
    if (ctx.outputFn != nullptr)
    {
        const std::string msg = std::string("[SSOT link-gap] headless backend route -> ") + toolName + "\n";
        ctx.output(msg.c_str());
        const std::string payload = result.toJson().dump(2) + "\n";
        ctx.output(payload.c_str());
    }

    if (result.isSuccess())
    {
        RawrXD::Core::RecordFallbackRoute("linkerGap", handlerName, toolName, true, false);
        outResult = CommandResult::ok("headless_backend_route");
    }
    else
    {
        RawrXD::Core::RecordFallbackRoute("linkerGap", handlerName, toolName, false, false);
        outResult = CommandResult::error("headless_backend_route_failed");
    }
    return true;
}

CommandResult linkGapHandler(const CommandContext& ctx, const char* name)
{
    CommandResult routed = CommandResult::error("link_gap_no_backend_route");
    if (tryHeadlessAgentToolRoute(ctx, name, routed))
    {
        if (routed.success)
            return routed;
    }

    if (ctx.outputFn != nullptr)
    {
        const std::string msg = std::string("[SSOT link-gap] no backend route available for: ") + name + "\\n";
        ctx.output(msg.c_str());
    }
    RawrXD::Core::RecordFallbackRoute("linkerGap", name ? name : "", "no_backend_route", false, false);
    return CommandResult::error("link_gap_no_backend_route");
}
}  // namespace

CommandResult handleAutonomyMemory(const CommandContext& ctx)
{
    CommandResult routed = CommandResult::error("link_gap_no_backend_route");
    if (tryHeadlessAgentToolRoute(ctx, "handleAutonomyMemory", routed))
        return routed;
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Inspect autonomy memory state");
    return executeConcreteTool(ctx, "handleAutonomyMemory", "plan_tasks", args);
}

CommandResult handleAutonomyStatus(const CommandContext& ctx)
{
    CommandResult routed = CommandResult::error("link_gap_no_backend_route");
    if (tryHeadlessAgentToolRoute(ctx, "handleAutonomyStatus", routed))
        return routed;
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleAutonomyStatus", "load_rules", args);
}

CommandResult handleAuditCheckMenus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleAuditCheckMenus", "load_rules", args);
}
CommandResult handleAuditDetectStubs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleAuditDetectStubs", "load_rules", args);
}
CommandResult handleAuditExportReport(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Export audit report artifacts");
    return executeConcreteTool(ctx, "handleAuditExportReport", "plan_tasks", args);
}
CommandResult handleAuditQuickStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleAuditQuickStats", "load_rules", args);
}
CommandResult handleAuditRunFull(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Run full workspace audit");
    return executeConcreteTool(ctx, "handleAuditRunFull", "plan_tasks", args);
}
CommandResult handleAuditRunTests(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Run audit validation test suite");
    return executeConcreteTool(ctx, "handleAuditRunTests", "plan_tasks", args);
}
CommandResult handleDecompCopyAll(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("path"))
    {
        args["path"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string(".");
    }
    return executeConcreteTool(ctx, "handleDecompCopyAll", "read_file", args);
}
CommandResult handleDecompCopyLine(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("path"))
    {
        args["path"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string(".");
    }
    return executeConcreteTool(ctx, "handleDecompCopyLine", "read_file", args);
}
CommandResult handleDecompFindRefs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("decomp symbol references");
    }
    return executeConcreteTool(ctx, "handleDecompFindRefs", "search_code", args);
}
CommandResult handleDecompGotoAddr(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("decomp goto address");
    }
    return executeConcreteTool(ctx, "handleDecompGotoAddr", "search_code", args);
}
CommandResult handleDecompGotoDef(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("decomp symbol definition");
    }
    return executeConcreteTool(ctx, "handleDecompGotoDef", "search_code", args);
}
CommandResult handleDecompRenameVar(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                             : std::string("Rename decompiled variable and update references");
    return executeConcreteTool(ctx, "handleDecompRenameVar", "plan_tasks", args);
}
CommandResult handleEditCopyFormat(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("path"))
    {
        args["path"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string(".");
    }
    return executeConcreteTool(ctx, "handleEditCopyFormat", "read_file", args);
}
CommandResult handleEditFindNext(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("find next match in active editor");
    }
    return executeConcreteTool(ctx, "handleEditFindNext", "search_code", args);
}
CommandResult handleEditFindPrev(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("find previous match in active editor");
    }
    return executeConcreteTool(ctx, "handleEditFindPrev", "search_code", args);
}
CommandResult handleEditGotoLine(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                             : std::string("Move cursor to requested line in active editor");
    return executeConcreteTool(ctx, "handleEditGotoLine", "plan_tasks", args);
}
CommandResult handleEditMulticursorAdd(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Add multicursor selection in editor");
    return executeConcreteTool(ctx, "handleEditMulticursorAdd", "plan_tasks", args);
}
CommandResult handleEditMulticursorRemove(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Remove multicursor selection in editor");
    return executeConcreteTool(ctx, "handleEditMulticursorRemove", "plan_tasks", args);
}
CommandResult handleEditPastePlain(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Paste plain text in active editor");
    return executeConcreteTool(ctx, "handleEditPastePlain", "plan_tasks", args);
}
CommandResult handleEditSnippet(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Insert or expand editor snippet");
    return executeConcreteTool(ctx, "handleEditSnippet", "plan_tasks", args);
}
CommandResult handleFileCloseFolder(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Close current workspace folder");
    return executeConcreteTool(ctx, "handleFileCloseFolder", "plan_tasks", args);
}
CommandResult handleFileCloseTab(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Close current editor tab");
    return executeConcreteTool(ctx, "handleFileCloseTab", "plan_tasks", args);
}
CommandResult handleFileExit(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Exit IDE session safely");
    return executeConcreteTool(ctx, "handleFileExit", "plan_tasks", args);
}
CommandResult handleFileNewWindow(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Open a new IDE window");
    return executeConcreteTool(ctx, "handleFileNewWindow", "plan_tasks", args);
}
CommandResult handleFileOpenFolder(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("path"))
    {
        args["path"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string(".");
    }
    return executeConcreteTool(ctx, "handleFileOpenFolder", "list_dir", args);
}
CommandResult handleFileRecentClear(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Clear recent file and folder history");
    return executeConcreteTool(ctx, "handleFileRecentClear", "plan_tasks", args);
}
CommandResult handleGauntletExport(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Export gauntlet run artifacts");
    return executeConcreteTool(ctx, "handleGauntletExport", "plan_tasks", args);
}
CommandResult handleGauntletRun(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Run gauntlet validation suite");
    return executeConcreteTool(ctx, "handleGauntletRun", "plan_tasks", args);
}
CommandResult handleHelpSearch(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("help command search");
    }
    return executeConcreteTool(ctx, "handleHelpSearch", "search_code", args);
}
CommandResult handleHotpatchByteSearch(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("pattern"))
    {
        args["pattern"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("90 90");
    }
    return executeConcreteTool(ctx, "handleHotpatchByteSearch", "search_files", args);
}
CommandResult handleHotpatchPresetLoad(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleHotpatchPresetLoad", "load_rules", args);
}
CommandResult handleHotpatchPresetSave(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Save hotpatch preset configuration");
    return executeConcreteTool(ctx, "handleHotpatchPresetSave", "plan_tasks", args);
}
CommandResult handleHotpatchProxyBias(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Adjust hotpatch proxy bias policy");
    return executeConcreteTool(ctx, "handleHotpatchProxyBias", "plan_tasks", args);
}
CommandResult handleHotpatchProxyRewrite(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Rewrite hotpatch proxy routing rules");
    return executeConcreteTool(ctx, "handleHotpatchProxyRewrite", "plan_tasks", args);
}
CommandResult handleHotpatchProxyTerminate(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Terminate hotpatch proxy session");
    return executeConcreteTool(ctx, "handleHotpatchProxyTerminate", "plan_tasks", args);
}
CommandResult handleHotpatchProxyValidate(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleHotpatchProxyValidate", "load_rules", args);
}
CommandResult handleHotpatchResetStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Reset hotpatch proxy and runtime statistics");
    return executeConcreteTool(ctx, "handleHotpatchResetStats", "plan_tasks", args);
}
CommandResult handleHotpatchServerRemove(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Remove hotpatch backend server");
    return executeConcreteTool(ctx, "handleHotpatchServerRemove", "plan_tasks", args);
}
CommandResult handleHotpatchToggleAll(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle all hotpatch rules");
    return executeConcreteTool(ctx, "handleHotpatchToggleAll", "plan_tasks", args);
}
CommandResult handlePdbCacheClear(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Clear PDB symbol cache");
    return executeConcreteTool(ctx, "handlePdbCacheClear", "plan_tasks", args);
}
CommandResult handlePdbEnable(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Enable PDB symbol subsystem");
    return executeConcreteTool(ctx, "handlePdbEnable", "plan_tasks", args);
}
CommandResult handlePdbExports(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("PDB exports");
    }
    return executeConcreteTool(ctx, "handlePdbExports", "search_code", args);
}
CommandResult handlePdbFetch(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Fetch PDB symbols for target module");
    return executeConcreteTool(ctx, "handlePdbFetch", "plan_tasks", args);
}
CommandResult handlePdbIatStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handlePdbIatStatus", "load_rules", args);
}
CommandResult handlePdbImports(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("PDB imports");
    }
    return executeConcreteTool(ctx, "handlePdbImports", "search_code", args);
}
CommandResult handlePdbLoad(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Load selected PDB symbol set");
    return executeConcreteTool(ctx, "handlePdbLoad", "plan_tasks", args);
}
CommandResult handlePdbResolve(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("resolve symbol address via PDB");
    }
    return executeConcreteTool(ctx, "handlePdbResolve", "search_code", args);
}
CommandResult handlePdbStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handlePdbStatus", "load_rules", args);
}
CommandResult handleQwAlertResourceStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleQwAlertResourceStatus", "load_rules", args);
}
CommandResult handleQwBackupAutoToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle automatic quick-win backups");
    return executeConcreteTool(ctx, "handleQwBackupAutoToggle", "plan_tasks", args);
}
CommandResult handleQwBackupCreate(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Create quick-win backup snapshot");
    return executeConcreteTool(ctx, "handleQwBackupCreate", "plan_tasks", args);
}
CommandResult handleQwBackupList(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleQwBackupList", "list_dir", args);
}
CommandResult handleQwBackupPrune(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Prune old quick-win backups");
    return executeConcreteTool(ctx, "handleQwBackupPrune", "plan_tasks", args);
}
CommandResult handleQwBackupRestore(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Restore quick-win backup snapshot");
    return executeConcreteTool(ctx, "handleQwBackupRestore", "plan_tasks", args);
}
CommandResult handleQwShortcutEditor(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Open quick-win shortcut editor");
    return executeConcreteTool(ctx, "handleQwShortcutEditor", "plan_tasks", args);
}
CommandResult handleQwShortcutReset(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Reset quick-win shortcuts to defaults");
    return executeConcreteTool(ctx, "handleQwShortcutReset", "plan_tasks", args);
}
CommandResult handleQwSloDashboard(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleQwSloDashboard", "load_rules", args);
}
CommandResult handleSwarmBuildCmake(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Build swarm project with CMake");
    return executeConcreteTool(ctx, "handleSwarmBuildCmake", "plan_tasks", args);
}
CommandResult handleSwarmBuildSources(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Build swarm source workers");
    return executeConcreteTool(ctx, "handleSwarmBuildSources", "plan_tasks", args);
}
CommandResult handleSwarmCacheClear(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Clear swarm build and task cache");
    return executeConcreteTool(ctx, "handleSwarmCacheClear", "plan_tasks", args);
}
CommandResult handleSwarmCacheStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleSwarmCacheStatus", "load_rules", args);
}
CommandResult handleSwarmCancelBuild(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Cancel active swarm build");
    return executeConcreteTool(ctx, "handleSwarmCancelBuild", "plan_tasks", args);
}
CommandResult handleSwarmRemoveNode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Remove swarm worker node");
    return executeConcreteTool(ctx, "handleSwarmRemoveNode", "plan_tasks", args);
}
CommandResult handleSwarmResetStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Reset swarm orchestration statistics");
    return executeConcreteTool(ctx, "handleSwarmResetStats", "plan_tasks", args);
}
CommandResult handleSwarmStartBuild(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Start distributed swarm build");
    return executeConcreteTool(ctx, "handleSwarmStartBuild", "plan_tasks", args);
}
CommandResult handleSwarmStartHybrid(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Start hybrid swarm mode");
    return executeConcreteTool(ctx, "handleSwarmStartHybrid", "plan_tasks", args);
}
CommandResult handleSwarmStartLeader(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Start swarm leader node");
    return executeConcreteTool(ctx, "handleSwarmStartLeader", "plan_tasks", args);
}
CommandResult handleSwarmStartWorker(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Start swarm worker node");
    return executeConcreteTool(ctx, "handleSwarmStartWorker", "plan_tasks", args);
}
CommandResult handleSwarmWorkerConnect(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Connect swarm worker node");
    return executeConcreteTool(ctx, "handleSwarmWorkerConnect", "plan_tasks", args);
}
CommandResult handleSwarmWorkerDisconnect(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Disconnect swarm worker node");
    return executeConcreteTool(ctx, "handleSwarmWorkerDisconnect", "plan_tasks", args);
}
CommandResult handleSwarmWorkerStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleSwarmWorkerStatus", "load_rules", args);
}
CommandResult handleTelemetryClear(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Clear telemetry buffers and counters");
    return executeConcreteTool(ctx, "handleTelemetryClear", "plan_tasks", args);
}
CommandResult handleTelemetryExportCsv(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Export telemetry data to CSV");
    return executeConcreteTool(ctx, "handleTelemetryExportCsv", "plan_tasks", args);
}
CommandResult handleTelemetryExportJson(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Export telemetry data to JSON");
    return executeConcreteTool(ctx, "handleTelemetryExportJson", "plan_tasks", args);
}
CommandResult handleTelemetrySnapshot(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Capture a telemetry snapshot");
    return executeConcreteTool(ctx, "handleTelemetrySnapshot", "plan_tasks", args);
}
CommandResult handleTelemetryToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle telemetry collection");
    return executeConcreteTool(ctx, "handleTelemetryToggle", "plan_tasks", args);
}
CommandResult handleTerminalSplitCode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Split terminal and run selected code");
    return executeConcreteTool(ctx, "handleTerminalSplitCode", "plan_tasks", args);
}
CommandResult handleThemeAbyss(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Apply theme: Abyss");
    return executeConcreteTool(ctx, "handleThemeAbyss", "plan_tasks", args);
}
CommandResult handleThemeDracula(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Apply theme: Dracula");
    return executeConcreteTool(ctx, "handleThemeDracula", "plan_tasks", args);
}
CommandResult handleThemeHighContrast(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Apply theme: High Contrast");
    return executeConcreteTool(ctx, "handleThemeHighContrast", "plan_tasks", args);
}
CommandResult handleThemeLightPlus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Apply theme: Light Plus");
    return executeConcreteTool(ctx, "handleThemeLightPlus", "plan_tasks", args);
}
CommandResult handleThemeMonokai(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Apply theme: Monokai");
    return executeConcreteTool(ctx, "handleThemeMonokai", "plan_tasks", args);
}
CommandResult handleThemeNord(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Apply theme: Nord");
    return executeConcreteTool(ctx, "handleThemeNord", "plan_tasks", args);
}
CommandResult handleToolsBuild(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["command"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("cmake --build build");
    return executeConcreteTool(ctx, "handleToolsBuild", "run_shell", args);
}
CommandResult handleToolsCommandPalette(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Open tools command palette");
    return executeConcreteTool(ctx, "handleToolsCommandPalette", "plan_tasks", args);
}
CommandResult handleToolsDebug(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Open debug tools");
    return executeConcreteTool(ctx, "handleToolsDebug", "plan_tasks", args);
}
CommandResult handleToolsExtensions(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Open extensions tools panel");
    return executeConcreteTool(ctx, "handleToolsExtensions", "plan_tasks", args);
}
CommandResult handleToolsSettings(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleToolsSettings", "load_rules", args);
}
CommandResult handleToolsTerminal(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["command"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("dir");
    return executeConcreteTool(ctx, "handleToolsTerminal", "run_shell", args);
}
CommandResult handleViewFloatingPanel(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle floating panel view");
    return executeConcreteTool(ctx, "handleViewFloatingPanel", "plan_tasks", args);
}
CommandResult handleViewMinimap(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle editor minimap view");
    return executeConcreteTool(ctx, "handleViewMinimap", "plan_tasks", args);
}
CommandResult handleViewModuleBrowser(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Open module browser view");
    return executeConcreteTool(ctx, "handleViewModuleBrowser", "plan_tasks", args);
}
CommandResult handleViewOutputPanel(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Show output panel");
    return executeConcreteTool(ctx, "handleViewOutputPanel", "plan_tasks", args);
}
CommandResult handleViewOutputTabs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Switch output tabs view");
    return executeConcreteTool(ctx, "handleViewOutputTabs", "plan_tasks", args);
}
CommandResult handleViewSidebar(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle sidebar visibility");
    return executeConcreteTool(ctx, "handleViewSidebar", "plan_tasks", args);
}
CommandResult handleViewTerminal(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle terminal panel visibility");
    return executeConcreteTool(ctx, "handleViewTerminal", "plan_tasks", args);
}
CommandResult handleViewThemeEditor(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Open theme editor");
    return executeConcreteTool(ctx, "handleViewThemeEditor", "plan_tasks", args);
}
CommandResult handleViewToggleFullscreen(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle fullscreen mode");
    return executeConcreteTool(ctx, "handleViewToggleFullscreen", "plan_tasks", args);
}
CommandResult handleViewToggleOutput(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle output panel");
    return executeConcreteTool(ctx, "handleViewToggleOutput", "plan_tasks", args);
}
CommandResult handleViewToggleSidebar(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle sidebar");
    return executeConcreteTool(ctx, "handleViewToggleSidebar", "plan_tasks", args);
}
CommandResult handleViewToggleTerminal(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle terminal");
    return executeConcreteTool(ctx, "handleViewToggleTerminal", "plan_tasks", args);
}
CommandResult handleViewZoomIn(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Increase UI zoom level");
    return executeConcreteTool(ctx, "handleViewZoomIn", "plan_tasks", args);
}
CommandResult handleViewZoomOut(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Decrease UI zoom level");
    return executeConcreteTool(ctx, "handleViewZoomOut", "plan_tasks", args);
}
CommandResult handleViewZoomReset(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Reset UI zoom level");
    return executeConcreteTool(ctx, "handleViewZoomReset", "plan_tasks", args);
}
CommandResult handleVoiceAutoNextVoice(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Select next voice profile");
    return executeConcreteTool(ctx, "handleVoiceAutoNextVoice", "plan_tasks", args);
}
CommandResult handleVoiceAutoPrevVoice(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Select previous voice profile");
    return executeConcreteTool(ctx, "handleVoiceAutoPrevVoice", "plan_tasks", args);
}
CommandResult handleVoiceAutoRateDown(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Decrease voice rate");
    return executeConcreteTool(ctx, "handleVoiceAutoRateDown", "plan_tasks", args);
}
CommandResult handleVoiceAutoRateUp(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Increase voice rate");
    return executeConcreteTool(ctx, "handleVoiceAutoRateUp", "plan_tasks", args);
}
CommandResult handleVoiceAutoSettings(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleVoiceAutoSettings", "load_rules", args);
}
CommandResult handleVoiceAutoStop(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Stop active voice session");
    return executeConcreteTool(ctx, "handleVoiceAutoStop", "plan_tasks", args);
}
CommandResult handleVoiceAutoToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Toggle automatic voice mode");
    return executeConcreteTool(ctx, "handleVoiceAutoToggle", "plan_tasks", args);
}
CommandResult handleVoiceJoinRoom(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Join voice collaboration room");
    return executeConcreteTool(ctx, "handleVoiceJoinRoom", "plan_tasks", args);
}
CommandResult handleVoiceModeContinuous(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Set voice mode to continuous");
    return executeConcreteTool(ctx, "handleVoiceModeContinuous", "plan_tasks", args);
}
CommandResult handleVoiceModeDisabled(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Disable voice mode");
    return executeConcreteTool(ctx, "handleVoiceModeDisabled", "plan_tasks", args);
}
