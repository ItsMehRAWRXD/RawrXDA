#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace RawrXD::Core
{
inline nlohmann::json ParseFallbackArgs(const char* rawArgs)
{
    nlohmann::json args = nlohmann::json::object();
    if (!(rawArgs && rawArgs[0] != '\0'))
        return args;
    try
    {
        args = nlohmann::json::parse(rawArgs);
        if (!args.is_object())
            args = nlohmann::json::object();
    }
    catch (...)
    {
        args = nlohmann::json::object();
        args["query"] = std::string(rawArgs);
    }
    return args;
}

inline bool ResolveFallbackToolRoute(const std::string& handlerName, const char* rawArgs, nlohmann::json& args,
                                     std::string& tool)
{
    args = ParseFallbackArgs(rawArgs);
    if (args.contains("tool") && args["tool"].is_string())
    {
        tool = args["tool"].get<std::string>();
        return true;
    }

    if (handlerName.find("handleSubagent") == 0)
    {
        if (handlerName.find("Todo") != std::string::npos)
        {
            tool = "plan_tasks";
            if (!args.contains("task"))
                args["task"] = "Subagent TODO fallback operation";
        }
        else if (handlerName.find("Status") != std::string::npos)
        {
            tool = "load_rules";
        }
        else
        {
            tool = "plan_code_exploration";
            if (!args.contains("goal"))
                args["goal"] = "Subagent fallback exploration operation";
        }
        return true;
    }

    if (handlerName.find("handleRouter") == 0)
    {
        if (handlerName.find("Capabilities") != std::string::npos)
        {
            tool = "load_rules";
        }
        else
        {
            tool = "optimize_tool_selection";
            if (!args.contains("task"))
                args["task"] = "Router fallback operation";
        }
        return true;
    }

    if (handlerName.find("handleHybrid") == 0)
    {
        // Hybrid commands are routed to concrete analysis/edit-planning tools.
        if (handlerName.find("Diagnostics") != std::string::npos)
            tool = "get_diagnostics";
        else if (handlerName.find("Explain") != std::string::npos || handlerName.find("Symbol") != std::string::npos ||
                 handlerName.find("Rename") != std::string::npos)
            tool = "resolve_symbol";
        else if (handlerName.find("Complete") != std::string::npos ||
                 handlerName.find("Correction") != std::string::npos)
            tool = "next_edit_hint";
        else
            tool = "semantic_search";
        if (tool == "resolve_symbol")
        {
            if (!args.contains("symbol"))
                args["symbol"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("main");
        }
        else if (tool == "get_diagnostics")
        {
            if (!args.contains("file"))
                args["file"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("CMakeLists.txt");
        }
        else if (tool == "next_edit_hint")
        {
            if (!args.contains("context"))
                args["context"] =
                    (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("hybrid edit assistance");
        }
        else if (!args.contains("query"))
        {
            args["query"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("hybrid fallback operation");
        }
        return true;
    }

    if (handlerName.find("handleMultiResp") == 0)
    {
        if (handlerName.find("Generate") != std::string::npos || handlerName.find("Compare") != std::string::npos)
            tool = "optimize_tool_selection";
        else
            tool = "plan_tasks";
        if (tool == "optimize_tool_selection")
        {
            if (!args.contains("task"))
                args["task"] =
                    (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("multi-response generation");
        }
        else if (!args.contains("goal"))
        {
            args["goal"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("multi-response fallback");
        }
        return true;
    }

    if (handlerName.find("handleAutonomy") == 0 || handlerName.find("handleTelemetry") == 0)
    {
        if (handlerName.find("Status") != std::string::npos || handlerName.find("Dashboard") != std::string::npos ||
            handlerName.find("Snapshot") != std::string::npos || handlerName.find("Export") != std::string::npos)
        {
            tool = "load_rules";
        }
        else
        {
            tool = "plan_tasks";
            if (!args.contains("goal"))
                args["goal"] = std::string("Autonomy/telemetry workflow fallback for ") + handlerName;
        }
        return true;
    }

    if (handlerName.find("handleLsp") == 0)
    {
        if (handlerName.find("Goto") != std::string::npos || handlerName.find("FindRefs") != std::string::npos ||
            handlerName.find("Rename") != std::string::npos || handlerName.find("Hover") != std::string::npos ||
            handlerName.find("Symbol") != std::string::npos)
            tool = "resolve_symbol";
        else if (handlerName.find("Config") != std::string::npos || handlerName.find("Restart") != std::string::npos ||
                 handlerName.find("Start") != std::string::npos || handlerName.find("Stop") != std::string::npos ||
                 handlerName.find("Status") != std::string::npos)
            tool = "load_rules";
        else
            tool = "get_diagnostics";
        if (tool == "resolve_symbol")
        {
            if (!args.contains("symbol"))
                args["symbol"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("main");
        }
        else if (tool == "get_diagnostics")
        {
            if (args.contains("path") && args["path"].is_string())
                args["file"] = args["path"];
            if (!args.contains("file"))
                args["file"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("CMakeLists.txt");
        }
        return true;
    }

    if (handlerName.find("handleBackend") == 0 || handlerName.find("handleAudit") == 0)
    {
        tool = "load_rules";
        return true;
    }

    if (handlerName.find("handleModel") == 0)
    {
        tool = "search_files";
        if (!args.contains("pattern"))
            args["pattern"] = "*.gguf";
        return true;
    }

    if (handlerName.find("handlePlugin") == 0 || handlerName.find("handleMarketplace") == 0 ||
        handlerName.find("handleVscExt") == 0 || handlerName.find("handleVscext") == 0 ||
        handlerName.find("handleDisk") == 0)
    {
        tool = "list_dir";
        if (!args.contains("path"))
            args["path"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string(".");
        return true;
    }

    if (handlerName.find("handleTheme") == 0 || handlerName.find("handleVoice") == 0 ||
        handlerName.find("handleTransparency") == 0 || handlerName.find("handleDbg") == 0 ||
        handlerName.find("handlePrompt") == 0)
    {
        tool = "plan_tasks";
        if (!args.contains("task"))
            args["task"] = std::string("fallback operation for ") + handlerName;
        return true;
    }

    if (handlerName.find("handleAsm") == 0 || handlerName.find("handleReveng") == 0 ||
        handlerName.find("handleRE") == 0)
    {
        tool = "search_code";
        if (!args.contains("query"))
            args["query"] = std::string("code intelligence for ") + handlerName;
        if (!args.contains("max_results"))
            args["max_results"] = 50;
        return true;
    }

    if (handlerName.find("handleGovernor") == 0 || handlerName.find("handleGov") == 0 ||
        handlerName.find("handleSafety") == 0)
    {
        if (handlerName.find("Status") != std::string::npos)
            tool = "load_rules";
        else
            tool = "plan_tasks";
        if (tool == "plan_tasks" && !args.contains("goal"))
            args["goal"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("governance policy action");
        return true;
    }

    if (handlerName.find("handleReplay") == 0)
    {
        tool = "restore_checkpoint";
        if (!args.contains("checkpoint_path"))
            args["checkpoint_path"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("");
        return true;
    }

    if (handlerName.find("handleEmbedding") == 0)
    {
        tool = "semantic_search";
        if (!args.contains("query"))
            args["query"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("embedding fallback");
        return true;
    }

    if (handlerName.find("handleAI") == 0 || handlerName.find("handleAgent") == 0)
    {
        if (handlerName.find("Explain") != std::string::npos || handlerName.find("Refactor") != std::string::npos ||
            handlerName.find("Generate") != std::string::npos || handlerName.find("Fix") != std::string::npos)
        {
            tool = "next_edit_hint";
            if (!args.contains("context"))
                args["context"] = "AI assist fallback operation";
        }
        else
        {
            tool = "optimize_tool_selection";
            if (!args.contains("task"))
                args["task"] = "AI/Agent fallback operation";
        }
        return true;
    }

    if (handlerName.find("handleHelp") == 0 || handlerName.find("handleEdit") == 0)
    {
        tool = "search_code";
        if (!args.contains("query"))
            args["query"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("TODO");
        if (!args.contains("max_results"))
            args["max_results"] = 50;
        return true;
    }

    if (handlerName.find("handleTools") == 0)
    {
        tool = "run_shell";
        if (!args.contains("command"))
            args["command"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("dir");
        return true;
    }

    if (handlerName.find("handleView") == 0)
    {
        tool = "list_dir";
        if (!args.contains("path"))
            args["path"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string(".");
        return true;
    }

    if (handlerName.find("handleLsp") == 0)
    {
        tool = "get_diagnostics";
        if (args.contains("path") && args["path"].is_string())
            args["file"] = args["path"];
        else if (!args.contains("file"))
            args["file"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("CMakeLists.txt");
        return true;
    }

    if (handlerName.find("handleBackend") == 0)
    {
        tool = "load_rules";
        return true;
    }

    if (handlerName.find("handleModel") == 0)
    {
        tool = "search_files";
        if (!args.contains("pattern"))
            args["pattern"] = "*.gguf";
        return true;
    }

    if (handlerName.find("handlePlugin") == 0 || handlerName.find("handleMarketplace") == 0 ||
        handlerName.find("handleVscExt") == 0 || handlerName.find("handleVscext") == 0 ||
        handlerName.find("handleDisk") == 0)
    {
        tool = "list_dir";
        if (!args.contains("path"))
            args["path"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string(".");
        return true;
    }

    if (handlerName.find("handleTheme") == 0 || handlerName.find("handleVoice") == 0 ||
        handlerName.find("handleTransparency") == 0 || handlerName.find("handleDbg") == 0 ||
        handlerName.find("handleMultiResp") == 0)
    {
        tool = "plan_tasks";
        if (!args.contains("goal"))
            args["goal"] = std::string("Apply command policy for ") + handlerName;
        return true;
    }

    if (handlerName.find("handleHybrid") == 0 || handlerName.find("handleEmbedding") == 0)
    {
        tool = "semantic_search";
        if (!args.contains("query"))
            args["query"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string(handlerName + " fallback");
        return true;
    }

    if (handlerName.find("handlePrompt") == 0)
    {
        tool = "optimize_tool_selection";
        if (!args.contains("task"))
            args["task"] = std::string("Prompt fallback operation for ") + handlerName;
        return true;
    }

    if (handlerName.find("handleAsm") == 0 || handlerName.find("handleReveng") == 0 ||
        handlerName.find("handleRE") == 0)
    {
        tool = "search_code";
        if (!args.contains("query"))
            args["query"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string(handlerName + " fallback");
        if (!args.contains("max_results"))
            args["max_results"] = 50;
        return true;
    }

    if (handlerName.find("handleGovernor") == 0 || handlerName.find("handleGov") == 0 ||
        handlerName.find("handleSafety") == 0 || handlerName.find("handleUnity") == 0 ||
        handlerName.find("handleUnreal") == 0)
    {
        if (handlerName.find("Status") != std::string::npos || handlerName.find("Rules") != std::string::npos ||
            handlerName.find("Policy") != std::string::npos)
        {
            tool = "load_rules";
        }
        else
        {
            tool = "plan_tasks";
            if (!args.contains("goal"))
                args["goal"] = std::string("Governance/safety workflow fallback for ") + handlerName;
        }
        return true;
    }

    if (handlerName.find("handleReplay") == 0)
    {
        tool = "restore_checkpoint";
        if (!args.contains("checkpoint_path"))
            args["checkpoint_path"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("");
        return true;
    }

    if (handlerName.find("handleFile") == 0)
    {
        tool = "list_dir";
        if (!args.contains("path"))
            args["path"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string(".");
        return true;
    }

    if (handlerName.find("handleMonaco") == 0 || handlerName.find("handleTier1") == 0 ||
        handlerName.find("handleQw") == 0 || handlerName.find("handleTrans") == 0)
    {
        tool = "plan_tasks";
        if (!args.contains("goal"))
            args["goal"] = std::string("UI workflow fallback for ") + handlerName;
        return true;
    }

    if (handlerName.find("handleSwarm") == 0)
    {
        tool = "plan_code_exploration";
        if (!args.contains("goal"))
            args["goal"] = std::string("Swarm orchestration fallback for ") + handlerName;
        return true;
    }

    if (handlerName.find("handleHotpatch") == 0)
    {
        if (handlerName.find("Show") != std::string::npos || handlerName.find("Status") != std::string::npos ||
            handlerName.find("Stats") != std::string::npos)
        {
            tool = "load_rules";
        }
        else
        {
            tool = "plan_tasks";
            if (!args.contains("goal"))
                args["goal"] = std::string("Hotpatch workflow fallback for ") + handlerName;
        }
        return true;
    }

    if (handlerName.find("handleAudit") == 0)
    {
        tool = "load_rules";
        return true;
    }

    if (handlerName.find("handleGit") == 0)
    {
        tool = "git_status";
        return true;
    }

    if (handlerName.find("handleEditor") == 0)
    {
        tool = "plan_tasks";
        if (!args.contains("goal"))
            args["goal"] = std::string("Editor workflow fallback for ") + handlerName;
        return true;
    }

    if (handlerName.find("handleTerminal") == 0)
    {
        tool = "run_shell";
        if (!args.contains("command"))
            args["command"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string("dir");
        return true;
    }

    if (handlerName.find("handleDecomp") == 0)
    {
        tool = "search_code";
        if (!args.contains("query"))
            args["query"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string(handlerName + " fallback");
        if (!args.contains("max_results"))
            args["max_results"] = 40;
        return true;
    }

    if (handlerName.find("handlePdb") == 0 || handlerName.find("handleModules") == 0)
    {
        tool = "search_files";
        if (!args.contains("pattern"))
            args["pattern"] = "*";
        return true;
    }

    if (handlerName.find("handleGauntlet") == 0)
    {
        tool = "plan_tasks";
        if (!args.contains("goal"))
            args["goal"] = std::string("Gauntlet workflow fallback for ") + handlerName;
        return true;
    }

    if (handlerName.find("handleConfidence") == 0)
    {
        tool = "load_rules";
        return true;
    }

    if (handlerName.find("handleBeacon") == 0)
    {
        tool = "search_files";
        if (!args.contains("pattern"))
            args["pattern"] = "*";
        return true;
    }

    if (handlerName.find("handleVision") == 0)
    {
        tool = "semantic_search";
        if (!args.contains("query"))
            args["query"] = (rawArgs && rawArgs[0]) ? std::string(rawArgs) : std::string(handlerName + " fallback");
        return true;
    }

    return false;
}
}  // namespace RawrXD::Core
