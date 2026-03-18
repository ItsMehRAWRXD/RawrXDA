#include "Win32IDE.h"
#include "telemetry/UnifiedTelemetryCore.h"
#include <windows.h>
#include <string>

// Handler for unified telemetry core feature
void HandleUnifiedTelemetry(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    // Initialize the unified telemetry core if not already done
    auto& telemetry = RawrXD::Telemetry::UnifiedTelemetryCore::Instance();
    if (!telemetry.IsInitialized()) {
        telemetry.Initialize("logs/telemetry", RawrXD::Telemetry::TelemetryLevel::Info);
    }

    // Emit a test event
    telemetry.EmitSystemEvent("ide.telemetry.test", "User activated unified telemetry core");

    // Show a message with telemetry status
    std::string status = "Unified Telemetry Core initialized.\n";
    status += "Log directory: logs/telemetry\n";
    status += "Events will be recorded to telemetry.jsonl";

    MessageBoxA(NULL, status.c_str(), "Unified Telemetry Core", MB_ICONINFORMATION | MB_OK);
}