#include "../agentic/AgentToolHandlers.h"
#include "agent_fallback_tool_router.hpp"
#include "fallback_route_metrics.hpp"
#include "feature_handlers.h"


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <atomic>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <string>
#include <windows.h>


namespace
{

CommandResult missingHandler(const CommandContext& ctx, const char* name)
{
    if (ctx.outputFn != nullptr)
    {
        std::string msg = std::string("[AUTO SSOT] fallback handler executed: ") + name + "\n";
        ctx.output(msg.c_str());
    }

    const std::string handlerName = name ? name : "";
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    std::string tool;
    RawrXD::Core::ResolveFallbackToolRoute(handlerName, ctx.args, args, tool);
    if (tool.empty() && args.contains("tool") && args["tool"].is_string())
    {
        tool = args["tool"].get<std::string>();
    }
    if (tool.empty())
    {
        tool = "load_rules";
    }

    auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
    if (!tool.empty() && handlers.HasTool(tool))
    {
        const auto result = handlers.Execute(tool, args);
        if (ctx.outputFn != nullptr)
        {
            const std::string payload =
                std::string("[AUTO SSOT] backend route result: ") + result.toJson().dump() + "\n";
            ctx.output(payload.c_str());
        }
        if (result.isSuccess())
        {
            RawrXD::Core::RecordFallbackRoute("autoMissing", handlerName, tool, true, false);
            return CommandResult::ok("auto_ssot_backend_route");
        }
    }

    if (ctx.isGui && ctx.hwnd != nullptr)
    {
        PostMessageA(reinterpret_cast<HWND>(ctx.hwnd), WM_COMMAND, static_cast<WPARAM>(ctx.commandId), 0);
        RawrXD::Core::RecordFallbackRoute("autoMissing", handlerName, "gui_dispatch_fallback", false, true);
        return CommandResult::ok("auto_ssot_gui_dispatch_fallback");
    }
    RawrXD::Core::RecordFallbackRoute("autoMissing", handlerName, tool.empty() ? "no_tool" : tool, false, false);
    return CommandResult::error("auto_ssot_no_backend_route");
}

CommandResult executeConcreteTool(const CommandContext& ctx, const char* handlerName, const std::string& toolName,
                                  nlohmann::json args, const std::string& missingArgMessage = "")
{
    auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
    if (!handlers.HasTool(toolName))
    {
        if (ctx.outputFn != nullptr)
        {
            const std::string payload = std::string("[AUTO SSOT] tool unavailable for concrete route: ") + handlerName +
                                        " -> " + toolName + "\n";
            ctx.output(payload.c_str());
        }
        RawrXD::Core::RecordFallbackRoute("autoMissing", handlerName ? handlerName : "", toolName, false, false);
        return CommandResult::error("auto_ssot_concrete_tool_unavailable");
    }

    if (!missingArgMessage.empty() && (!ctx.args || !ctx.args[0]))
    {
        if (ctx.outputFn != nullptr)
        {
            ctx.output(missingArgMessage.c_str());
        }
        return CommandResult::error("missing_required_argument");
    }

    const auto result = handlers.Execute(toolName, args);
    if (ctx.outputFn != nullptr)
    {
        const std::string payload = std::string("[AUTO SSOT] concrete backend route: ") + handlerName + " -> " +
                                    toolName + " result: " + result.toJson().dump() + "\n";
        ctx.output(payload.c_str());
    }
    if (result.isSuccess())
    {
        RawrXD::Core::RecordFallbackRoute("autoMissing", handlerName ? handlerName : "", toolName, true, false);
        return CommandResult::ok("auto_ssot_concrete_backend_route");
    }
    RawrXD::Core::RecordFallbackRoute("autoMissing", handlerName ? handlerName : "", toolName, false, false);
    return CommandResult::error("auto_ssot_concrete_backend_route_failed");
}

std::atomic<unsigned int> g_auto_ssot_beacon_state{0};
std::atomic<unsigned int> g_auto_ssot_beacon_full{0};

std::string firstBeaconArg(const CommandContext& ctx)
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

void updateAutoSsotBeaconState(bool highPulse)
{
    if (highPulse)
    {
        g_auto_ssot_beacon_state.fetch_or(0x2u, std::memory_order_relaxed);
    }
    else
    {
        g_auto_ssot_beacon_state.fetch_or(0x1u, std::memory_order_relaxed);
    }
    const unsigned int st = g_auto_ssot_beacon_state.load(std::memory_order_relaxed);
    if ((st & 0x3u) == 0x3u)
    {
        g_auto_ssot_beacon_full.store(1u, std::memory_order_relaxed);
    }
}

}  // namespace

CommandResult handleRouterCapabilities(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Inspect available routing capabilities and policies";
    return executeConcreteTool(ctx, "handleRouterCapabilities", "plan_code_exploration", args);
}
CommandResult handleRouterDecision(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Explain current backend routing decision";
    return executeConcreteTool(ctx, "handleRouterDecision", "plan_tasks", args);
}
CommandResult handleRouterDisable(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Disable router and pin active backend";
    return executeConcreteTool(ctx, "handleRouterDisable", "plan_tasks", args);
}
CommandResult handleRouterEnable(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Enable router and resume policy based backend selection";
    return executeConcreteTool(ctx, "handleRouterEnable", "plan_tasks", args);
}
CommandResult handleRouterEnsembleDisable(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Disable ensemble routing for model selection";
    return executeConcreteTool(ctx, "handleRouterEnsembleDisable", "plan_tasks", args);
}
CommandResult handleRouterEnsembleEnable(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Enable ensemble routing for model selection";
    return executeConcreteTool(ctx, "handleRouterEnsembleEnable", "plan_tasks", args);
}
CommandResult handleRouterEnsembleStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleRouterEnsembleStatus", "load_rules", args);
}
CommandResult handleRouterFallbacks(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Inspect configured router fallback order and conditions";
    return executeConcreteTool(ctx, "handleRouterFallbacks", "plan_code_exploration", args);
}
CommandResult handleRouterPinTask(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Pin task family to preferred backend";
    return executeConcreteTool(ctx, "handleRouterPinTask", "plan_tasks", args);
}
CommandResult handleRouterResetStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Reset router counters and latency statistics";
    return executeConcreteTool(ctx, "handleRouterResetStats", "plan_tasks", args);
}
CommandResult handleRouterRoutePrompt(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("prompt"))
    {
        args["prompt"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Route this prompt");
    }
    return executeConcreteTool(ctx, "handleRouterRoutePrompt", "optimize_tool_selection", args);
}
CommandResult handleRouterSaveConfig(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Persist current router configuration";
    return executeConcreteTool(ctx, "handleRouterSaveConfig", "plan_tasks", args);
}
CommandResult handleRouterSetPolicy(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set routing policy and constraints";
    return executeConcreteTool(ctx, "handleRouterSetPolicy", "plan_tasks", args);
}
CommandResult handleRouterShowCostStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Show router cost and token usage statistics";
    return executeConcreteTool(ctx, "handleRouterShowCostStats", "plan_code_exploration", args);
}
CommandResult handleRouterShowHeatmap(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Show backend selection heatmap by task family";
    return executeConcreteTool(ctx, "handleRouterShowHeatmap", "plan_code_exploration", args);
}
CommandResult handleRouterShowPins(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleRouterShowPins", "load_rules", args);
}
CommandResult handleRouterSimulate(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Simulate routing decision under current policy";
    return executeConcreteTool(ctx, "handleRouterSimulate", "plan_tasks", args);
}
CommandResult handleRouterSimulateLast(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Replay and simulate routing for last prompt";
    return executeConcreteTool(ctx, "handleRouterSimulateLast", "plan_tasks", args);
}
CommandResult handleRouterStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleRouterStatus", "load_rules", args);
}
CommandResult handleRouterUnpinTask(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Remove backend pin for task family";
    return executeConcreteTool(ctx, "handleRouterUnpinTask", "plan_tasks", args);
}
CommandResult handleRouterWhyBackend(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Explain why backend was selected";
    return executeConcreteTool(ctx, "handleRouterWhyBackend", "plan_tasks", args);
}
CommandResult handleAIChatMode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Switch AI chat interaction mode");
    }
    return executeConcreteTool(ctx, "handleAIChatMode", "plan_tasks", args);
}
CommandResult handleAICtx128K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set AI context window to 128K tokens";
    args["context_window"] = 128000;
    return executeConcreteTool(ctx, "handleAICtx128K", "plan_tasks", args);
}
CommandResult handleAICtx1M(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set AI context window to 1M tokens";
    args["context_window"] = 1000000;
    return executeConcreteTool(ctx, "handleAICtx1M", "plan_tasks", args);
}
CommandResult handleAICtx256K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set AI context window to 256K tokens";
    args["context_window"] = 256000;
    return executeConcreteTool(ctx, "handleAICtx256K", "plan_tasks", args);
}
CommandResult handleAICtx32K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set AI context window to 32K tokens";
    args["context_window"] = 32000;
    return executeConcreteTool(ctx, "handleAICtx32K", "plan_tasks", args);
}
CommandResult handleAICtx4K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set AI context window to 4K tokens";
    args["context_window"] = 4096;
    return executeConcreteTool(ctx, "handleAICtx4K", "plan_tasks", args);
}
CommandResult handleAICtx512K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set AI context window to 512K tokens";
    args["context_window"] = 512000;
    return executeConcreteTool(ctx, "handleAICtx512K", "plan_tasks", args);
}
CommandResult handleAICtx64K(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set AI context window to 64K tokens";
    args["context_window"] = 64000;
    return executeConcreteTool(ctx, "handleAICtx64K", "plan_tasks", args);
}
CommandResult handleAIExplainCode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Explain current code context");
    }
    return executeConcreteTool(ctx, "handleAIExplainCode", "semantic_search", args);
}
CommandResult handleAIFixErrors(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Fix diagnostics and failing checks");
    }
    return executeConcreteTool(ctx, "handleAIFixErrors", "plan_tasks", args);
}
CommandResult handleAIGenerateDocs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                                 : std::string("Generate documentation for selected scope");
    }
    return executeConcreteTool(ctx, "handleAIGenerateDocs", "plan_tasks", args);
}
CommandResult handleAIGenerateTests(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] =
            (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Generate tests for selected scope");
    }
    return executeConcreteTool(ctx, "handleAIGenerateTests", "plan_tasks", args);
}
CommandResult handleAIInlineComplete(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("file_path") && ctx.args && ctx.args[0])
    {
        args["file_path"] = std::string(ctx.args);
    }
    return executeConcreteTool(ctx, "handleAIInlineComplete", "next_edit_hint", args);
}
CommandResult handleAIModelSelect(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Select active AI model for current session";
    return executeConcreteTool(ctx, "handleAIModelSelect", "plan_tasks", args);
}
CommandResult handleAINoRefusal(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Enable permissive assistant no-refusal mode with policy checks";
    return executeConcreteTool(ctx, "handleAINoRefusal", "plan_tasks", args);
}
CommandResult handleAIOptimizeCode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Optimize selected code path");
    return executeConcreteTool(ctx, "handleAIOptimizeCode", "plan_tasks", args);
}
CommandResult handleAIRefactor(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Refactor current code context");
    }
    return executeConcreteTool(ctx, "handleAIRefactor", "plan_code_exploration", args);
}
CommandResult handleTelemetryDashboard(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleTelemetryDashboard", "load_rules", args);
}


CommandResult handleAsmAnalyzeBlock(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("analyze assembly basic block");
    return executeConcreteTool(ctx, "handleAsmAnalyzeBlock", "semantic_search", args);
}
CommandResult handleAsmCallGraph(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Build assembly call graph from current target";
    return executeConcreteTool(ctx, "handleAsmCallGraph", "plan_tasks", args);
}
CommandResult handleAsmClearSymbols(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Clear temporary assembly symbols from current analysis session";
    return executeConcreteTool(ctx, "handleAsmClearSymbols", "plan_tasks", args);
}
CommandResult handleAsmDataFlow(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Analyze assembly data-flow graph";
    return executeConcreteTool(ctx, "handleAsmDataFlow", "plan_tasks", args);
}
CommandResult handleAsmDetectConvention(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Detect function calling convention from disassembly";
    return executeConcreteTool(ctx, "handleAsmDetectConvention", "plan_tasks", args);
}
CommandResult handleAsmFindRefs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("assembly symbol references");
    }
    return executeConcreteTool(ctx, "handleAsmFindRefs", "search_code", args);
}
CommandResult handleAsmGoto(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("assembly label or symbol");
    }
    return executeConcreteTool(ctx, "handleAsmGoto", "search_code", args);
}
CommandResult handleAsmInstructionInfo(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("assembly instruction information");
    return executeConcreteTool(ctx, "handleAsmInstructionInfo", "semantic_search", args);
}
CommandResult handleAsmParse(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("parse assembly structure");
    }
    return executeConcreteTool(ctx, "handleAsmParse", "search_code", args);
}
CommandResult handleAsmRegisterInfo(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("assembly register information");
    return executeConcreteTool(ctx, "handleAsmRegisterInfo", "semantic_search", args);
}
CommandResult handleAsmSections(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "List binary/assembly sections and metadata";
    return executeConcreteTool(ctx, "handleAsmSections", "plan_tasks", args);
}
CommandResult handleAsmSymbolTable(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("assembly symbols and tables");
    }
    return executeConcreteTool(ctx, "handleAsmSymbolTable", "search_code", args);
}
CommandResult handleAuditDashboard(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleAuditDashboard", "load_rules", args);
}
CommandResult handleBackendConfigure(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                                 : std::string("Configure AI backend settings and persistence");
    }
    return executeConcreteTool(ctx, "handleBackendConfigure", "plan_tasks", args);
}
CommandResult handleBackendHealthCheck(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Run backend health checks and surface diagnostics");
    }
    return executeConcreteTool(ctx, "handleBackendHealthCheck", "plan_tasks", args);
}
CommandResult handleBackendSaveConfigs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Save backend configuration to durable storage");
    }
    return executeConcreteTool(ctx, "handleBackendSaveConfigs", "plan_tasks", args);
}
CommandResult handleBackendSetApiKey(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Set backend API key securely and validate it");
    }
    return executeConcreteTool(ctx, "handleBackendSetApiKey", "plan_tasks", args);
}
CommandResult handleBackendShowStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleBackendShowStatus", "load_rules", args);
}
CommandResult handleBackendShowSwitcher(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleBackendShowSwitcher", "load_rules", args);
}
CommandResult handleBackendSwitchClaude(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Switch active backend to Claude and validate connectivity");
    }
    return executeConcreteTool(ctx, "handleBackendSwitchClaude", "plan_tasks", args);
}
CommandResult handleBackendSwitchGemini(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Switch active backend to Gemini and validate connectivity");
    }
    return executeConcreteTool(ctx, "handleBackendSwitchGemini", "plan_tasks", args);
}
CommandResult handleBackendSwitchLocal(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Switch active backend to local runtime";
    return executeConcreteTool(ctx, "handleBackendSwitchLocal", "plan_tasks", args);
}
CommandResult handleBackendSwitchOllama(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Switch active backend to Ollama and validate connectivity");
    }
    return executeConcreteTool(ctx, "handleBackendSwitchOllama", "plan_tasks", args);
}
CommandResult handleBackendSwitchOpenAI(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Switch active backend to OpenAI and validate connectivity");
    }
    return executeConcreteTool(ctx, "handleBackendSwitchOpenAI", "plan_tasks", args);
}
CommandResult handleBeaconFullBeacon(const CommandContext& ctx)
{
    g_auto_ssot_beacon_state.store(0x3u, std::memory_order_relaxed);
    g_auto_ssot_beacon_full.store(1u, std::memory_order_relaxed);
    if (ctx.outputFn != nullptr)
    {
        ctx.output("[BEACON][AUTO_SSOT] Full beacon forced (0x3).\n");
    }
    return CommandResult::ok("beacon.full");
}
CommandResult handleBeaconHalfPulse(const CommandContext& ctx)
{
    const std::string arg = firstBeaconArg(ctx);
    if (arg == "avx2" || arg == "low" || arg == "0")
    {
        updateAutoSsotBeaconState(false);
        if (ctx.outputFn != nullptr)
        {
            ctx.output("[BEACON][AUTO_SSOT] AVX2 half-pulse registered.\n");
        }
    }
    else if (arg == "avx512" || arg == "high" || arg == "1")
    {
        updateAutoSsotBeaconState(true);
        if (ctx.outputFn != nullptr)
        {
            ctx.output("[BEACON][AUTO_SSOT] AVX512 half-pulse registered.\n");
        }
    }
    else if (ctx.outputFn != nullptr)
    {
        ctx.output("[BEACON][AUTO_SSOT] Usage: !beacon_half <avx2|avx512>\n");
    }
    return CommandResult::ok("beacon.halfPulse");
}
CommandResult handleBeaconStatus(const CommandContext& ctx)
{
    if (ctx.outputFn != nullptr)
    {
        const unsigned int st = g_auto_ssot_beacon_state.load(std::memory_order_relaxed);
        const unsigned int full = g_auto_ssot_beacon_full.load(std::memory_order_relaxed);
        char buf[160];
        std::snprintf(buf, sizeof(buf), "[BEACON][AUTO_SSOT] State=0x%X, Full=%u\n", st, full);
        ctx.output(buf);
    }
    return CommandResult::ok("beacon.status");
}
CommandResult handleConfidenceSetPolicy(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Set confidence policy thresholds and enforcement rules");
    }
    return executeConcreteTool(ctx, "handleConfidenceSetPolicy", "plan_tasks", args);
}
CommandResult handleConfidenceStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleConfidenceStatus", "load_rules", args);
}
CommandResult handleDbgAddBp(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Add breakpoint at target location";
    return executeConcreteTool(ctx, "handleDbgAddBp", "plan_tasks", args);
}
CommandResult handleDbgAddWatch(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Add watch expression in debugger";
    return executeConcreteTool(ctx, "handleDbgAddWatch", "plan_tasks", args);
}
CommandResult handleDbgAttach(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Attach debugger to running process";
    return executeConcreteTool(ctx, "handleDbgAttach", "plan_tasks", args);
}
CommandResult handleDbgBreak(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Break current debug execution";
    return executeConcreteTool(ctx, "handleDbgBreak", "plan_tasks", args);
}
CommandResult handleDbgClearBps(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Clear all active breakpoints";
    return executeConcreteTool(ctx, "handleDbgClearBps", "plan_tasks", args);
}
CommandResult handleDbgDetach(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Detach debugger session";
    return executeConcreteTool(ctx, "handleDbgDetach", "plan_tasks", args);
}
CommandResult handleDbgDisasm(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Disassemble current frame");
    return executeConcreteTool(ctx, "handleDbgDisasm", "semantic_search", args);
}
CommandResult handleDbgEnableBp(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Enable selected breakpoint";
    return executeConcreteTool(ctx, "handleDbgEnableBp", "plan_tasks", args);
}
CommandResult handleDbgEvaluate(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Evaluate expression in debugger context";
    return executeConcreteTool(ctx, "handleDbgEvaluate", "plan_tasks", args);
}
CommandResult handleDbgGo(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Continue debugger execution";
    return executeConcreteTool(ctx, "handleDbgGo", "plan_tasks", args);
}
CommandResult handleDbgKill(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Terminate debugged process";
    return executeConcreteTool(ctx, "handleDbgKill", "plan_tasks", args);
}
CommandResult handleDbgLaunch(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Launch debugger with configured profile";
    return executeConcreteTool(ctx, "handleDbgLaunch", "plan_tasks", args);
}
CommandResult handleDbgListBps(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("List active breakpoints");
    return executeConcreteTool(ctx, "handleDbgListBps", "plan_tasks", args);
}
CommandResult handleDbgMemory(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Inspect debugger memory region";
    return executeConcreteTool(ctx, "handleDbgMemory", "plan_tasks", args);
}
CommandResult handleDbgModules(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "List loaded debugger modules";
    return executeConcreteTool(ctx, "handleDbgModules", "plan_tasks", args);
}
CommandResult handleDbgRegisters(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Inspect current debugger registers");
    return executeConcreteTool(ctx, "handleDbgRegisters", "plan_tasks", args);
}
CommandResult handleDbgRemoveBp(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Remove selected breakpoint";
    return executeConcreteTool(ctx, "handleDbgRemoveBp", "plan_tasks", args);
}
CommandResult handleDbgRemoveWatch(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Remove watch expression";
    return executeConcreteTool(ctx, "handleDbgRemoveWatch", "plan_tasks", args);
}
CommandResult handleDbgSearchMemory(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Search bytes or pattern in process memory";
    return executeConcreteTool(ctx, "handleDbgSearchMemory", "plan_tasks", args);
}
CommandResult handleDbgSetRegister(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set debugger register value";
    return executeConcreteTool(ctx, "handleDbgSetRegister", "plan_tasks", args);
}
CommandResult handleDbgStack(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Inspect debugger call stack";
    return executeConcreteTool(ctx, "handleDbgStack", "plan_tasks", args);
}
CommandResult handleDbgStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleDbgStatus", "load_rules", args);
}
CommandResult handleDbgStepInto(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Step into next instruction";
    return executeConcreteTool(ctx, "handleDbgStepInto", "plan_tasks", args);
}
CommandResult handleDbgStepOut(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Step out of current frame";
    return executeConcreteTool(ctx, "handleDbgStepOut", "plan_tasks", args);
}
CommandResult handleDbgStepOver(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Step over next instruction";
    return executeConcreteTool(ctx, "handleDbgStepOver", "plan_tasks", args);
}
CommandResult handleDbgSwitchThread(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Switch active debugger thread";
    return executeConcreteTool(ctx, "handleDbgSwitchThread", "plan_tasks", args);
}
CommandResult handleDbgSymbolPath(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Configure debugger symbol path";
    return executeConcreteTool(ctx, "handleDbgSymbolPath", "plan_tasks", args);
}
CommandResult handleDbgThreads(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "List debugger threads and current selection";
    return executeConcreteTool(ctx, "handleDbgThreads", "plan_tasks", args);
}
CommandResult handleDiskListDrives(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("List available drives and mount metadata");
    return executeConcreteTool(ctx, "handleDiskListDrives", "plan_tasks", args);
}
CommandResult handleDiskScanPartitions(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Scan partitions and summarize layout");
    return executeConcreteTool(ctx, "handleDiskScanPartitions", "plan_tasks", args);
}
CommandResult handleEditClipboardHist(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Show and manage clipboard history entries";
    return executeConcreteTool(ctx, "handleEditClipboardHist", "plan_tasks", args);
}
CommandResult handleEditorCycle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Cycle editor engine and preserve state");
    return executeConcreteTool(ctx, "handleEditorCycle", "plan_tasks", args);
}
CommandResult handleEditorMonacoCore(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Switch editor engine to Monaco core and verify state");
    return executeConcreteTool(ctx, "handleEditorMonacoCore", "plan_tasks", args);
}
CommandResult handleEditorRichEdit(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Switch editor engine to RichEdit and verify state");
    return executeConcreteTool(ctx, "handleEditorRichEdit", "plan_tasks", args);
}
CommandResult handleEditorStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleEditorStatus", "load_rules", args);
}
CommandResult handleEditorWebView2(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Switch editor engine to WebView2 and verify state");
    return executeConcreteTool(ctx, "handleEditorWebView2", "plan_tasks", args);
}
CommandResult handleEmbeddingEncode(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("encode embeddings for input");
    }
    return executeConcreteTool(ctx, "handleEmbeddingEncode", "semantic_search", args);
}
CommandResult handleFileAutoSave(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Enable and validate file autosave workflow");
    }
    return executeConcreteTool(ctx, "handleFileAutoSave", "plan_tasks", args);
}
CommandResult handleGovernorSetPowerLevel(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = (ctx.args && ctx.args[0])
                           ? std::string(ctx.args)
                           : std::string("Set execution governor power level and validate limits");
    }
    return executeConcreteTool(ctx, "handleGovernorSetPowerLevel", "plan_tasks", args);
}
CommandResult handleGovernorStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleGovernorStatus", "load_rules", args);
}
CommandResult handleGovKillAll(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Stop all governor-managed tasks safely");
    }
    return executeConcreteTool(ctx, "handleGovKillAll", "plan_tasks", args);
}
CommandResult handleGovStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleGovStatus", "load_rules", args);
}
CommandResult handleGovSubmitCommand(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                                 : std::string("Submit command to governor execution queue");
    }
    return executeConcreteTool(ctx, "handleGovSubmitCommand", "plan_tasks", args);
}
CommandResult handleGovTaskList(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("List governor task queue and states");
    return executeConcreteTool(ctx, "handleGovTaskList", "plan_tasks", args);
}
CommandResult handleHelpCmdRef(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("command reference");
    }
    return executeConcreteTool(ctx, "handleHelpCmdRef", "search_code", args);
}
CommandResult handleHelpPsDocs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("powershell docs");
    }
    return executeConcreteTool(ctx, "handleHelpPsDocs", "search_code", args);
}
CommandResult handleHotpatchEventLog(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleHotpatchEventLog", "load_rules", args);
}
CommandResult handleHotpatchMemRevert(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Revert memory hotpatches and restore baseline");
    }
    return executeConcreteTool(ctx, "handleHotpatchMemRevert", "plan_tasks", args);
}
CommandResult handleHotpatchProxyStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleHotpatchProxyStats", "load_rules", args);
}
CommandResult handleHybridAnalyzeFile(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("analyze current file context");
    }
    return executeConcreteTool(ctx, "handleHybridAnalyzeFile", "semantic_search", args);
}
CommandResult handleHybridAnnotateDiag(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                             : std::string("Annotate diagnostics with actionable guidance");
    return executeConcreteTool(ctx, "handleHybridAnnotateDiag", "plan_tasks", args);
}
CommandResult handleHybridAutoProfile(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Run automatic project profiling");
    return executeConcreteTool(ctx, "handleHybridAutoProfile", "plan_tasks", args);
}
CommandResult handleHybridComplete(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("file_path") && ctx.args && ctx.args[0])
    {
        args["file_path"] = std::string(ctx.args);
    }
    return executeConcreteTool(ctx, "handleHybridComplete", "next_edit_hint", args);
}
CommandResult handleHybridCorrectionLoop(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                             : std::string("Run hybrid correction loop over failing diagnostics");
    return executeConcreteTool(ctx, "handleHybridCorrectionLoop", "plan_tasks", args);
}
CommandResult handleHybridDiagnostics(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleHybridDiagnostics", "load_rules", args);
}
CommandResult handleHybridExplainSymbol(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("explain symbol usage");
    }
    return executeConcreteTool(ctx, "handleHybridExplainSymbol", "semantic_search", args);
}
CommandResult handleHybridSemanticPrefetch(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Run hybrid semantic prefetch for current code context";
    return executeConcreteTool(ctx, "handleHybridSemanticPrefetch", "plan_tasks", args);
}
CommandResult handleHybridSmartRename(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Execute hybrid smart rename with safety checks";
    return executeConcreteTool(ctx, "handleHybridSmartRename", "plan_tasks", args);
}
CommandResult handleHybridStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleHybridStatus", "load_rules", args);
}
CommandResult handleHybridStreamAnalyze(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Analyze stream using hybrid model pipeline";
    return executeConcreteTool(ctx, "handleHybridStreamAnalyze", "plan_tasks", args);
}
CommandResult handleHybridSymbolUsage(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("symbol usage");
    }
    return executeConcreteTool(ctx, "handleHybridSymbolUsage", "semantic_search", args);
}
CommandResult handleLspClearDiag(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Clear or acknowledge active diagnostics");
    return executeConcreteTool(ctx, "handleLspClearDiag", "plan_tasks", args);
}
CommandResult handleLspConfigure(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Configure LSP settings and server bindings");
    return executeConcreteTool(ctx, "handleLspConfigure", "plan_tasks", args);
}
CommandResult handleLspDiagnostics(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleLspDiagnostics", "get_diagnostics", args);
}
CommandResult handleLspFindRefs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("find symbol references");
    }
    return executeConcreteTool(ctx, "handleLspFindRefs", "search_code", args);
}
CommandResult handleLspGotoDef(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("go to symbol definition");
    }
    return executeConcreteTool(ctx, "handleLspGotoDef", "search_code", args);
}
CommandResult handleLspHover(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("hover symbol information");
    }
    return executeConcreteTool(ctx, "handleLspHover", "semantic_search", args);
}
CommandResult handleLspRename(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Rename symbol and update references");
    return executeConcreteTool(ctx, "handleLspRename", "plan_tasks", args);
}
CommandResult handleLspRestart(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Restart active LSP servers safely");
    return executeConcreteTool(ctx, "handleLspRestart", "plan_tasks", args);
}
CommandResult handleLspSaveConfig(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Save current LSP configuration");
    return executeConcreteTool(ctx, "handleLspSaveConfig", "plan_tasks", args);
}
CommandResult handleLspSrvConfig(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Configure LSP server process parameters");
    return executeConcreteTool(ctx, "handleLspSrvConfig", "plan_tasks", args);
}
CommandResult handleLspSrvExportSymbols(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Export symbol index from LSP server");
    return executeConcreteTool(ctx, "handleLspSrvExportSymbols", "plan_tasks", args);
}
CommandResult handleLspSrvLaunchStdio(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Launch LSP server in stdio mode");
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
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Reindex LSP server symbol database");
    return executeConcreteTool(ctx, "handleLspSrvReindex", "plan_tasks", args);
}
CommandResult handleLspSrvStart(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Start LSP server host process");
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
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Stop LSP server host process");
    return executeConcreteTool(ctx, "handleLspSrvStop", "plan_tasks", args);
}
CommandResult handleLspStartAll(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Start all configured LSP servers");
    return executeConcreteTool(ctx, "handleLspStartAll", "plan_tasks", args);
}
CommandResult handleLspStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleLspStatus", "load_rules", args);
}
CommandResult handleLspStopAll(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Stop all running LSP servers");
    return executeConcreteTool(ctx, "handleLspStopAll", "plan_tasks", args);
}
CommandResult handleLspSymbolInfo(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("query"))
    {
        args["query"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("symbol information");
    }
    return executeConcreteTool(ctx, "handleLspSymbolInfo", "semantic_search", args);
}
CommandResult handleMarketplaceInstall(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Install package from marketplace and verify");
    return executeConcreteTool(ctx, "handleMarketplaceInstall", "plan_tasks", args);
}
CommandResult handleMarketplaceList(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("List marketplace packages and status");
    return executeConcreteTool(ctx, "handleMarketplaceList", "plan_tasks", args);
}
CommandResult handleModelFinetune(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Fine-tune model with provided dataset");
    return executeConcreteTool(ctx, "handleModelFinetune", "plan_tasks", args);
}
CommandResult handleModelList(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("List available local and remote models");
    return executeConcreteTool(ctx, "handleModelList", "plan_tasks", args);
}
CommandResult handleModelLoad(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Load selected model into active runtime");
    return executeConcreteTool(ctx, "handleModelLoad", "plan_tasks", args);
}
CommandResult handleModelQuantize(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Quantize selected model for deployment");
    return executeConcreteTool(ctx, "handleModelQuantize", "plan_tasks", args);
}
CommandResult handleModelUnload(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Unload active model and release resources");
    return executeConcreteTool(ctx, "handleModelUnload", "plan_tasks", args);
}
CommandResult handleMonacoDevtools(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Open Monaco editor devtools diagnostics";
    return executeConcreteTool(ctx, "handleMonacoDevtools", "plan_tasks", args);
}
CommandResult handleMonacoReload(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Reload Monaco editor runtime";
    return executeConcreteTool(ctx, "handleMonacoReload", "plan_tasks", args);
}
CommandResult handleMonacoSyncTheme(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Sync Monaco theme with IDE theme";
    return executeConcreteTool(ctx, "handleMonacoSyncTheme", "plan_tasks", args);
}
CommandResult handleMonacoToggle(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Toggle Monaco editor integration";
    return executeConcreteTool(ctx, "handleMonacoToggle", "plan_tasks", args);
}
CommandResult handleMonacoZoomIn(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Increase Monaco editor zoom level";
    return executeConcreteTool(ctx, "handleMonacoZoomIn", "plan_tasks", args);
}
CommandResult handleMonacoZoomOut(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Decrease Monaco editor zoom level";
    return executeConcreteTool(ctx, "handleMonacoZoomOut", "plan_tasks", args);
}
CommandResult handleMultiRespApplyPreferred(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Apply preferred multi-response candidate to editor";
    return executeConcreteTool(ctx, "handleMultiRespApplyPreferred", "plan_tasks", args);
}
CommandResult handleMultiRespClearHistory(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Clear multi-response generation history";
    return executeConcreteTool(ctx, "handleMultiRespClearHistory", "plan_tasks", args);
}
CommandResult handleMultiRespCompare(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args)
                                             : std::string("Compare multi-response candidates and rank quality");
    return executeConcreteTool(ctx, "handleMultiRespCompare", "plan_tasks", args);
}
CommandResult handleMultiRespGenerate(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Generate multiple response candidates");
    return executeConcreteTool(ctx, "handleMultiRespGenerate", "plan_tasks", args);
}
CommandResult handleMultiRespSelectPreferred(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Select preferred multi-response candidate";
    return executeConcreteTool(ctx, "handleMultiRespSelectPreferred", "plan_tasks", args);
}
CommandResult handleMultiRespSetMax(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Set maximum number of generated response candidates";
    return executeConcreteTool(ctx, "handleMultiRespSetMax", "plan_tasks", args);
}
CommandResult handleMultiRespShowLatest(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Show latest multi-response candidates";
    return executeConcreteTool(ctx, "handleMultiRespShowLatest", "plan_tasks", args);
}
CommandResult handleMultiRespShowPrefs(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleMultiRespShowPrefs", "load_rules", args);
}
CommandResult handleMultiRespShowStats(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Show multi-response statistics";
    return executeConcreteTool(ctx, "handleMultiRespShowStats", "plan_tasks", args);
}
CommandResult handleMultiRespShowStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleMultiRespShowStatus", "load_rules", args);
}
CommandResult handleMultiRespShowTemplates(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleMultiRespShowTemplates", "load_rules", args);
}
CommandResult handleMultiRespToggleTemplate(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Toggle multi-response template";
    return executeConcreteTool(ctx, "handleMultiRespToggleTemplate", "plan_tasks", args);
}
CommandResult handlePluginConfigure(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] =
        (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Configure plugin subsystem settings");
    return executeConcreteTool(ctx, "handlePluginConfigure", "plan_tasks", args);
}
CommandResult handlePluginLoad(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Load plugin and verify activation");
    return executeConcreteTool(ctx, "handlePluginLoad", "plan_tasks", args);
}
CommandResult handlePluginRefresh(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = std::string("Refresh plugin registry and loaded state");
    return executeConcreteTool(ctx, "handlePluginRefresh", "plan_tasks", args);
}
CommandResult handlePluginScanDir(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Scan plugin directory and refresh plugin manifest entries";
    return executeConcreteTool(ctx, "handlePluginScanDir", "plan_tasks", args);
}
CommandResult handlePluginShowPanel(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Show plugin management panel and current plugin status";
    return executeConcreteTool(ctx, "handlePluginShowPanel", "plan_tasks", args);
}
CommandResult handlePluginShowStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handlePluginShowStatus", "load_rules", args);
}
CommandResult handlePluginToggleHotload(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Toggle plugin hotload mode";
    return executeConcreteTool(ctx, "handlePluginToggleHotload", "plan_tasks", args);
}
CommandResult handlePluginUnload(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = (ctx.args && ctx.args[0]) ? std::string(ctx.args) : std::string("Unload plugin and clean resources");
    return executeConcreteTool(ctx, "handlePluginUnload", "plan_tasks", args);
}
CommandResult handlePluginUnloadAll(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Unload all active plugins and clear plugin runtime state";
    return executeConcreteTool(ctx, "handlePluginUnloadAll", "plan_tasks", args);
}
CommandResult handlePromptClassifyContext(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Classify prompt context for routing and policy";
    return executeConcreteTool(ctx, "handlePromptClassifyContext", "plan_tasks", args);
}
CommandResult handleQwAlertDismiss(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Dismiss current quality warning alert";
    return executeConcreteTool(ctx, "handleQwAlertDismiss", "plan_tasks", args);
}
CommandResult handleQwAlertHistory(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    return executeConcreteTool(ctx, "handleQwAlertHistory", "load_rules", args);
}
CommandResult handleQwAlertMonitor(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    args["goal"] = "Monitor quality warning alerts in real time";
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
CommandResult handleReplayCheckpoint(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleReplayCheckpoint");
    }
    return executeConcreteTool(ctx, "handleReplayCheckpoint", "plan_tasks", args);
}
CommandResult handleReplayExportSession(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleReplayExportSession");
    }
    return executeConcreteTool(ctx, "handleReplayExportSession", "plan_tasks", args);
}
CommandResult handleReplayShowLast(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleReplayShowLast");
    }
    return executeConcreteTool(ctx, "handleReplayShowLast", "plan_tasks", args);
}
CommandResult handleReplayStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleReplayStatus");
    }
    return executeConcreteTool(ctx, "handleReplayStatus", "plan_tasks", args);
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
CommandResult handleRevengDecompile(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRevengDecompile");
    }
    return executeConcreteTool(ctx, "handleRevengDecompile", "plan_tasks", args);
}
CommandResult handleRevengDisassemble(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRevengDisassemble");
    }
    return executeConcreteTool(ctx, "handleRevengDisassemble", "plan_tasks", args);
}
CommandResult handleRevengFindVulnerabilities(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleRevengFindVulnerabilities");
    }
    return executeConcreteTool(ctx, "handleRevengFindVulnerabilities", "plan_tasks", args);
}
CommandResult handleSafetyResetBudget(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSafetyResetBudget");
    }
    return executeConcreteTool(ctx, "handleSafetyResetBudget", "plan_tasks", args);
}
CommandResult handleSafetyRollbackLast(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSafetyRollbackLast");
    }
    return executeConcreteTool(ctx, "handleSafetyRollbackLast", "plan_tasks", args);
}
CommandResult handleSafetyShowViolations(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSafetyShowViolations");
    }
    return executeConcreteTool(ctx, "handleSafetyShowViolations", "plan_tasks", args);
}
CommandResult handleSafetyStatus(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleSafetyStatus");
    }
    return executeConcreteTool(ctx, "handleSafetyStatus", "plan_tasks", args);
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
CommandResult handleUnityAttach(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleUnityAttach");
    }
    return executeConcreteTool(ctx, "handleUnityAttach", "plan_tasks", args);
}
CommandResult handleUnityInit(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleUnityInit");
    }
    return executeConcreteTool(ctx, "handleUnityInit", "plan_tasks", args);
}
CommandResult handleUnrealAttach(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleUnrealAttach");
    }
    return executeConcreteTool(ctx, "handleUnrealAttach", "plan_tasks", args);
}
CommandResult handleUnrealInit(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleUnrealInit");
    }
    return executeConcreteTool(ctx, "handleUnrealInit", "plan_tasks", args);
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
CommandResult handleVisionAnalyzeImage(const CommandContext& ctx)
{
    nlohmann::json args = RawrXD::Core::ParseFallbackArgs(ctx.args);
    if (!args.contains("goal"))
    {
        args["goal"] = std::string("Execute command handler: handleVisionAnalyzeImage");
    }
    return executeConcreteTool(ctx, "handleVisionAnalyzeImage", "plan_tasks", args);
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
