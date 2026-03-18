#include "Win32IDE.h"
#include "../core/shared_feature_dispatch.h"
#include "../core/hardware_synthesizer.hpp"
#include <windows.h>

CommandResult HandleHardwareSynthesizer(const CommandContext& ctx) {
    Win32IDE* ide = static_cast<Win32IDE*>(ctx.idePtr);

    auto init = HardwareSynthesizer::instance().initialize();
    if (!init.success) {
        return CommandResult::error(init.detail.c_str(), init.errorCode);
    }

    static thread_local std::string status;
    status  = "Hardware Synthesizer Active\n";
    status += "- FPGA synthesis\n";
    status += "- ASIC design\n";
    status += "- Hardware acceleration\n";
    status += "- Custom processor design\n";
    status += "- Neural hardware synthesis";

    if (ctx.outputFn) ctx.outputLine(status);
    if (ctx.isGui && ide) {
        MessageBoxA(nullptr, status.c_str(), "Hardware Synthesizer", MB_ICONINFORMATION | MB_OK);
    }
    return CommandResult::ok(status.c_str());
}
