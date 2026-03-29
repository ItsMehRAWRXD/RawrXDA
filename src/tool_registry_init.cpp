#include "tool_registry_init.hpp"
#include "tool_registry.h"
#include "engine_iface.h"
#include <iostream>
#include <string>
#include <sstream>
#include "agentic/AgentToolHandlers.h"
#include "tool_registry.hpp"
#include <nlohmann/json.hpp>

// ============================================================================
// Helper: wrap a ToolCallResult into the json shape ToolRegistry handlers return.
// ============================================================================
static nlohmann::json toolResultToJson(const RawrXD::Agent::ToolCallResult& r)
{
    return r.toJson();
}

// ============================================================================
// initializeAllTools — registers all 7 production IDE operational tools plus
// the supporting category registrations.  Returns true if all succeeded.
// ============================================================================
bool initializeAllTools(ToolRegistry* registry)
{
    if (!registry)
        return false;

    int total = 0;
    total += registerCodeAnalysisTools(registry);   // compact_conversation, optimize_tool_selection, resolve_symbol
    total += registerFileSystemTools(registry);     // read_lines, search_files
    total += registerBuildTestTools(registry);      // plan_code_exploration, evaluate_integration_audit_feasibility
    total += registerVersionControlTools(registry); // git operations stub (0 currently)
    total += registerExecutionTools(registry);      // execute_command, run_shell
    total += registerModelTools(registry);          // inference stubs
    total += registerDeploymentTools(registry);     // deployment stubs

    std::cout << "[REGISTRY] initializeAllTools: " << total << " tools registered\n";
    return total > 0;
}

// ============================================================================
// registerCodeAnalysisTools — compact_conversation, optimize_tool_selection,
//                              resolve_symbol
// ============================================================================
int registerCodeAnalysisTools(ToolRegistry* registry)
{
    if (!registry)
        return 0;
    int n = 0;

    {
        ToolDefinition def;
        def.name        = "compact_conversation";
        def.description = "Compact agent conversation history to the last N events";
        def.category    = ToolCategory::Analysis;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::CompactConversation(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "optimize_tool_selection";
        def.description = "Recommend the best tool set for a given task description";
        def.category    = ToolCategory::Analysis;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::OptimizeToolSelection(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "resolve_symbol";
        def.description = "Locate all definitions and usages of a symbol in the workspace";
        def.category    = ToolCategory::Analysis;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::ResolveSymbol(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    std::cout << "[REGISTRY] registerCodeAnalysisTools: " << n << " tools\n";
    return n;
}

// ============================================================================
// registerFileSystemTools — read_lines, search_files (+ foundational fs ops)
// ============================================================================
int registerFileSystemTools(ToolRegistry* registry)
{
    if (!registry)
        return 0;
    int n = 0;

    {
        ToolDefinition def;
        def.name        = "read_lines";
        def.description = "Read a specific line range from a source file";
        def.category    = ToolCategory::FileSystem;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::ReadLines(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "search_files";
        def.description = "Search for files matching a glob or name pattern in the workspace";
        def.category    = ToolCategory::FileSystem;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::SearchFiles(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "read_file";
        def.description = "Read the full contents of a file (capped at 10 MB)";
        def.category    = ToolCategory::FileSystem;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::ToolReadFile(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "list_dir";
        def.description = "List the contents of a directory";
        def.category    = ToolCategory::FileSystem;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::ListDir(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "write_file";
        def.description = "Write or create a file atomically (with backup)";
        def.category    = ToolCategory::FileSystem;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::WriteFile(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "search_code";
        def.description = "Recursively search for a regex pattern in source files";
        def.category    = ToolCategory::FileSystem;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::SearchCode(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    std::cout << "[REGISTRY] registerFileSystemTools: " << n << " tools\n";
    return n;
}

// ============================================================================
// registerBuildTestTools — plan_code_exploration,
//                           evaluate_integration_audit_feasibility
// ============================================================================
int registerBuildTestTools(ToolRegistry* registry)
{
    if (!registry)
        return 0;
    int n = 0;

    {
        ToolDefinition def;
        def.name        = "plan_code_exploration";
        def.description = "Generate a structured, targeted code exploration plan for a given goal";
        def.category    = ToolCategory::Analysis;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::PlanCodeExploration(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "evaluate_integration_audit_feasibility";
        def.description = "Assess whether a workspace can be fully audited in one pass";
        def.category    = ToolCategory::Analysis;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(
                RawrXD::Agent::AgentToolHandlers::EvaluateIntegrationAuditFeasibility(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "restore_checkpoint";
        def.description = "Restore agent workflow state from a saved checkpoint file";
        def.category    = ToolCategory::Analysis;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::RestoreCheckpoint(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "plan_tasks";
        def.description = "Break a high-level goal into ordered actionable tasks";
        def.category    = ToolCategory::Analysis;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::PlanTasks(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    std::cout << "[REGISTRY] registerBuildTestTools: " << n << " tools\n";
    return n;
}

// ============================================================================
// registerVersionControlTools — git_status (the only implemented VCS tool)
// ============================================================================
int registerVersionControlTools(ToolRegistry* registry)
{
    if (!registry)
        return 0;
    int n = 0;

    {
        ToolDefinition def;
        def.name        = "git_status";
        def.description = "Run git status in the workspace root and return the output";
        def.category    = ToolCategory::VersionControl;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::GitStatus(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    std::cout << "[REGISTRY] registerVersionControlTools: " << n << " tools\n";
    return n;
}

// ============================================================================
// registerExecutionTools — execute_command, run_shell
// ============================================================================
int registerExecutionTools(ToolRegistry* registry)
{
    if (!registry)
        return 0;
    int n = 0;

    {
        ToolDefinition def;
        def.name        = "execute_command";
        def.description = "Execute a shell command with timeout and output capture";
        def.category    = ToolCategory::Execution;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::ExecuteCommand(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    {
        ToolDefinition def;
        def.name        = "run_shell";
        def.description = "Run an allowlisted shell command (sandbox-tier enforced)";
        def.category    = ToolCategory::Execution;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::RunShell(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    std::cout << "[REGISTRY] registerExecutionTools: " << n << " tools\n";
    return n;
}

// ============================================================================
// registerModelTools — semantic_search (uses TF-IDF local index)
// ============================================================================
int registerModelTools(ToolRegistry* registry)
{
    if (!registry)
        return 0;
    int n = 0;

    {
        ToolDefinition def;
        def.name        = "semantic_search";
        def.description = "TF-IDF-based semantic search across the local workspace";
        def.category    = ToolCategory::Analysis;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::SemanticSearch(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    std::cout << "[REGISTRY] registerModelTools: " << n << " tools\n";
    return n;
}

// ============================================================================
// registerDeploymentTools — get_diagnostics
// ============================================================================
int registerDeploymentTools(ToolRegistry* registry)
{
    if (!registry)
        return 0;
    int n = 0;

    {
        ToolDefinition def;
        def.name        = "get_diagnostics";
        def.description = "Retrieve compiler or LSP diagnostics for a file";
        def.category    = ToolCategory::Build;
        def.handler     = [](const nlohmann::json& params) -> nlohmann::json
        {
            return toolResultToJson(RawrXD::Agent::AgentToolHandlers::GetDiagnostics(params));
        };
        if (registry->registerTool(def))
            ++n;
    }

    std::cout << "[REGISTRY] registerDeploymentTools: " << n << " tools\n";
    return n;
}

// ============================================================================
// register_rawr_inference — Registers the RAWR inference tool with ToolRegistry.
// This tool dispatches prompts to the loaded GGUF model via EngineRegistry.
// ============================================================================
void register_rawr_inference() {
    ToolRegistry::register_tool("rawr_inference", [](const std::string& input) -> std::string {
        // Build an AgentRequest from the raw input
        AgentRequest req{};
        req.mode = 0;              // standard inference
        req.prompt = input;
        req.deep_thinking = false;
        req.deep_research = false;
        req.no_refusal = false;
        req.context_limit = 4096;

        // Try to route through EngineRegistry — pick the first available engine
        Engine* engine = EngineRegistry::get("default");
        if (!engine) engine = EngineRegistry::get("cpu");
        if (!engine) engine = EngineRegistry::get("sovereign_small");

        if (engine) {
            std::string result = engine->infer(req);
            if (!result.empty()) {
                return result;
            }
            return "[rawr_inference] Engine returned empty response";
        }

        return "[rawr_inference] No inference engine available — load a GGUF model first";
    });

    std::cout << "[REGISTRY] Registered RAWR inference tool (routes to EngineRegistry)\n";
}

// ============================================================================
// register_sovereign_engines — Registers Engine800B + SovereignSmall with EngineRegistry.
// Linker fallback: when the real engine module is not linked, this provides
// a diagnostic stub that reports the missing linkage.
// ============================================================================
void register_sovereign_engines() {
    // Check if engines are already registered (real module may have beaten us)
    Engine* existing = EngineRegistry::get("engine_800b");
    if (existing) {
        std::cout << "[REGISTRY] Engine800B already registered by engine module\n";
        return;
    }

    existing = EngineRegistry::get("sovereign_small");
    if (existing) {
        std::cout << "[REGISTRY] SovereignSmall already registered by engine module\n";
        return;
    }

    // Real engine module not linked for Engine800B / SovereignSmall.
    // Register tools that route through whatever engine IS available,
    // but report the missing linkage when absolutely nothing is found.
    ToolRegistry::register_tool("engine_800b", [](const std::string& input) -> std::string {
        // Try to find engine_800b first, then fall back to any available engine
        Engine* e = EngineRegistry::get("engine_800b");
        if (!e) e = EngineRegistry::get("default");
        if (!e) e = EngineRegistry::get("cpu");
        if (e) {
            AgentRequest req{};
            req.mode = 0;
            req.prompt = input;
            req.context_limit = 4096;
            std::string r = e->infer(req);
            return r.empty() ? "[engine_800b] (empty response from fallback engine)" : r;
        }
        return "[engine_800b] Not available — engine module not linked and no fallback engine registered. "
               "Rebuild with -DRAWR_ENGINE_MODULE=ON to enable Engine800B.";
    });

    ToolRegistry::register_tool("sovereign_small", [](const std::string& input) -> std::string {
        Engine* e = EngineRegistry::get("sovereign_small");
        if (!e) e = EngineRegistry::get("default");
        if (!e) e = EngineRegistry::get("cpu");
        if (e) {
            AgentRequest req{};
            req.mode = 0;
            req.prompt = input;
            req.context_limit = 4096;
            std::string r = e->infer(req);
            return r.empty() ? "[sovereign_small] (empty response from fallback engine)" : r;
        }
        return "[sovereign_small] Not available — engine module not linked and no fallback engine registered. "
               "Rebuild with -DRAWR_ENGINE_MODULE=ON to enable SovereignSmall.";
    });

    std::cout << "[REGISTRY] register_sovereign_engines — engine module not linked, "
                 "diagnostic tools registered\n";
}
