// =============================================================================
// test_orchestrator_modules.cpp — Regression Tests for Orchestrator Modules
// =============================================================================
// Black-box behavioral tests per tools.instructions.md § Comprehensive Testing.
// Tests the X-Macro ToolRegistry, FIMPromptBuilder, AgentOllamaClient config,
// AgentOrchestrator session management, and DiskRecoveryAgent C++ wrapper.
//
// Build:
//   cl /std:c++17 /EHsc /W4 /I<src> /I<include> test_orchestrator_modules.cpp
//      ToolRegistry.obj FIMPromptBuilder.obj AgentOllamaClient.obj
//      AgentOrchestrator.obj DiskRecoveryAgent.obj OrchestratorBridge.obj
//      /link winhttp.lib kernel32.lib user32.lib advapi32.lib
//
// Pattern: assert-based with structured pass/fail output.
// =============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstdint>
#include <chrono>
#include <sstream>
#include <fstream>
#include <functional>
#include <filesystem>

#include "logging/logger.h"
static Logger s_logger("test_orchestrator_modules");

// Module headers under test
#include "ToolRegistry.h"
#include "FIMPromptBuilder.h"
#include "AgentOllamaClient.h"
#include "AgentOrchestrator.h"
#include "DiskRecoveryAgent.h"

using namespace RawrXD::Agent;
using namespace RawrXD::Recovery;

// ---------------------------------------------------------------------------
// Test framework (minimal — no external dependencies)
// ---------------------------------------------------------------------------

static int g_testsPassed = 0;
static int g_testsFailed = 0;
static int g_testsSkipped = 0;

struct TestResult {
    bool passed;
    std::string name;
    std::string detail;
    double elapsed_ms;
};

static std::vector<TestResult> g_results;

#define TEST_ASSERT(cond, name) do { \
    if (!(cond)) { \
        s_logger.error( "  FAIL: " << name << " (" #cond ")" << std::endl; \
        g_testsFailed++; \
        g_results.push_back({false, name, #cond, 0}); \
        return; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, name) do { \
    if ((a) != (b)) { \
        s_logger.error( "  FAIL: " << name << " (expected=" << (b) << " got=" << (a) << ")" << std::endl; \
        g_testsFailed++; \
        g_results.push_back({false, name, "mismatch", 0}); \
        return; \
    } \
} while(0)

#define TEST_PASS(name) do { \
    s_logger.info("  PASS: "); \
    g_testsPassed++; \
    g_results.push_back({true, name, "", 0}); \
} while(0)

#define TEST_SKIP(name, reason) do { \
    s_logger.info("  SKIP: "); \
    g_testsSkipped++; \
} while(0)

// ==========================================================================
// § 1. ToolRegistry — X-Macro Enum & Schema Generation
// ==========================================================================

void test_xmacro_enum_count() {
    // The X-Macro should generate exactly 11 tools (including disk_recovery)
    auto count = static_cast<uint32_t>(ToolId::_COUNT);
    TEST_ASSERT(count == 11, "X-Macro generates 11 ToolId values");
    TEST_PASS("xmacro_enum_count");
    return true;
}

void test_registry_singleton() {
    auto& reg1 = AgentToolRegistry::Instance();
    auto& reg2 = AgentToolRegistry::Instance();
    TEST_ASSERT(&reg1 == &reg2, "Registry is a singleton");
    TEST_PASS("registry_singleton");
    return true;
}

void test_registry_list_tools() {
    auto& reg = AgentToolRegistry::Instance();
    auto tools = reg.ListTools();
    TEST_ASSERT(tools.size() == 11, "ListTools returns 11 tools");

    // Check specific tool names exist
    bool hasReadFile = false, hasDiskRecovery = false;
    for (const auto& t : tools) {
        if (t == "read_file") hasReadFile = true;
        if (t == "disk_recovery") hasDiskRecovery = true;
    return true;
}

    TEST_ASSERT(hasReadFile, "read_file tool registered");
    TEST_ASSERT(hasDiskRecovery, "disk_recovery tool registered");
    TEST_PASS("registry_list_tools");
    return true;
}

void test_registry_schemas_valid() {
    auto& reg = AgentToolRegistry::Instance();
    json schemas = reg.GetToolSchemas();
    TEST_ASSERT(schemas.size() == 11, "GetToolSchemas returns 11 entries");

    // Each schema should have type=function with function.name and function.parameters
    for (size_t i = 0; i < schemas.size(); ++i) {
        const auto& s = schemas[i];
        TEST_ASSERT(s.contains("type"), "Schema has type field");
        TEST_ASSERT(s.contains("function"), "Schema has function field");
        const auto& fn = s["function"];
        TEST_ASSERT(fn.contains("name"), "Function has name");
        TEST_ASSERT(fn.contains("description"), "Function has description");
        TEST_ASSERT(fn.contains("parameters"), "Function has parameters");
    return true;
}

    TEST_PASS("registry_schemas_valid");
    return true;
}

void test_registry_dispatch_read_file() {
    auto& reg = AgentToolRegistry::Instance();

    // Create a temp file for testing
    std::string tempPath = (std::filesystem::temp_directory_path() / "rawrxd_test_read.txt").string();
    {
        std::ofstream ofs(tempPath);
        ofs << "Hello, RawrXD!";
    return true;
}

    json args;
    args["path"] = tempPath;
    auto result = reg.Dispatch("read_file", args);
    TEST_ASSERT(result.success, "read_file dispatch succeeds");
    TEST_ASSERT(result.output == "Hello, RawrXD!", "read_file returns correct content");
    TEST_ASSERT(result.elapsed_ms >= 0, "elapsed_ms is non-negative");

    // Cleanup
    std::filesystem::remove(tempPath);
    TEST_PASS("registry_dispatch_read_file");
    return true;
}

void test_registry_dispatch_write_file() {
    auto& reg = AgentToolRegistry::Instance();

    std::string tempPath = (std::filesystem::temp_directory_path() / "rawrxd_test_write.txt").string();

    json args;
    args["path"] = tempPath;
    args["content"] = "Written by test";
    auto result = reg.Dispatch("write_file", args);
    TEST_ASSERT(result.success, "write_file dispatch succeeds");

    // Verify content
    std::ifstream ifs(tempPath);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    TEST_ASSERT(content == "Written by test", "write_file content is correct");

    std::filesystem::remove(tempPath);
    TEST_PASS("registry_dispatch_write_file");
    return true;
}

void test_registry_dispatch_unknown_tool() {
    auto& reg = AgentToolRegistry::Instance();

    json args;
    auto result = reg.Dispatch("nonexistent_tool", args);
    TEST_ASSERT(!result.success, "Unknown tool returns failure");
    TEST_ASSERT(result.output.find("Unknown tool") != std::string::npos, "Error mentions unknown");
    TEST_PASS("registry_dispatch_unknown_tool");
    return true;
}

void test_registry_validation_missing_required() {
    auto& reg = AgentToolRegistry::Instance();

    json emptyArgs;
    auto result = reg.Dispatch("read_file", emptyArgs);
    TEST_ASSERT(!result.success, "Missing required param returns failure");
    TEST_ASSERT(result.output.find("path") != std::string::npos ||
                result.output.find("Validation") != std::string::npos,
                "Error mentions the missing parameter");
    TEST_PASS("registry_validation_missing_required");
    return true;
}

void test_registry_stats() {
    auto& reg = AgentToolRegistry::Instance();

    uint64_t invocations = reg.GetTotalInvocations();
    uint64_t errors = reg.GetTotalErrors();
    // After previous tests, should have some invocations
    TEST_ASSERT(invocations > 0, "Invocation count > 0 after dispatches");
    // We had at least one error (unknown tool, validation fail)
    TEST_ASSERT(errors >= 1, "Error count tracks failures");
    TEST_PASS("registry_stats");
    return true;
}

void test_registry_system_prompt() {
    auto& reg = AgentToolRegistry::Instance();

    std::string prompt = reg.GetSystemPrompt("d:\\rawrxd", {"main.cpp", "ToolRegistry.h"});
    TEST_ASSERT(!prompt.empty(), "System prompt is non-empty");
    TEST_ASSERT(prompt.find("RawrXD Agent") != std::string::npos, "Prompt identifies as RawrXD Agent");
    TEST_ASSERT(prompt.find("d:\\rawrxd") != std::string::npos, "Prompt includes CWD");
    TEST_ASSERT(prompt.find("main.cpp") != std::string::npos, "Prompt lists open files");
    TEST_PASS("registry_system_prompt");
    return true;
}

// ==========================================================================
// § 2. FIMPromptBuilder — Fill-in-Middle Prompt Construction
// ==========================================================================

void test_fim_build_basic() {
    FIMPromptBuilder builder;
    builder.SetFormat(FIMFormat::Qwen);
    builder.SetMaxContextTokens(4096);

    EditorContext ctx;
    ctx.filename = "test.cpp";
    ctx.filepath = "d:\\project\\test.cpp";
    ctx.language = "cpp";
    ctx.cursor_line = 2;
    ctx.cursor_column = 0;
    ctx.full_content = "#include <iostream>\n\nint main() { return 0; }\n";

    auto result = builder.Build(ctx);
    TEST_ASSERT(result.success, "FIM build succeeds");
    TEST_ASSERT(!result.prompt.formatted_prompt.empty(), "Formatted prompt is non-empty");
    TEST_ASSERT(result.prompt.estimated_tokens > 0, "Token estimate > 0");
    TEST_PASS("fim_build_basic");
    return true;
}

void test_fim_build_empty_content() {
    FIMPromptBuilder builder;
    EditorContext ctx;
    ctx.full_content = "";
    ctx.cursor_line = 0;

    auto result = builder.Build(ctx);
    TEST_ASSERT(!result.success, "Empty content returns failure");
    TEST_ASSERT(result.error.find("Empty") != std::string::npos, "Error mentions empty");
    TEST_PASS("fim_build_empty_content");
    return true;
}

void test_fim_build_invalid_cursor() {
    FIMPromptBuilder builder;
    EditorContext ctx;
    ctx.full_content = "some code";
    ctx.cursor_line = -5;

    auto result = builder.Build(ctx);
    TEST_ASSERT(!result.success, "Negative cursor returns failure");
    TEST_PASS("fim_build_invalid_cursor");
    return true;
}

void test_fim_qwen_format_tokens() {
    FIMPromptBuilder builder;
    builder.SetFormat(FIMFormat::Qwen);
    builder.SetMaxContextTokens(2048);

    EditorContext ctx;
    ctx.filename = "example.py";
    ctx.filepath = "example.py";
    ctx.language = "python";
    ctx.cursor_line = 1;
    ctx.cursor_column = 0;
    ctx.full_content = "def hello():\n    pass\n";

    auto result = builder.Build(ctx);
    TEST_ASSERT(result.success, "Qwen FIM build succeeds");
    // Qwen uses <|fim_prefix|> / <|fim_suffix|> / <|fim_middle|>
    auto& fp = result.prompt.formatted_prompt;
    TEST_ASSERT(fp.find("fim_prefix") != std::string::npos ||
                fp.find("fim_suffix") != std::string::npos ||
                fp.find("prefix") != std::string::npos,
                "Formatted prompt contains FIM tokens");
    TEST_PASS("fim_qwen_format_tokens");
    return true;
}

void test_fim_build_from_parts() {
    FIMPromptBuilder builder;
    builder.SetFormat(FIMFormat::Qwen);
    builder.SetMaxContextTokens(1024);

    auto result = builder.BuildFromParts("int x = ", ";", "test.cpp");
    TEST_ASSERT(result.success, "BuildFromParts succeeds");
    TEST_ASSERT(result.prompt.prefix_lines >= 1, "Has prefix lines");
    TEST_PASS("fim_build_from_parts");
    return true;
}

void test_fim_prefix_ratio() {
    FIMPromptBuilder builder;
    builder.SetPrefixRatio(0.8f);
    builder.SetMaxContextTokens(100);

    // Create content large enough to require trimming
    std::string bigContent;
    for (int i = 0; i < 200; i++) bigContent += "line " + std::to_string(i) + "\n";

    EditorContext ctx;
    ctx.filename = "big.txt";
    ctx.filepath = "big.txt";
    ctx.cursor_line = 100;
    ctx.cursor_column = 0;
    ctx.full_content = bigContent;

    auto result = builder.Build(ctx);
    TEST_ASSERT(result.success, "Large content FIM build succeeds");
    TEST_ASSERT(result.prompt.estimated_tokens <= 120, "Token count is reasonably bounded");
    TEST_PASS("fim_prefix_ratio");
    return true;
}

// ==========================================================================
// § 3. AgentOllamaClient — Config Validation (no server needed)
// ==========================================================================

void test_ollama_config_defaults() {
    OllamaConfig cfg;
    TEST_ASSERT(cfg.host == "127.0.0.1", "Default host is localhost");
    TEST_ASSERT(cfg.port == 11434, "Default port is 11434");
    TEST_ASSERT(!cfg.chat_model.empty(), "Default chat model is set");
    TEST_ASSERT(!cfg.fim_model.empty(), "Default FIM model is set");
    TEST_ASSERT(cfg.timeout_ms > 0, "Default timeout > 0");
    TEST_PASS("ollama_config_defaults");
    return true;
}

void test_ollama_client_construction() {
    OllamaConfig cfg;
    cfg.host = "127.0.0.1";
    cfg.port = 11434;

    // Construction should not throw or crash
    AgentOllamaClient client(cfg);
    TEST_PASS("ollama_client_construction");
    return true;
}

void test_ollama_cancel_before_stream() {
    OllamaConfig cfg;
    AgentOllamaClient client(cfg);

    // Cancel when nothing is running should be safe
    client.CancelStream();
    TEST_PASS("ollama_cancel_before_stream");
    return true;
}

// ==========================================================================
// § 4. AgentOrchestrator — Session Management
// ==========================================================================

void test_orchestrator_construction() {
    // Should create without crashing
    AgentOrchestrator orch;
    TEST_PASS("orchestrator_construction");
    return true;
}

void test_orchestrator_config() {
    AgentOrchestrator orch;
    OrchestratorConfig cfg;
    cfg.max_tool_rounds = 20;
    cfg.max_conversation_tokens = 16000;
    cfg.working_directory = "d:\\rawrxd";
    cfg.auto_build_after_edit = true;
    cfg.auto_diagnostics = true;

    orch.SetConfig(cfg);
    TEST_PASS("orchestrator_config");
    return true;
}

void test_orchestrator_cancel() {
    AgentOrchestrator orch;

    // Cancel when nothing is running should be safe
    orch.Cancel();
    TEST_PASS("orchestrator_cancel");
    return true;
}

// ==========================================================================
// § 5. DiskRecoveryAgent — C++ Wrapper (no hardware required)
// ==========================================================================

void test_recovery_agent_construction() {
    DiskRecoveryAgent agent;
    TEST_ASSERT(!agent.IsInitialized(), "Agent starts uninitialized");
    TEST_ASSERT(!agent.IsKeyExtracted(), "No key extracted on construction");
    TEST_ASSERT(agent.GetBridgeType() == BridgeType::Unknown, "Bridge type is Unknown");
    TEST_PASS("recovery_agent_construction");
    return true;
}

void test_recovery_agent_stats_uninitialized() {
    DiskRecoveryAgent agent;
    auto stats = agent.GetStats();
    TEST_ASSERT(stats.goodSectors == 0, "Zero good sectors when uninitialized");
    TEST_ASSERT(stats.badSectors == 0, "Zero bad sectors when uninitialized");
    TEST_ASSERT(stats.currentLBA == 0, "Zero current LBA when uninitialized");
    TEST_ASSERT(stats.totalSectors == 0, "Zero total sectors when uninitialized");
    TEST_PASS("recovery_agent_stats_uninitialized");
    return true;
}

void test_recovery_agent_abort_safe() {
    DiskRecoveryAgent agent;
    // Abort on uninitialized agent should be safe (no-op)
    agent.Abort();
    TEST_PASS("recovery_agent_abort_safe");
    return true;
}

void test_recovery_agent_move_semantics() {
    DiskRecoveryAgent agent1;
    DiskRecoveryAgent agent2 = std::move(agent1);
    TEST_ASSERT(!agent2.IsInitialized(), "Moved-to agent has same state");
    TEST_PASS("recovery_agent_move_semantics");
    return true;
}

void test_recovery_stats_progress() {
    RecoveryStats stats;
    stats.goodSectors = 100;
    stats.badSectors = 5;
    stats.currentLBA = 500;
    stats.totalSectors = 1000;

    double pct = stats.ProgressPercent();
    TEST_ASSERT(pct >= 49.9 && pct <= 50.1, "ProgressPercent computes correctly");

    RecoveryStats empty;
    empty.totalSectors = 0;
    empty.currentLBA = 0;
    TEST_ASSERT(empty.ProgressPercent() == 0.0, "Zero totalSectors gives 0%");
    TEST_PASS("recovery_stats_progress");
    return true;
}

void test_recovery_result_factories() {
    auto ok = RecoveryResult::ok("Success");
    TEST_ASSERT(ok.success, "ok() returns success=true");
    TEST_ASSERT(ok.detail == "Success", "ok() detail matches");
    TEST_ASSERT(ok.errorCode == 0, "ok() errorCode is 0");

    auto err = RecoveryResult::error("Failed", 42);
    TEST_ASSERT(!err.success, "error() returns success=false");
    TEST_ASSERT(err.errorCode == 42, "error() preserves errorCode");
    TEST_PASS("recovery_result_factories");
    return true;
}

// ==========================================================================
// § 6. ToolExecResult — Factory Pattern Validation
// ==========================================================================

void test_tool_exec_result_ok() {
    auto r = ToolExecResult::ok("output data", 12.5);
    TEST_ASSERT(r.success, "ok() is successful");
    TEST_ASSERT(r.output == "output data", "ok() preserves output");
    TEST_ASSERT(r.exit_code == 0, "ok() exit_code is 0");
    TEST_ASSERT(r.elapsed_ms == 12.5, "ok() preserves elapsed_ms");
    TEST_PASS("tool_exec_result_ok");
    return true;
}

void test_tool_exec_result_error() {
    auto r = ToolExecResult::error("bad things", -3);
    TEST_ASSERT(!r.success, "error() is failure");
    TEST_ASSERT(r.output == "bad things", "error() preserves message");
    TEST_ASSERT(r.exit_code == -3, "error() preserves exit code");
    TEST_PASS("tool_exec_result_error");
    return true;
}

// ==========================================================================
// § 7. Disk Recovery Tool Integration (via ToolRegistry dispatch)
// ==========================================================================

void test_disk_recovery_tool_stats_action() {
    auto& reg = AgentToolRegistry::Instance();

    json args;
    args["action"] = "stats";
    auto result = reg.Dispatch("disk_recovery", args);
    // Should succeed even without hardware (returns zeroed stats)
    TEST_ASSERT(result.success, "disk_recovery stats succeeds without hardware");
    TEST_ASSERT(result.output.find("Good:") != std::string::npos, "Stats output has Good field");
    TEST_PASS("disk_recovery_tool_stats_action");
    return true;
}

void test_disk_recovery_tool_invalid_action() {
    auto& reg = AgentToolRegistry::Instance();

    json args;
    args["action"] = "invalid_blah";
    auto result = reg.Dispatch("disk_recovery", args);
    TEST_ASSERT(!result.success, "Invalid action returns failure");
    TEST_ASSERT(result.output.find("Unknown action") != std::string::npos, "Error mentions unknown");
    TEST_PASS("disk_recovery_tool_invalid_action");
    return true;
}

void test_disk_recovery_tool_missing_action() {
    auto& reg = AgentToolRegistry::Instance();

    json args;
    auto result = reg.Dispatch("disk_recovery", args);
    TEST_ASSERT(!result.success, "Missing action returns failure");
    TEST_PASS("disk_recovery_tool_missing_action");
    return true;
}

// ==========================================================================
// Test Runner
// ==========================================================================

int main() {
    s_logger.info("========================================");
    s_logger.info("  RawrXD Orchestrator Module Tests");
    s_logger.info("  Regression Suite v1.0");
    s_logger.info("========================================");

    auto startTime = std::chrono::high_resolution_clock::now();

    // § 1. ToolRegistry
    s_logger.info("\n--- ToolRegistry Tests ---");
    test_xmacro_enum_count();
    test_registry_singleton();
    test_registry_list_tools();
    test_registry_schemas_valid();
    test_registry_dispatch_read_file();
    test_registry_dispatch_write_file();
    test_registry_dispatch_unknown_tool();
    test_registry_validation_missing_required();
    test_registry_stats();
    test_registry_system_prompt();

    // § 2. FIMPromptBuilder
    s_logger.info("\n--- FIMPromptBuilder Tests ---");
    test_fim_build_basic();
    test_fim_build_empty_content();
    test_fim_build_invalid_cursor();
    test_fim_qwen_format_tokens();
    test_fim_build_from_parts();
    test_fim_prefix_ratio();

    // § 3. AgentOllamaClient
    s_logger.info("\n--- AgentOllamaClient Tests ---");
    test_ollama_config_defaults();
    test_ollama_client_construction();
    test_ollama_cancel_before_stream();

    // § 4. AgentOrchestrator
    s_logger.info("\n--- AgentOrchestrator Tests ---");
    test_orchestrator_construction();
    test_orchestrator_config();
    test_orchestrator_cancel();

    // § 5. DiskRecoveryAgent
    s_logger.info("\n--- DiskRecoveryAgent Tests ---");
    test_recovery_agent_construction();
    test_recovery_agent_stats_uninitialized();
    test_recovery_agent_abort_safe();
    test_recovery_agent_move_semantics();
    test_recovery_stats_progress();
    test_recovery_result_factories();

    // § 6. ToolExecResult
    s_logger.info("\n--- ToolExecResult Tests ---");
    test_tool_exec_result_ok();
    test_tool_exec_result_error();

    // § 7. Disk Recovery Tool Integration
    s_logger.info("\n--- Disk Recovery Tool Integration ---");
    test_disk_recovery_tool_stats_action();
    test_disk_recovery_tool_invalid_action();
    test_disk_recovery_tool_missing_action();

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // Summary
    s_logger.info("\n========================================");
    s_logger.info("  Results: ");
    s_logger.info("  Elapsed: ");
    s_logger.info("========================================");

    return g_testsFailed > 0 ? 1 : 0;
    return true;
}

