// ============================================================================
// Win32IDE_CommandHandlers.cpp — CommandResult overloads for feature handlers
// ============================================================================
// Bridges legacy GUI-only handlers (void*, MessageBox) to the shared
// CommandContext/CommandResult dispatch used by both GUI and CLI.
// Each handler delegates to the existing implementation and reports success.
// ============================================================================

#include "Win32IDE.h"
#include "../core/shared_feature_dispatch.h"

// Forward declarations of existing GUI handlers (void* idePtr signature)
void HandleTranscendenceCoordinator(void* idePtr);
void HandleVulkanRenderer(void* idePtr);
void HandleOSExplorerInterceptor(void* idePtr);
void HandleMCPHooks(void* idePtr);
void HandleIOCPFileWatcher(void* idePtr);
void HandleIDEDiagnosticAutoHealer(void* idePtr);
void HandleConsentPrompt(void* idePtr);
void HandleAutonomousAgent(void* idePtr);
void HandleChatMessageRenderer(void* idePtr);
void HandleToolActionStatus(void* idePtr);
void HandleChatPanel(void* idePtr);
void HandlePerfTelemetry(void* idePtr);
void HandleUpdateSignature(void* idePtr);
void HandlePluginSignature(void* idePtr);

// Helper to emit output consistently
static void emitStatus(const CommandContext& ctx, const char* message) {
    if (ctx.outputFn) ctx.outputLine(message);
}

CommandResult HandleTranscendenceCoordinator(const CommandContext& ctx) {
    HandleTranscendenceCoordinator(ctx.idePtr);
    emitStatus(ctx, "Transcendence Coordinator executed");
    return CommandResult::ok("Transcendence Coordinator executed");
}

CommandResult HandleVulkanRenderer(const CommandContext& ctx) {
    HandleVulkanRenderer(ctx.idePtr);
    emitStatus(ctx, "Vulkan Renderer invoked");
    return CommandResult::ok("Vulkan Renderer invoked");
}

CommandResult HandleOSExplorerInterceptor(const CommandContext& ctx) {
    HandleOSExplorerInterceptor(ctx.idePtr);
    emitStatus(ctx, "OS Explorer Interceptor invoked");
    return CommandResult::ok("OS Explorer Interceptor invoked");
}

CommandResult HandleMCPHooks(const CommandContext& ctx) {
    HandleMCPHooks(ctx.idePtr);
    emitStatus(ctx, "MCP Hooks invoked");
    return CommandResult::ok("MCP Hooks invoked");
}

CommandResult HandleIOCPFileWatcher(const CommandContext& ctx) {
    HandleIOCPFileWatcher(ctx.idePtr);
    emitStatus(ctx, "IOCP File Watcher invoked");
    return CommandResult::ok("IOCP File Watcher invoked");
}

CommandResult HandleIDEDiagnosticAutoHealer(const CommandContext& ctx) {
    HandleIDEDiagnosticAutoHealer(ctx.idePtr);
    emitStatus(ctx, "IDE Diagnostic AutoHealer invoked");
    return CommandResult::ok("IDE Diagnostic AutoHealer invoked");
}

CommandResult HandleConsentPrompt(const CommandContext& ctx) {
    HandleConsentPrompt(ctx.idePtr);
    emitStatus(ctx, "Consent Prompt shown");
    return CommandResult::ok("Consent Prompt shown");
}

CommandResult HandleAutonomousAgent(const CommandContext& ctx) {
    HandleAutonomousAgent(ctx.idePtr);
    emitStatus(ctx, "Autonomous Agent invoked");
    return CommandResult::ok("Autonomous Agent invoked");
}

CommandResult HandleChatMessageRenderer(const CommandContext& ctx) {
    HandleChatMessageRenderer(ctx.idePtr);
    emitStatus(ctx, "Chat Message Renderer invoked");
    return CommandResult::ok("Chat Message Renderer invoked");
}

CommandResult HandleToolActionStatus(const CommandContext& ctx) {
    HandleToolActionStatus(ctx.idePtr);
    emitStatus(ctx, "Tool Action Status invoked");
    return CommandResult::ok("Tool Action Status invoked");
}

CommandResult HandleChatPanel(const CommandContext& ctx) {
    HandleChatPanel(ctx.idePtr);
    emitStatus(ctx, "Chat Panel invoked");
    return CommandResult::ok("Chat Panel invoked");
}

CommandResult HandlePerfTelemetry(const CommandContext& ctx) {
    HandlePerfTelemetry(ctx.idePtr);
    emitStatus(ctx, "Performance Telemetry invoked");
    return CommandResult::ok("Performance Telemetry invoked");
}

CommandResult HandleUpdateSignature(const CommandContext& ctx) {
    HandleUpdateSignature(ctx.idePtr);
    emitStatus(ctx, "Update Signature executed");
    return CommandResult::ok("Update Signature executed");
}

CommandResult HandlePluginSignature(const CommandContext& ctx) {
    HandlePluginSignature(ctx.idePtr);
    emitStatus(ctx, "Plugin Signature executed");
    return CommandResult::ok("Plugin Signature executed");
}
