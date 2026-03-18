#include "Win32IDE.h"
#include "../core/perf_telemetry.hpp"

#include <sstream>

void HandlePerfTelemetry(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) {
        return;
    }

    auto& telemetry = RawrXD::Perf::PerfTelemetry::instance();
    const auto init = telemetry.initialize();
    if (!init.success && !telemetry.isInitialized()) {
        MessageBoxA(ide->getMainWindow(), "Failed to initialize performance telemetry kernel.", "Performance Telemetry", MB_ICONERROR | MB_OK);
        return;
    }

    std::ostringstream msg;
    msg << "Performance telemetry is active.\n\n";

    const auto report = telemetry.generateReport(static_cast<uint32_t>(RawrXD::Perf::KernelSlot::Camellia_EncryptBlock));
    msg << "Sample slot: " << (report.kernelName ? report.kernelName : "Camellia_EncryptBlock") << "\n"
        << "Count: " << report.count << "\n"
        << "Min cycles: " << report.minCycles << "\n"
        << "Max cycles: " << report.maxCycles << "\n"
        << "Mean cycles: " << report.meanCycles << "\n";

    MessageBoxA(ide->getMainWindow(), msg.str().c_str(), "Performance Telemetry", MB_ICONINFORMATION | MB_OK);
}
