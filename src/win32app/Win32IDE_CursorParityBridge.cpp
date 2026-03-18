#include "Win32IDE.h"
#include "../core/shared_feature_dispatch.h"
#include "../../include/cursor_github_parity_bridge.h"
#include <windows.h>

// Handler for Cursor/GitHub Parity Bridge feature (CommandResult signature)
CommandResult HandleCursorParityBridge(const CommandContext& ctx) {
    Win32IDE* ide = static_cast<Win32IDE*>(ctx.idePtr);

    // Runtime verification of parity wiring
    int verifyResult = RawrXD::Parity::verifyCursorParityWiring(ctx.idePtr);
    if (verifyResult != 0) {
        return CommandResult::error("Cursor Parity Bridge verification failed", verifyResult);
    }

    // Emit status to UI/CLI
    static thread_local std::string status;
    status  = "Cursor/GitHub Parity Bridge Active\n";
    status += "- Multi-cursor editing\n";
    status += "- Inline completions\n";
    status += "- Code actions\n";
    status += "- Refactoring tools\n";
    status += "- AI chat integration";

    if (ctx.outputFn) ctx.outputLine(status);
    if (ctx.isGui && ide) {
        MessageBoxA(nullptr, status.c_str(), "Cursor Parity Bridge", MB_ICONINFORMATION | MB_OK);
    }
    return CommandResult::ok(status.c_str());
}
