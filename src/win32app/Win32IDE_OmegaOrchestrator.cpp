#include "Win32IDE.h"
#include "../core/shared_feature_dispatch.h"
#include "../core/omega_orchestrator.hpp"
#include <windows.h>

// Handler for Omega Orchestrator feature
CommandResult HandleOmegaOrchestrator(const CommandContext& ctx) {
    Win32IDE* ide = static_cast<Win32IDE*>(ctx.idePtr);

    auto& orchestrator = rawrxd::OmegaOrchestrator::instance();
    auto init = orchestrator.initialize();
    if (!init.success) {
        return CommandResult::error(init.detail.c_str(), init.errorCode);
    }

    static thread_local std::string status;
    status  = "Omega Orchestrator Active\n";
    status += "- Multi-agent coordination\n";
    status += "- Task scheduling\n";
    status += "- Resource management\n";
    status += "- Performance optimization\n";
    status += "- Autonomous operations";

    if (ctx.outputFn) ctx.outputLine(status);
    if (ctx.isGui && ide) {
        MessageBoxA(nullptr, status.c_str(), "Omega Orchestrator", MB_ICONINFORMATION | MB_OK);
    }
    return CommandResult::ok(status.c_str());
}
