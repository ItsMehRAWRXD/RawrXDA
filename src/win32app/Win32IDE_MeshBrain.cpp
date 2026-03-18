#include "Win32IDE.h"
#include "../core/shared_feature_dispatch.h"
#include "../core/mesh_brain.hpp"
#include <windows.h>

// Handler for Mesh Brain feature
CommandResult HandleMeshBrain(const CommandContext& ctx) {
    Win32IDE* ide = static_cast<Win32IDE*>(ctx.idePtr);

    auto init = MeshBrain::instance().initialize();
    if (!init.success) {
        return CommandResult::error(init.detail.c_str(), init.errorCode);
    }

    static thread_local std::string status;
    status  = "Mesh Brain Active\n";
    status += "- Distributed processing\n";
    status += "- Neural network coordination\n";
    status += "- Pattern recognition\n";
    status += "- Adaptive learning\n";
    status += "- Mesh optimization";

    if (ctx.outputFn) ctx.outputLine(status);
    if (ctx.isGui && ide) {
        MessageBoxA(nullptr, status.c_str(), "Mesh Brain", MB_ICONINFORMATION | MB_OK);
    }
    return CommandResult::ok(status.c_str());
}
