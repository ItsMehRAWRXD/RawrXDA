#include "Win32IDE.h"
#include "../core/shared_feature_dispatch.h"
#include "../core/speciator_engine.hpp"
#include <windows.h>

CommandResult HandleSpeciatorEngine(const CommandContext& ctx) {
    Win32IDE* ide = static_cast<Win32IDE*>(ctx.idePtr);

    auto init = SpeciatorEngine::instance().initialize();
    if (!init.success) {
        return CommandResult::error(init.detail.c_str(), init.errorCode);
    }

    static thread_local std::string status;
    status  = "Speciator Engine Active\n";
    status += "- Evolutionary algorithms\n";
    status += "- Population management\n";
    status += "- Genetic optimization\n";
    status += "- Diversity preservation\n";
    status += "- Adaptive speciation";

    if (ctx.outputFn) ctx.outputLine(status);
    if (ctx.isGui && ide) {
        MessageBoxA(nullptr, status.c_str(), "Speciator Engine", MB_ICONINFORMATION | MB_OK);
    }
    return CommandResult::ok(status.c_str());
}
