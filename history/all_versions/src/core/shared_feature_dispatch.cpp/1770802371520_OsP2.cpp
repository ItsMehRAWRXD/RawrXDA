// ============================================================================
// shared_feature_dispatch.cpp — Unified Feature Dispatch Implementation
// ============================================================================
// Architecture: C++20, Win32, no Qt, no exceptions
// Implements the extern "C" MASM bridge and core feature registrations.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "shared_feature_dispatch.h"
#include <cstdio>
#include <cstring>

// ============================================================================
// EXTERN "C" BRIDGE IMPLEMENTATION — Callable from x64 MASM
// ============================================================================

extern "C" {

int rawrxd_dispatch_feature(const char* featureId, const char* args, void* idePtr) {
    if (!featureId) return -1;
    
    CommandContext ctx{};
    ctx.rawInput = args ? args : "";
    ctx.args = args ? args : "";
    ctx.idePtr = idePtr;
    ctx.cliStatePtr = nullptr;
    ctx.commandId = 0;
    ctx.isGui = (idePtr != nullptr);
    ctx.isHeadless = false;
    ctx.outputFn = nullptr;
    ctx.outputUserData = nullptr;
    
    auto result = SharedFeatureRegistry::instance().dispatch(featureId, ctx);
    return result.success ? 0 : result.errorCode;
}

int rawrxd_dispatch_command(uint32_t commandId, void* idePtr) {
    CommandContext ctx{};
    ctx.rawInput = "";
    ctx.args = "";
    ctx.idePtr = idePtr;
    ctx.cliStatePtr = nullptr;
    ctx.commandId = commandId;
    ctx.isGui = true;
    ctx.isHeadless = false;
    ctx.outputFn = nullptr;
    ctx.outputUserData = nullptr;
    
    auto result = SharedFeatureRegistry::instance().dispatchByCommandId(commandId, ctx);
    return result.success ? 0 : result.errorCode;
}

int rawrxd_dispatch_cli(const char* cliCommand, const char* args, void* cliStatePtr) {
    if (!cliCommand) return -1;
    
    CommandContext ctx{};
    ctx.rawInput = cliCommand;
    ctx.args = args ? args : "";
    ctx.idePtr = nullptr;
    ctx.cliStatePtr = cliStatePtr;
    ctx.commandId = 0;
    ctx.isGui = false;
    ctx.isHeadless = false;
    ctx.outputFn = nullptr;
    ctx.outputUserData = nullptr;
    
    auto result = SharedFeatureRegistry::instance().dispatchByCli(cliCommand, ctx);
    return result.success ? 0 : result.errorCode;
}

int rawrxd_get_feature_count(void) {
    return static_cast<int>(SharedFeatureRegistry::instance().totalRegistered());
}

} // extern "C"

// ============================================================================
// STDOUT output callback (for CLI mode)
// ============================================================================

static void stdoutOutputFn(const char* text, void* /*userData*/) {
    if (text) fputs(text, stdout);
}

// ============================================================================
// Helper: Build CLI context
// ============================================================================

CommandContext makeCliContext(const char* input, const char* args, void* cliState) {
    CommandContext ctx{};
    ctx.rawInput = input ? input : "";
    ctx.args = args ? args : "";
    ctx.idePtr = nullptr;
    ctx.cliStatePtr = cliState;
    ctx.commandId = 0;
    ctx.isGui = false;
    ctx.isHeadless = false;
    ctx.outputFn = stdoutOutputFn;
    ctx.outputUserData = nullptr;
    return ctx;
}

// ============================================================================
// Helper: Build GUI context
// ============================================================================

CommandContext makeGuiContext(uint32_t cmdId, void* idePtr) {
    CommandContext ctx{};
    ctx.rawInput = "";
    ctx.args = "";
    ctx.idePtr = idePtr;
    ctx.cliStatePtr = nullptr;
    ctx.commandId = cmdId;
    ctx.isGui = true;
    ctx.isHeadless = false;
    ctx.outputFn = nullptr;   // GUI sets its own
    ctx.outputUserData = nullptr;
    return ctx;
}
