#include "Win32IDE.h"
#include "../core/shared_feature_dispatch.h"
#include "../core/self_host_engine.hpp"
#include <windows.h>

// Handler for Self-Host Engine feature
CommandResult HandleSelfHostEngine(const CommandContext& ctx) {
    Win32IDE* ide = static_cast<Win32IDE*>(ctx.idePtr);

    auto init = SelfHostEngine::instance().initialize();
    if (!init.success) {
        return CommandResult::error(init.detail.c_str(), init.errorCode);
    }

    static thread_local std::string status;
    status  = "Self-Host Engine Active\n";
    status += "- Autonomous deployment\n";
    status += "- Self-maintenance\n";
    status += "- Resource self-management\n";
    status += "- Adaptive scaling\n";
    status += "- Self-healing systems";

    if (ctx.outputFn) ctx.outputLine(status);
    if (ctx.isGui && ide) {
        MessageBoxA(nullptr, status.c_str(), "Self-Host Engine", MB_ICONINFORMATION | MB_OK);
    }
    return CommandResult::ok(status.c_str());
}
