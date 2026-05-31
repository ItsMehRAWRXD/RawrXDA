// ============================================================================
// escalate_to_swarm_verification.cpp — Smoke test for structural escape hatch
// ============================================================================
// This test validates the escalate_to_swarm tool dispatch, schema registration,
// and orchestrator handoff payload structure.
// ============================================================================

#include "../src/agentic/AgentToolHandlers.h"
#include <iostream>
#include <cassert>

using json = nlohmann::json;
using RawrXD::Agent::AgentToolHandlers;
using RawrXD::Agent::ToolCallResult;

int main()
{
    std::cout << "=== Escalate-To-Swarm Verification ===" << std::endl;

    // ========================================================================
    // TEST 1: Tool Registration in Dispatch Table
    // ========================================================================
    std::cout << "\n[TEST 1] Dispatch table registration...\n";
    
    AgentToolHandlers& handlers = AgentToolHandlers::Instance();
    
    // Check all three aliases are registered
    assert(handlers.HasTool("escalate_to_swarm") && "Primary name not found");
    assert(handlers.HasTool("model_escalate") && "Alias: model_escalate not found");
    assert(handlers.HasTool("escalate_model_switch") && "Alias: escalate_model_switch not found");
    
    std::cout << "✓ All three dispatch aliases registered:\n"
              << "  - escalate_to_swarm\n"
              << "  - model_escalate\n"
              << "  - escalate_model_switch\n";

    // ========================================================================
    // TEST 2: Schema Presence in GetAllSchemas()
    // ========================================================================
    std::cout << "\n[TEST 2] Schema registration in GetAllSchemas()...\n";
    
    const json schemas = AgentToolHandlers::GetAllSchemas();
    assert(schemas.is_array() && "Schemas array missing");
    
    bool foundSchema = false;
    for (const auto& tool : schemas)
    {
        if (tool.contains("function") && tool["function"].contains("name"))
        {
            const std::string name = tool["function"]["name"].get<std::string>();
            if (name == "escalate_to_swarm")
            {
                foundSchema = true;
                const auto& desc = tool["function"]["description"].get<std::string>();
                assert(!desc.empty() && "Schema description missing");
                assert(desc.find("escape hatch") != std::string::npos && "Description lacks key phrase");
                
                // Validate schema structure
                assert(tool["function"].contains("parameters") && "Parameters missing");
                const auto& params = tool["function"]["parameters"];
                assert(params.contains("properties") && "Properties missing");
                assert(params.contains("required") && "Required array missing");
                
                // Check required fields
                const auto& required = params["required"];
                assert(required.is_array() && "Required must be array");
                bool hasReason = false, hasTask = false;
                for (const auto& r : required)
                {
                    if (r.is_string())
                    {
                        const std::string field = r.get<std::string>();
                        if (field == "reason") hasReason = true;
                        if (field == "task") hasTask = true;
                    }
                }
                assert(hasReason && "reason not in required array");
                assert(hasTask && "task not in required array");
                
                // Check parameter properties
                const auto& props = params["properties"];
                assert(props.contains("reason") && "reason property missing");
                assert(props.contains("task") && "task property missing");
                assert(props.contains("preferred_model") && "preferred_model property missing");
                assert(props.contains("subtask_count") && "subtask_count property missing");
                
                std::cout << "✓ Schema found with correct structure:\n"
                          << "  - Description: OK\n"
                          << "  - Required fields: reason, task\n"
                          << "  - Optional fields: preferred_model, subtask_count\n";
            }
        }
    }
    assert(foundSchema && "escalate_to_swarm schema not in GetAllSchemas()");

    // ========================================================================
    // TEST 3: Tool Execution - Bottleneck Scenario
    // ========================================================================
    std::cout << "\n[TEST 3] Bottleneck scenario execution...\n";
    
    json args = json::object();
    args["reason"] = "Architecture mismatch: Current 3B model cannot handle assembly-level reasoning";
    args["task"] = "Reverse engineer x64 calling convention from disassembly dump";
    args["preferred_model"] = "codestral-22b";
    args["subtask_count"] = 2;
    
    const ToolCallResult result = handlers.Execute("escalate_to_swarm", args);
    
    // Validate result structure
    assert(result.outcome == RawrXD::Agent::ToolOutcome::Success && "Execution failed");
    assert(!result.output.empty() && "Output (session ID) missing");
    assert(result.metadata.is_object() && "Metadata missing");
    
    // Validate metadata payload
    const auto& meta = result.metadata;
    assert(meta.contains("status") && meta["status"] == "escalated" && "Status not escalated");
    assert(meta.contains("session_id") && "session_id missing");
    assert(meta.contains("reason") && meta["reason"] == args["reason"] && "reason mismatch");
    assert(meta.contains("task") && meta["task"] == args["task"] && "task mismatch");
    assert(meta.contains("preferred_model") && meta["preferred_model"] == "codestral-22b" && "preferred_model mismatch");
        assert(meta.contains("subtask_count") && meta["subtask_count"].is_number_integer() &&
            meta["subtask_count"].get<int>() == 2 && "subtask_count mismatch");
    assert(meta.contains("timestamp") && "timestamp missing");
    
    std::cout << "✓ Bottleneck execution successful:\n"
              << "  - Status: " << meta["status"].get<std::string>() << "\n"
              << "  - Session ID: " << meta["session_id"].get<std::string>() << "\n"
              << "  - Reason: " << meta["reason"].get<std::string>() << "\n"
              << "  - Task: " << meta["task"].get<std::string>() << "\n"
              << "  - Preferred Model: " << meta["preferred_model"].get<std::string>() << "\n"
              << "  - Subtask Count: " << meta["subtask_count"].get<int>() << "\n";

    // ========================================================================
    // TEST 4: Validation - Missing Required Fields
    // ========================================================================
    std::cout << "\n[TEST 4] Validation: missing required fields...\n";
    
    json badArgs = json::object();
    badArgs["reason"] = "Test reason";
    // Missing task
    
    const ToolCallResult badResult = handlers.Execute("escalate_to_swarm", badArgs);
    assert(badResult.outcome != RawrXD::Agent::ToolOutcome::Success && "Should reject missing task");
    assert(badResult.output.find("requires non-empty") != std::string::npos && "Wrong error message");
    
    std::cout << "✓ Validation correctly rejects missing 'task' field\n";

    // ========================================================================
    // TEST 5: Validation - Subtask Count Bounds
    // ========================================================================
    std::cout << "\n[TEST 5] Validation: subtask_count bounds...\n";
    
    json boundsArgs = json::object();
    boundsArgs["reason"] = "Test";
    boundsArgs["task"] = "Test task";
    boundsArgs["subtask_count"] = 25;  // Out of range (> 16)
    
    const ToolCallResult boundsResult = handlers.Execute("escalate_to_swarm", boundsArgs);
    assert(boundsResult.outcome != RawrXD::Agent::ToolOutcome::Success && "Should reject out-of-bounds subtask_count");
    assert(boundsResult.output.find("between 1 and 16") != std::string::npos && "Wrong bounds error");
    
    std::cout << "✓ Validation correctly enforces subtask_count bounds (1-16)\n";

    // ========================================================================
    // TEST 6: Alias Dispatch Equivalence
    // ========================================================================
    std::cout << "\n[TEST 6] Alias dispatch equivalence...\n";
    
    json testArgs = json::object();
    testArgs["reason"] = "Alias test";
    testArgs["task"] = "Test escalation";
    testArgs["preferred_model"] = "gpt-4o";
    
    const ToolCallResult primary = handlers.Execute("escalate_to_swarm", testArgs);
    const ToolCallResult alias1 = handlers.Execute("model_escalate", testArgs);
    const ToolCallResult alias2 = handlers.Execute("escalate_model_switch", testArgs);
    
    assert(primary.outcome == RawrXD::Agent::ToolOutcome::Success && "Primary failed");
    assert(alias1.outcome == RawrXD::Agent::ToolOutcome::Success && "Alias 1 failed");
    assert(alias2.outcome == RawrXD::Agent::ToolOutcome::Success && "Alias 2 failed");
    
    // All three should produce same structure (though different session IDs)
    assert(primary.metadata["status"] == alias1.metadata["status"] && "Status mismatch");
    assert(primary.metadata["preferred_model"] == alias2.metadata["preferred_model"] && "Model mismatch");
    
    std::cout << "✓ All three aliases produce equivalent output structure\n";

    // ========================================================================
    // SUMMARY
    // ========================================================================
    std::cout << "\n" << std::string(60, '=') << "\n"
              << "✓✓✓ ALL TESTS PASSED ✓✓✓\n"
              << "Structural escape hatch is ready for deployment.\n"
              << "Orchestrator can now observe status=escalated and route swarm.\n"
              << std::string(60, '=') << "\n\n";

    return 0;
}
