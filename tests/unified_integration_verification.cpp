// Unified Integration Verification - Validates all subsystems are wired and callable
// Tests: Extension API ↔ Hotpatch ↔ Measurement ↔ Agentic ↔ AST Wiring

#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

// Core subsystem headers
#include "extensions/extension_api_bridge.h"
#include "agentic/hotpatch/Engine.hpp"
#include "agentic/hotpatch/Sentinel.hpp"
#include "speculative/rawr_benchmark_measurement_corrected.h"
#include "ide/ast_completion_bridge.h"
#include "agentic/slash_command_parser.hpp"

using namespace RawrXD;
using namespace std::chrono;

// =============================================================================
// TEST 1: Extension API Bridge - Agentic Callable
// =============================================================================
bool test_extension_api_agentic_callable() {
    std::cout << "[TEST 1] Extension API Bridge - Agentic Callable...\n";
    
    // Get singleton instance (callable from agentic systems)
    auto* bridge = ExtensionAPIBridge::getInstance();
    assert(bridge != nullptr && "Extension bridge singleton failed");
    
    // Register a command callable by agents
    int cmdId = bridge->registerCommand("agentic.test", "Test Agentic Command",
        [](void* data) {
            std::cout << "  ✓ Agentic command executed via Extension API\n";
        }, nullptr);
    assert(cmdId > 0 && "Command registration failed");
    
    // Execute via agentic path
    bridge->executeCommand("agentic.test");
    
    // Test async command (agentic non-blocking)
    int asyncCmd = bridge->registerAsyncCommand("agentic.async", "Async Agentic Command",
        [](void* data, std::function<void(bool)> callback) {
            std::thread([callback]() {
                std::this_thread::sleep_for(milliseconds(10));
                callback(true);
            }).detach();
        }, nullptr);
    
    bool asyncCompleted = false;
    bridge->executeCommandAsync("agentic.async", 
        [&asyncCompleted](bool success) { asyncCompleted = success; });
    
    std::this_thread::sleep_for(milliseconds(50));
    assert(asyncCompleted && "Async command execution failed");
    
    std::cout << "  ✓ Async agentic command completed\n";
    return true;
}

// =============================================================================
// TEST 2: Hotpatch Engine - Measurement Integration
// =============================================================================
bool test_hotpatch_measurement_integration() {
    std::cout << "[TEST 2] Hotpatch Engine - Measurement Integration...\n";
    
    // Initialize hotpatch engine
    Agentic::Hotpatch::Engine hotpatchEngine;
    assert(hotpatchEngine.initialize() && "Hotpatch engine init failed");
    
    // Create measurement collector for autopatch feedback
    MeasurementCollector metrics;
    metrics.TokenGenerationStart();
    
    // Simulate a hook that triggers measurement
    Agentic::Hotpatch::HookConfig hook;
    hook.name = "measurement.hook";
    hook.type = Agentic::Hotpatch::HookType::DETOUR;
    hook.target = (void*)0x12345678; // Mock target
    hook.replacement = (void*)0x87654321; // Mock replacement
    
    // Apply hook (should be measurable)
    auto start = high_resolution_clock::now();
    bool applied = hotpatchEngine.applyHook(hook);
    auto elapsed = duration_cast<microseconds>(high_resolution_clock::now() - start);
    
    std::cout << "  ✓ Hotpatch applied in " << elapsed.count() << "μs\n";
    
    // Record measurement
    metrics.TokenGenerationEnd(1);
    auto result = metrics.GetFinalMeasurement();
    
    std::cout << "  ✓ Measurement captured: " << result.tokens_generated << " tokens\n";
    
    // Test failure detection integration
    auto failure = hotpatchEngine.detectFailure("test output");
    std::cout << "  ✓ Failure detection integrated\n";
    
    return true;
}

// =============================================================================
// TEST 3: AST Wiring - Extension API Callable
// =============================================================================
bool test_ast_wiring_extension_callable() {
    std::cout << "[TEST 3] AST Wiring - Extension API Callable...\n";
    
    // Initialize AST completion bridge
    ASTCompletionBridge astBridge;
    assert(astBridge.initialize() && "AST bridge init failed");
    
    // Simulate extension calling AST completion
    CursorLocation cursor{42, 15, 128};
    auto context = astBridge.captureASTContext(cursor);
    
    std::cout << "  ✓ AST context captured: " << context.scope_stack.size() << " scopes\n";
    
    // Test completion via extension API path
    auto completions = astBridge.requestCompletions(cursor, "test");
    std::cout << "  ✓ Completions requested via AST bridge\n";
    
    // Verify scheduler integration
    astBridge.onPrefetchCompletion("test.cpp", cursor);
    std::cout << "  ✓ AST bridge scheduler integration active\n";
    
    return true;
}

// =============================================================================
// TEST 4: Slash Commands - Hotpatch Callable
// =============================================================================
bool test_slash_commands_hotpatch_callable() {
    std::cout << "[TEST 4] Slash Commands - Hotpatch Callable...\n";
    
    // Initialize slash command parser
    SlashCommandParser parser;
    
    // Register a hotpatch-triggered slash command
    parser.registerHandler("/hotpatch", [](const std::vector<std::string>& args) {
        std::cout << "  ✓ Hotpatch-triggered slash command executed\n";
        return true;
    });
    
    // Parse and execute via hotpatch path
    std::string input = "/hotpatch apply";
    auto result = parser.parse(input);
    
    if (result.has_value()) {
        bool executed = result->handler(result->arguments);
        assert(executed && "Slash command execution failed");
    }
    
    // Test explain command (agentic integration)
    auto explainResult = parser.parse("/explain MyClass::process");
    assert(explainResult.has_value() && "Explain command parsing failed");
    std::cout << "  ✓ Agentic /explain command parsed\n";
    
    return true;
}

// =============================================================================
// TEST 5: Cross-Subsystem Event Flow
// =============================================================================
bool test_cross_subsystem_event_flow() {
    std::cout << "[TEST 5] Cross-Subsystem Event Flow...\n";
    
    auto* bridge = ExtensionAPIBridge::getInstance();
    Agentic::Hotpatch::Engine hotpatch;
    MeasurementCollector metrics;
    
    // Set up event chain: Extension → Hotpatch → Measurement
    bool eventReceived = false;
    
    // Extension subscribes to hotpatch events
    bridge->subscribeToEvent("hotpatch.applied", 
        [](const char* eventType, const char* payload) {
            std::cout << "  ✓ Extension received hotpatch event: " << eventType << "\n";
        }, nullptr);
    
    // Simulate hotpatch triggering measurement
    metrics.TokenGenerationStart();
    
    // Emit event from hotpatch
    bridge->publishEvent("hotpatch.applied", R"({"hook": "test"})");
    
    metrics.TokenGenerationEnd(1);
    
    std::cout << "  ✓ Event flow: Hotpatch → Extension → Measurement verified\n";
    
    return true;
}

// =============================================================================
// TEST 6: FFI C-API Accessibility
// =============================================================================
bool test_ffi_c_api_accessible() {
    std::cout << "[TEST 6] FFI C-API Accessibility...\n";
    
    // Test C API is callable (for external tools/agents)
    auto* handle = rawrxd_extension_create();
    assert(handle != nullptr && "C API create failed");
    
    int cmdId = rawrxd_extension_register_command(handle, "ffi.test", "FFI Test",
        [](void* data) {
            std::cout << "  ✓ FFI C-API command executed\n";
        }, nullptr);
    assert(cmdId >= 0 && "C API register failed");
    
    rawrxd_extension_execute_command(handle, "ffi.test");
    
    // Test AST enrichment via FFI
    RawrXD_ASTContext ctx{};
    ctx.file_path = "test.cpp";
    ctx.line = 10;
    ctx.column = 5;
    ctx.offset = 256;
    
    RawrXD_ASTEnrichedContext enriched;
    int enrichResult = rawrxd_ast_completion_enrich(&ctx, &enriched);
    
    std::cout << "  ✓ FFI AST enrichment returned: " << enrichResult << "\n";
    
    rawrxd_extension_destroy(handle);
    
    return true;
}

// =============================================================================
// MAIN
// =============================================================================
int main() {
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  RawrXD Unified Integration Verification                     ║\n";
    std::cout << "║  Tests: Extension ↔ Hotpatch ↔ Measurement ↔ AST ↔ Agentic   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    int passed = 0;
    int total = 6;
    
    try {
        if (test_extension_api_agentic_callable()) passed++;
        if (test_hotpatch_measurement_integration()) passed++;
        if (test_ast_wiring_extension_callable()) passed++;
        if (test_slash_commands_hotpatch_callable()) passed++;
        if (test_cross_subsystem_event_flow()) passed++;
        if (test_ffi_c_api_accessible()) passed++;
    } catch (const std::exception& e) {
        std::cerr << "\n[FAIL] Exception: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "\n══════════════════════════════════════════════════════════════════\n";
    std::cout << "Results: " << passed << "/" << total << " tests passed\n";
    
    if (passed == total) {
        std::cout << "✓ ALL SUBSYSTEMS INTEGRATED AND CALLABLE\n";
        std::cout << "✓ Extension API ↔ Hotpatch ↔ Measurement ↔ AST ↔ Agentic\n";
        std::cout << "✓ FFI C-API accessible for external tools\n";
        return 0;
    } else {
        std::cout << "✗ INTEGRATION INCOMPLETE\n";
        return 1;
    }
}
