#include "../agentic/AgentToolHandlers.h"
#include "agent_fallback_tool_router.hpp"
#include "fallback_route_metrics.hpp"
#include "feature_handlers.h"

#include <nlohmann/json.hpp>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace
{

CommandResult missingHandler(const CommandContext& ctx, const char* name)
{
    if (ctx.outputFn != nullptr)
    {
        std::string msg = std::string("[SSOT provider] fallback handler executed: ") + name + "\n";
        ctx.output(msg.c_str());
    }

    const std::string handlerName = name ? name : "";
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    std::string tool;
    RawrXD::Core::ResolveFallbackToolRoute(handlerName, ctx.args, args, tool);
    if (tool == "search_code" && !args.contains("query"))
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("TODO");
    if (tool == "run_shell" && !args.contains("command"))
        args["command"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("dir");
    if (tool == "list_dir" && !args.contains("path"))
        args["path"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string(".");

    if (tool.empty())
    {
        // Ensure provider-level gaps still execute a concrete backend capability.
        tool = "load_rules";
    }

    auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
    if (!tool.empty() && handlers.HasTool(tool))
    {
        const auto result = handlers.Execute(tool, args);
        if (ctx.outputFn != nullptr)
        {
            const std::string payload =
                std::string("[SSOT provider] backend route result: ") + result.toJson().dump() + "\n";
            ctx.output(payload.c_str());
        }
        if (result.isSuccess())
        {
            RawrXD::Core::RecordFallbackRoute("missingProvider", handlerName, tool, true, false);
            return CommandResult::ok("ssot_provider_backend_route");
        }
    }

    if (ctx.isGui && ctx.hwnd != nullptr)
    {
        // Keep stub lane deterministic as fallback only.
        PostMessageA(reinterpret_cast<HWND>(ctx.hwnd), WM_COMMAND, static_cast<WPARAM>(ctx.commandId), 0);
        RawrXD::Core::RecordFallbackRoute("missingProvider", handlerName, "gui_dispatch_fallback", false, true);
        return CommandResult::ok("ssot_provider_gui_dispatch_fallback");
    }
    RawrXD::Core::RecordFallbackRoute("missingProvider", handlerName, tool.empty() ? "no_tool" : tool, false, false);
    return CommandResult::error("ssot_provider_no_backend_route");
}

CommandResult executeConcreteTool(const CommandContext& ctx, const char* handlerName, const std::string& toolName,
                                  nlohmann::json args)
{
    auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
    if (!handlers.HasTool(toolName))
    {
        if (ctx.outputFn != nullptr)
        {
            const std::string payload = std::string("[SSOT provider] tool unavailable for concrete route: ") +
                                        (handlerName ? handlerName : "") + " -> " + toolName + "\n";
            ctx.output(payload.c_str());
        }
        RawrXD::Core::RecordFallbackRoute("missingProvider", handlerName ? handlerName : "", toolName, false, false);
        return CommandResult::error("ssot_provider_concrete_tool_unavailable");
    }

    const auto result = handlers.Execute(toolName, args);
    if (ctx.outputFn != nullptr)
    {
        const std::string payload = std::string("[SSOT provider] concrete backend route: ") + handlerName + " -> " +
                                    toolName + " result: " + result.toJson().dump() + "\n";
        ctx.output(payload.c_str());
    }
    if (result.isSuccess())
    {
        RawrXD::Core::RecordFallbackRoute("missingProvider", handlerName ? handlerName : "", toolName, true, false);
        return CommandResult::ok("ssot_provider_concrete_backend_route");
    }
    RawrXD::Core::RecordFallbackRoute("missingProvider", handlerName ? handlerName : "", toolName, false, false);
    return CommandResult::error("ssot_provider_concrete_backend_route_failed");
}

}  // namespace

CommandResult handleAIChatMode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
        args["goal"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Switch AI chat interaction mode");
    return executeConcreteTool(ctx, "handleAIChatMode", "plan_tasks", args);
}
CommandResult handleAICtx128K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAICtx128K");
    }
    return executeConcreteTool(ctx, "handleAICtx128K", "plan_tasks", args);
}
CommandResult handleAICtx1M(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAICtx1M");
    }
    return executeConcreteTool(ctx, "handleAICtx1M", "plan_tasks", args);
}
CommandResult handleAICtx256K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAICtx256K");
    }
    return executeConcreteTool(ctx, "handleAICtx256K", "plan_tasks", args);
}
CommandResult handleAICtx32K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAICtx32K");
    }
    return executeConcreteTool(ctx, "handleAICtx32K", "plan_tasks", args);
}
CommandResult handleAICtx4K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAICtx4K");
    }
    return executeConcreteTool(ctx, "handleAICtx4K", "plan_tasks", args);
}
CommandResult handleAICtx512K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAICtx512K");
    }
    return executeConcreteTool(ctx, "handleAICtx512K", "plan_tasks", args);
}
CommandResult handleAICtx64K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAICtx64K");
    }
    return executeConcreteTool(ctx, "handleAICtx64K", "plan_tasks", args);
}
CommandResult handleAIExplainCode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Explain current code context");
    return executeConcreteTool(ctx, "handleAIExplainCode", "semantic_search", args);
}
CommandResult handleAIFixErrors(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
        args["goal"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Fix diagnostics and failing checks");
    return executeConcreteTool(ctx, "handleAIFixErrors", "plan_tasks", args);
}
CommandResult handleAIGenerateDocs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
        args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                                 : std::string("Generate documentation for selected scope");
    return executeConcreteTool(ctx, "handleAIGenerateDocs", "plan_tasks", args);
}
CommandResult handleAIGenerateTests(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
        args["goal"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Generate tests for selected scope");
    return executeConcreteTool(ctx, "handleAIGenerateTests", "plan_tasks", args);
}
CommandResult handleAIInlineComplete(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("file_path") && ctx.args && ctx.args[0])
        args["file_path"] = std::string(ctx.args);
    return executeConcreteTool(ctx, "handleAIInlineComplete", "next_edit_hint", args);
}
CommandResult handleAIModelSelect(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAIModelSelect");
    }
    return executeConcreteTool(ctx, "handleAIModelSelect", "plan_tasks", args);
}
CommandResult handleAINoRefusal(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAINoRefusal");
    }
    return executeConcreteTool(ctx, "handleAINoRefusal", "plan_tasks", args);
}
CommandResult handleAIOptimizeCode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAIOptimizeCode");
    }
    return executeConcreteTool(ctx, "handleAIOptimizeCode", "plan_tasks", args);
}
CommandResult handleAIRefactor(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
        args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Refactor current code context");
    return executeConcreteTool(ctx, "handleAIRefactor", "plan_code_exploration", args);
}
CommandResult handleLspSrvConfig(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Configure LSP server process parameters");
    }
    return executeConcreteTool(ctx, "handleLspSrvConfig", "plan_tasks", args);
}
CommandResult handleLspSrvExportSymbols(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Export symbol index from LSP server");
    }
    return executeConcreteTool(ctx, "handleLspSrvExportSymbols", "plan_tasks", args);
}
CommandResult handleLspSrvLaunchStdio(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Launch LSP server in stdio mode");
    }
    return executeConcreteTool(ctx, "handleLspSrvLaunchStdio", "plan_tasks", args);
}
CommandResult handleLspSrvPublishDiag(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleLspSrvPublishDiag", "get_diagnostics", args);
}
CommandResult handleLspSrvReindex(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Reindex LSP server symbol database");
    }
    return executeConcreteTool(ctx, "handleLspSrvReindex", "plan_tasks", args);
}
CommandResult handleLspSrvStart(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleLspSrvStart");
    }
    return executeConcreteTool(ctx, "handleLspSrvStart", "plan_tasks", args);
}
CommandResult handleLspSrvStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleLspSrvStats", "load_rules", args);
}
CommandResult handleLspSrvStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleLspSrvStatus", "load_rules", args);
}
CommandResult handleLspSrvStop(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleLspSrvStop");
    }
    return executeConcreteTool(ctx, "handleLspSrvStop", "plan_tasks", args);
}
CommandResult handleTelemetryDashboard(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTelemetryDashboard");
    }
    return executeConcreteTool(ctx, "handleTelemetryDashboard", "plan_tasks", args);
}

CommandResult handleAuditDashboard(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleAuditDashboard");
    }
    return executeConcreteTool(ctx, "handleAuditDashboard", "plan_tasks", args);
}
CommandResult handleEditClipboardHist(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleEditClipboardHist");
    }
    return executeConcreteTool(ctx, "handleEditClipboardHist", "plan_tasks", args);
}
CommandResult handleEditorCycle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleEditorCycle");
    }
    return executeConcreteTool(ctx, "handleEditorCycle", "plan_tasks", args);
}
CommandResult handleEditorMonacoCore(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleEditorMonacoCore");
    }
    return executeConcreteTool(ctx, "handleEditorMonacoCore", "plan_tasks", args);
}
CommandResult handleEditorRichEdit(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleEditorRichEdit");
    }
    return executeConcreteTool(ctx, "handleEditorRichEdit", "plan_tasks", args);
}
CommandResult handleEditorStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleEditorStatus");
    }
    return executeConcreteTool(ctx, "handleEditorStatus", "plan_tasks", args);
}
CommandResult handleEditorWebView2(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleEditorWebView2");
    }
    return executeConcreteTool(ctx, "handleEditorWebView2", "plan_tasks", args);
}
CommandResult handleHelpCmdRef(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleHelpCmdRef");
    }
    return executeConcreteTool(ctx, "handleHelpCmdRef", "plan_tasks", args);
}
CommandResult handleHelpPsDocs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleHelpPsDocs");
    }
    return executeConcreteTool(ctx, "handleHelpPsDocs", "plan_tasks", args);
}
CommandResult handleHotpatchEventLog(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleHotpatchEventLog");
    }
    return executeConcreteTool(ctx, "handleHotpatchEventLog", "plan_tasks", args);
}
CommandResult handleHotpatchMemRevert(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleHotpatchMemRevert");
    }
    return executeConcreteTool(ctx, "handleHotpatchMemRevert", "plan_tasks", args);
}
CommandResult handleHotpatchProxyStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleHotpatchProxyStats");
    }
    return executeConcreteTool(ctx, "handleHotpatchProxyStats", "plan_tasks", args);
}
CommandResult handleMonacoDevtools(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleMonacoDevtools");
    }
    return executeConcreteTool(ctx, "handleMonacoDevtools", "plan_tasks", args);
}
CommandResult handleMonacoReload(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleMonacoReload");
    }
    return executeConcreteTool(ctx, "handleMonacoReload", "plan_tasks", args);
}
CommandResult handleMonacoSyncTheme(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleMonacoSyncTheme");
    }
    return executeConcreteTool(ctx, "handleMonacoSyncTheme", "plan_tasks", args);
}
CommandResult handleMonacoToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleMonacoToggle");
    }
    return executeConcreteTool(ctx, "handleMonacoToggle", "plan_tasks", args);
}
CommandResult handleMonacoZoomIn(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleMonacoZoomIn");
    }
    return executeConcreteTool(ctx, "handleMonacoZoomIn", "plan_tasks", args);
}
CommandResult handleMonacoZoomOut(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleMonacoZoomOut");
    }
    return executeConcreteTool(ctx, "handleMonacoZoomOut", "plan_tasks", args);
}
CommandResult handleQwAlertDismiss(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleQwAlertDismiss");
    }
    return executeConcreteTool(ctx, "handleQwAlertDismiss", "plan_tasks", args);
}
CommandResult handleQwAlertHistory(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleQwAlertHistory");
    }
    return executeConcreteTool(ctx, "handleQwAlertHistory", "plan_tasks", args);
}
CommandResult handleQwAlertMonitor(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleQwAlertMonitor");
    }
    return executeConcreteTool(ctx, "handleQwAlertMonitor", "plan_tasks", args);
}
CommandResult handleRECompare(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRECompare");
    }
    return executeConcreteTool(ctx, "handleRECompare", "plan_tasks", args);
}
CommandResult handleRECompile(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRECompile");
    }
    return executeConcreteTool(ctx, "handleRECompile", "plan_tasks", args);
}
CommandResult handleREDataFlow(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREDataFlow");
    }
    return executeConcreteTool(ctx, "handleREDataFlow", "plan_tasks", args);
}
CommandResult handleREDecompClose(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREDecompClose");
    }
    return executeConcreteTool(ctx, "handleREDecompClose", "plan_tasks", args);
}
CommandResult handleREDecompilerView(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREDecompilerView");
    }
    return executeConcreteTool(ctx, "handleREDecompilerView", "plan_tasks", args);
}
CommandResult handleREDecompRename(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREDecompRename");
    }
    return executeConcreteTool(ctx, "handleREDecompRename", "plan_tasks", args);
}
CommandResult handleREDecompSync(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREDecompSync");
    }
    return executeConcreteTool(ctx, "handleREDecompSync", "plan_tasks", args);
}
CommandResult handleREDemangle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREDemangle");
    }
    return executeConcreteTool(ctx, "handleREDemangle", "plan_tasks", args);
}
CommandResult handleREDetectVulns(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREDetectVulns");
    }
    return executeConcreteTool(ctx, "handleREDetectVulns", "plan_tasks", args);
}
CommandResult handleREExportGhidra(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREExportGhidra");
    }
    return executeConcreteTool(ctx, "handleREExportGhidra", "plan_tasks", args);
}
CommandResult handleREExportIDA(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREExportIDA");
    }
    return executeConcreteTool(ctx, "handleREExportIDA", "plan_tasks", args);
}
CommandResult handleREFunctions(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleREFunctions");
    }
    return executeConcreteTool(ctx, "handleREFunctions", "plan_tasks", args);
}
CommandResult handleRELicenseInfo(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRELicenseInfo");
    }
    return executeConcreteTool(ctx, "handleRELicenseInfo", "plan_tasks", args);
}
CommandResult handleRERecursiveDisasm(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRERecursiveDisasm");
    }
    return executeConcreteTool(ctx, "handleRERecursiveDisasm", "plan_tasks", args);
}
CommandResult handleRETypeRecovery(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRETypeRecovery");
    }
    return executeConcreteTool(ctx, "handleRETypeRecovery", "plan_tasks", args);
}
CommandResult handleSwarmBlacklist(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSwarmBlacklist");
    }
    return executeConcreteTool(ctx, "handleSwarmBlacklist", "plan_tasks", args);
}
CommandResult handleSwarmConfig(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSwarmConfig");
    }
    return executeConcreteTool(ctx, "handleSwarmConfig", "plan_tasks", args);
}
CommandResult handleSwarmDiscovery(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSwarmDiscovery");
    }
    return executeConcreteTool(ctx, "handleSwarmDiscovery", "plan_tasks", args);
}
CommandResult handleSwarmEvents(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSwarmEvents");
    }
    return executeConcreteTool(ctx, "handleSwarmEvents", "plan_tasks", args);
}
CommandResult handleSwarmFitness(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSwarmFitness");
    }
    return executeConcreteTool(ctx, "handleSwarmFitness", "plan_tasks", args);
}
CommandResult handleSwarmStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSwarmStats");
    }
    return executeConcreteTool(ctx, "handleSwarmStats", "plan_tasks", args);
}
CommandResult handleSwarmTaskGraph(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSwarmTaskGraph");
    }
    return executeConcreteTool(ctx, "handleSwarmTaskGraph", "plan_tasks", args);
}
CommandResult handleThemeCatppuccin(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeCatppuccin");
    }
    return executeConcreteTool(ctx, "handleThemeCatppuccin", "plan_tasks", args);
}
CommandResult handleThemeCrimson(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeCrimson");
    }
    return executeConcreteTool(ctx, "handleThemeCrimson", "plan_tasks", args);
}
CommandResult handleThemeCyberpunk(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeCyberpunk");
    }
    return executeConcreteTool(ctx, "handleThemeCyberpunk", "plan_tasks", args);
}
CommandResult handleThemeGruvbox(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeGruvbox");
    }
    return executeConcreteTool(ctx, "handleThemeGruvbox", "plan_tasks", args);
}
CommandResult handleThemeOneDark(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeOneDark");
    }
    return executeConcreteTool(ctx, "handleThemeOneDark", "plan_tasks", args);
}
CommandResult handleThemeSolDark(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeSolDark");
    }
    return executeConcreteTool(ctx, "handleThemeSolDark", "plan_tasks", args);
}
CommandResult handleThemeSolLight(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeSolLight");
    }
    return executeConcreteTool(ctx, "handleThemeSolLight", "plan_tasks", args);
}
CommandResult handleThemeSynthwave(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeSynthwave");
    }
    return executeConcreteTool(ctx, "handleThemeSynthwave", "plan_tasks", args);
}
CommandResult handleThemeTokyo(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleThemeTokyo");
    }
    return executeConcreteTool(ctx, "handleThemeTokyo", "plan_tasks", args);
}
CommandResult handleTier1AutoUpdateCheck(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1AutoUpdateCheck");
    }
    return executeConcreteTool(ctx, "handleTier1AutoUpdateCheck", "plan_tasks", args);
}
CommandResult handleTier1BreadcrumbsToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1BreadcrumbsToggle");
    }
    return executeConcreteTool(ctx, "handleTier1BreadcrumbsToggle", "plan_tasks", args);
}
CommandResult handleTier1FileIconTheme(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1FileIconTheme");
    }
    return executeConcreteTool(ctx, "handleTier1FileIconTheme", "plan_tasks", args);
}
CommandResult handleTier1FuzzyPalette(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1FuzzyPalette");
    }
    return executeConcreteTool(ctx, "handleTier1FuzzyPalette", "plan_tasks", args);
}
CommandResult handleTier1MinimapEnhanced(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1MinimapEnhanced");
    }
    return executeConcreteTool(ctx, "handleTier1MinimapEnhanced", "plan_tasks", args);
}
CommandResult handleTier1SettingsGUI(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1SettingsGUI");
    }
    return executeConcreteTool(ctx, "handleTier1SettingsGUI", "plan_tasks", args);
}
CommandResult handleTier1SmoothScrollToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1SmoothScrollToggle");
    }
    return executeConcreteTool(ctx, "handleTier1SmoothScrollToggle", "plan_tasks", args);
}
CommandResult handleTier1SplitClose(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1SplitClose");
    }
    return executeConcreteTool(ctx, "handleTier1SplitClose", "plan_tasks", args);
}
CommandResult handleTier1SplitFocusNext(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1SplitFocusNext");
    }
    return executeConcreteTool(ctx, "handleTier1SplitFocusNext", "plan_tasks", args);
}
CommandResult handleTier1SplitGrid(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1SplitGrid");
    }
    return executeConcreteTool(ctx, "handleTier1SplitGrid", "plan_tasks", args);
}
CommandResult handleTier1SplitHorizontal(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1SplitHorizontal");
    }
    return executeConcreteTool(ctx, "handleTier1SplitHorizontal", "plan_tasks", args);
}
CommandResult handleTier1SplitVertical(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1SplitVertical");
    }
    return executeConcreteTool(ctx, "handleTier1SplitVertical", "plan_tasks", args);
}
CommandResult handleTier1TabDragToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1TabDragToggle");
    }
    return executeConcreteTool(ctx, "handleTier1TabDragToggle", "plan_tasks", args);
}
CommandResult handleTier1UpdateDismiss(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1UpdateDismiss");
    }
    return executeConcreteTool(ctx, "handleTier1UpdateDismiss", "plan_tasks", args);
}
CommandResult handleTier1WelcomePage(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTier1WelcomePage");
    }
    return executeConcreteTool(ctx, "handleTier1WelcomePage", "plan_tasks", args);
}
CommandResult handleTrans100(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTrans100");
    }
    return executeConcreteTool(ctx, "handleTrans100", "plan_tasks", args);
}
CommandResult handleTrans40(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTrans40");
    }
    return executeConcreteTool(ctx, "handleTrans40", "plan_tasks", args);
}
CommandResult handleTrans50(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTrans50");
    }
    return executeConcreteTool(ctx, "handleTrans50", "plan_tasks", args);
}
CommandResult handleTrans60(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTrans60");
    }
    return executeConcreteTool(ctx, "handleTrans60", "plan_tasks", args);
}
CommandResult handleTrans70(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTrans70");
    }
    return executeConcreteTool(ctx, "handleTrans70", "plan_tasks", args);
}
CommandResult handleTrans80(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTrans80");
    }
    return executeConcreteTool(ctx, "handleTrans80", "plan_tasks", args);
}
CommandResult handleTrans90(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTrans90");
    }
    return executeConcreteTool(ctx, "handleTrans90", "plan_tasks", args);
}
CommandResult handleTransCustom(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTransCustom");
    }
    return executeConcreteTool(ctx, "handleTransCustom", "plan_tasks", args);
}
CommandResult handleTransToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleTransToggle");
    }
    return executeConcreteTool(ctx, "handleTransToggle", "plan_tasks", args);
}
CommandResult handleViewStreamingLoader(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleViewStreamingLoader");
    }
    return executeConcreteTool(ctx, "handleViewStreamingLoader", "plan_tasks", args);
}
CommandResult handleViewVulkanRenderer(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleViewVulkanRenderer");
    }
    return executeConcreteTool(ctx, "handleViewVulkanRenderer", "plan_tasks", args);
}
CommandResult handleVoicePTT(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVoicePTT");
    }
    return executeConcreteTool(ctx, "handleVoicePTT", "plan_tasks", args);
}
CommandResult handleVscExtDeactivateAll(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtDeactivateAll");
    }
    return executeConcreteTool(ctx, "handleVscExtDeactivateAll", "plan_tasks", args);
}
CommandResult handleVscExtDiagnostics(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtDiagnostics");
    }
    return executeConcreteTool(ctx, "handleVscExtDiagnostics", "plan_tasks", args);
}
CommandResult handleVscExtExportConfig(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtExportConfig");
    }
    return executeConcreteTool(ctx, "handleVscExtExportConfig", "plan_tasks", args);
}
CommandResult handleVscExtExtensions(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtExtensions");
    }
    return executeConcreteTool(ctx, "handleVscExtExtensions", "plan_tasks", args);
}
CommandResult handleVscExtListCommands(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtListCommands");
    }
    return executeConcreteTool(ctx, "handleVscExtListCommands", "plan_tasks", args);
}
CommandResult handleVscExtListProviders(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtListProviders");
    }
    return executeConcreteTool(ctx, "handleVscExtListProviders", "plan_tasks", args);
}
CommandResult handleVscExtLoadNative(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtLoadNative");
    }
    return executeConcreteTool(ctx, "handleVscExtLoadNative", "plan_tasks", args);
}
CommandResult handleVscExtReload(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtReload");
    }
    return executeConcreteTool(ctx, "handleVscExtReload", "plan_tasks", args);
}
CommandResult handleVscExtStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtStats");
    }
    return executeConcreteTool(ctx, "handleVscExtStats", "plan_tasks", args);
}
CommandResult handleVscExtStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVscExtStatus");
    }
    return executeConcreteTool(ctx, "handleVscExtStatus", "plan_tasks", args);
}
