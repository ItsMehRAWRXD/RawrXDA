#include "Win32IDE.h"
#include "../core/shared_feature_dispatch.h"
#include "../core/neural_bridge.hpp"
#include <windows.h>

// Handler for Neural Bridge feature
CommandResult HandleNeuralBridge(const CommandContext& ctx) {
    Win32IDE* ide = static_cast<Win32IDE*>(ctx.idePtr);

    auto init = rawrxd::NeuralBridge::instance().initialize();
    if (!init.success) {
        return CommandResult::error(init.detail.c_str(), init.errorCode);
    }

    static thread_local std::string status;
    status  = "Neural Bridge Active\n";
    status += "Neural bridging capabilities:\n";
    status += "- Model integration\n";
    status += "- Neural data flow\n";
    status += "- Cross-model communication\n";
    status += "- Inference optimization\n";
    status += "- Neural pipeline management";

    if (ctx.outputFn) ctx.outputLine(status);
    if (ctx.isGui && ide) {
        MessageBoxA(nullptr, status.c_str(), "Neural Bridge", MB_ICONINFORMATION | MB_OK);
    }
    return CommandResult::ok(status.c_str());
}
