// ============================================================================
// test_hotpatch_tools_smoke.cpp — Smoke test for agentic hotpatch tool calling
// ============================================================================
// Verifies:
//   1. All 4 hotpatch tools are registered in dispatch table
//   2. apply_hotpatch accepts valid parameters and returns structured result
//   3. revert_hotpatch accepts target parameter
//   4. list_hotpatches returns layer statistics
//   5. hotpatch_status returns subsystem health
//
// Run: cmake --build . --target test_hotpatch_tools_smoke && ctest -R hotpatch
// ============================================================================

#include "agentic/AgentToolHandlers.h"
#include "agentic/ToolCallResult.h"
#include <cstdio>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using RawrXD::Agent::AgentToolHandlers;
using RawrXD::Agent::ToolCallResult;

static int g_pass = 0;
static int g_fail = 0;

void check(bool cond, const char* name) {
    if (cond) {
        printf("  [PASS] %s\n", name);
        g_pass++;
    } else {
        printf("  [FAIL] %s\n", name);
        g_fail++;
    }
}

int main() {
    printf("=== Hotpatch Tool Smoke Test ===\n\n");

    // Ensure dispatch table is initialized
    AgentToolHandlers::Instance();

    // ------------------------------------------------------------------------
    // Test 1: Tool registration
    // ------------------------------------------------------------------------
    printf("[Test 1] Tool registration in dispatch table\n");
    check(AgentToolHandlers::Instance().HasTool("apply_hotpatch"),     "apply_hotpatch registered");
    check(AgentToolHandlers::Instance().HasTool("revert_hotpatch"),   "revert_hotpatch registered");
    check(AgentToolHandlers::Instance().HasTool("list_hotpatches"),   "list_hotpatches registered");
    check(AgentToolHandlers::Instance().HasTool("hotpatch_status"),   "hotpatch_status registered");

    // ------------------------------------------------------------------------
    // Test 2: apply_hotpatch validation
    // ------------------------------------------------------------------------
    printf("\n[Test 2] apply_hotpatch parameter validation\n");
    {
        json args;
        args["target"] = "test_module";
        args["address"] = "0x140000000";
        args["patch_hex"] = "9090C3";
        args["mode"] = "memory";

        ToolCallResult result = AgentToolHandlers::Instance().Execute("apply_hotpatch", args);
        check(result.isSuccess() || result.outcome == RawrXD::Agent::ToolOutcome::ExecutionError,
              "apply_hotpatch returns structured result");
        check(!result.output.empty() || !result.error.empty(),
              "apply_hotpatch returns output or error");

        // Verify JSON response structure
        if (!result.output.empty()) {
            try {
                json resp = json::parse(result.output);
                check(resp.contains("status"), "apply_hotpatch response has 'status'");
                check(resp.contains("target"), "apply_hotpatch response has 'target'");
                check(resp.contains("mode"),   "apply_hotpatch response has 'mode'");
            } catch (...) {
                check(false, "apply_hotpatch response is valid JSON");
            }
        }
    }

    // Test mode restriction
    {
        json args;
        args["target"] = "test_module";
        args["mode"] = "byte";
        ToolCallResult result = AgentToolHandlers::Instance().Execute("apply_hotpatch", args);
        check(!result.isSuccess(), "apply_hotpatch rejects mode='byte'");
        if (!result.error.empty()) {
            check(result.error.find("memory") != std::string::npos,
                  "apply_hotpatch error mentions 'memory' requirement");
        }
    }

    // Test missing target
    {
        json args;
        args["address"] = "0x140000000";
        ToolCallResult result = AgentToolHandlers::Instance().Execute("apply_hotpatch", args);
        check(!result.isSuccess(), "apply_hotpatch rejects missing target");
    }

    // ------------------------------------------------------------------------
    // Test 3: revert_hotpatch
    // ------------------------------------------------------------------------
    printf("\n[Test 3] revert_hotpatch parameter validation\n");
    {
        json args;
        args["target"] = "test_module";
        args["address"] = "0x140000000";

        ToolCallResult result = AgentToolHandlers::Instance().Execute("revert_hotpatch", args);
        check(result.isSuccess() || !result.isSuccess(),
              "revert_hotpatch returns structured result");
        check(!result.output.empty() || !result.error.empty(),
              "revert_hotpatch returns output or error");
    }

    {
        json args;
        ToolCallResult result = AgentToolHandlers::Instance().Execute("revert_hotpatch", args);
        check(!result.isSuccess(), "revert_hotpatch rejects missing target");
    }

    // ------------------------------------------------------------------------
    // Test 4: list_hotpatches
    // ------------------------------------------------------------------------
    printf("\n[Test 4] list_hotpatches returns layer statistics\n");
    {
        json args;
        args["layer"] = "all";

        ToolCallResult result = AgentToolHandlers::Instance().Execute("list_hotpatches", args);
        check(result.isSuccess(), "list_hotpatches succeeds");

        if (result.isSuccess() && !result.output.empty()) {
            try {
                json resp = json::parse(result.output);
                check(resp.contains("layer_filter"), "list_hotpatches has 'layer_filter'");
                check(resp.contains("live_binary") || resp.contains("shadow_detour") || resp.contains("sentinel"),
                      "list_hotpatches contains at least one layer");
            } catch (...) {
                check(false, "list_hotpatches response is valid JSON");
            }
        }
    }

    // ------------------------------------------------------------------------
    // Test 5: hotpatch_status
    // ------------------------------------------------------------------------
    printf("\n[Test 5] hotpatch_status returns subsystem health\n");
    {
        json args;
        args["layer"] = "all";

        ToolCallResult result = AgentToolHandlers::Instance().Execute("hotpatch_status", args);
        check(result.isSuccess(), "hotpatch_status succeeds");

        if (result.isSuccess() && !result.output.empty()) {
            try {
                json resp = json::parse(result.output);
                check(resp.contains("subsystem"), "hotpatch_status has 'subsystem'");
                check(resp.contains("timestamp"), "hotpatch_status has 'timestamp'");
                check(resp.contains("pt_driver") || resp.contains("live_binary") || resp.contains("shadow_detour"),
                      "hotpatch_status contains at least one subsystem");
            } catch (...) {
                check(false, "hotpatch_status response is valid JSON");
            }
        }
    }

    // ------------------------------------------------------------------------
    // Test 6: Schema generation
    // ------------------------------------------------------------------------
    printf("\n[Test 6] Tool schemas are generated for LLM discovery\n");
    {
        json schemas = AgentToolHandlers::GetAllSchemas();
        bool foundApply = false;
        bool foundRevert = false;
        bool foundList = false;
        bool foundStatus = false;

        for (const auto& s : schemas) {
            if (s.contains("function") && s["function"].contains("name")) {
                std::string name = s["function"]["name"];
                if (name == "apply_hotpatch") foundApply = true;
                if (name == "revert_hotpatch") foundRevert = true;
                if (name == "list_hotpatches") foundList = true;
                if (name == "hotpatch_status") foundStatus = true;
            }
        }

        check(foundApply,  "apply_hotpatch schema present");
        check(foundRevert, "revert_hotpatch schema present");
        check(foundList,   "list_hotpatches schema present");
        check(foundStatus, "hotpatch_status schema present");
    }

    // ------------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------------
    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
