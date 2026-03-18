// Win32IDE_AutonomousDebugger.cpp — Autonomous Debugger Integration
// Integrates AutonomousDebugger into Win32IDE for AI-assisted debugging

#include "Win32IDE.h"
#include "../core/autonomous_debugger.hpp"
#include <windows.h>
#include <string>
#include <sstream>
#include <vector>

using RawrXD::Debugging::AutonomousDebugger;
using RawrXD::Debugging::DebugConfig;
using RawrXD::Debugging::DebugResult;

namespace RawrXD::Win32IDE {

void HandleAutonomousDebug(HWND hwnd, WPARAM /*wParam*/, LPARAM /*lParam*/) {
    static bool initialized = false;
    if (!initialized) {
        DebugConfig config = DebugConfig::defaults();
        DebugResult initResult = AutonomousDebugger::instance().initialize(config);
        if (!initResult.success) {
            MessageBoxA(hwnd,
                        initResult.detail ? initResult.detail : "Autonomous Debugger init failed",
                        "Error",
                        MB_OK | MB_ICONERROR);
            return;
        }
        initialized = true;
    }

    // Display current stats as a lightweight sanity check
    std::string stats = AutonomousDebugger::instance().statsToJson();
    MessageBoxA(hwnd, stats.c_str(), "Autonomous Debug", MB_OK | MB_ICONINFORMATION);
}

} // namespace RawrXD::Win32IDE
