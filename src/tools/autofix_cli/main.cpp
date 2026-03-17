// RawrXD-AutoFixCLI — Standalone self-healing build-fix entry point
// Calls QuantumOrchestrator::executeAutoFix() and emits healing_telemetry.json
//
// Usage:
//   RawrXD-AutoFixCLI.exe [options]
//
// Options:
//   --build-command <cmd>     Build command to run (default: cmake --build build_prod --config Release)
//   --workspace-root <path>   Working directory for build (default: .)
//   --telemetry-out <file>    JSON telemetry output path (default: healing_telemetry.json)
//   --max-attempts <n>        Max auto-fix iterations (default: 3)

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include "../../agent/quantum_agent_orchestrator.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <string>

int main(int argc, char* argv[])
{
    std::string buildCommand  = "cmake --build build_prod --config Release";
    std::string workspaceRoot = ".";
    std::string telemetryOut  = "healing_telemetry.json";
    int         maxAttempts   = 3;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--build-command")  == 0 && i + 1 < argc) buildCommand  = argv[++i];
        else if (strcmp(argv[i], "--workspace-root") == 0 && i + 1 < argc) workspaceRoot = argv[++i];
        else if (strcmp(argv[i], "--telemetry-out")  == 0 && i + 1 < argc) telemetryOut  = argv[++i];
        else if (strcmp(argv[i], "--max-attempts")   == 0 && i + 1 < argc) maxAttempts   = std::stoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("RawrXD-AutoFixCLI  --  Quantum self-healing build fixer\n\n"
                   "  --build-command <cmd>     Build command (default: cmake --build build_prod --config Release)\n"
                   "  --workspace-root <path>   Working directory (default: .)\n"
                   "  --telemetry-out <file>    Telemetry JSON output (default: healing_telemetry.json)\n"
                   "  --max-attempts <n>        Max fix iterations (default: 3)\n");
            return 0;
        }
    }

    printf("[RAWRXD-AUTOFIX] build:     %s\n", buildCommand.c_str());
    printf("[RAWRXD-AUTOFIX] workspace: %s\n", workspaceRoot.c_str());
    printf("[RAWRXD-AUTOFIX] attempts:  %d\n", maxAttempts);
    fflush(stdout);

    using namespace RawrXD::Quantum;
    QuantumOrchestrator& orch = globalQuantumOrchestrator();

    ExecutionResult result = orch.executeAutoFix(buildCommand, workspaceRoot, maxAttempts);

    // Write telemetry JSON
    {
        std::ofstream tf(telemetryOut);
        if (tf.is_open()) {
            tf << "{\n"
               << "  \"attemptCount\": "           << result.iterationCount    << ",\n"
               << "  \"totalDiagnosticsGenerated\": " << result.todoItemsGenerated << ",\n"
               << "  \"totalDiagnosticsHandled\": "   << result.todoItemsGenerated << ",\n"
               << "  \"totalFixesStaged\": "         << result.todoItemsCompleted << ",\n"
               << "  \"finalStatus\": \""            << (result.success ? "success" : "failure") << "\",\n"
               << "  \"durationMs\": "               << result.totalDurationMs  << "\n"
               << "}\n";
            printf("[RAWRXD-AUTOFIX] telemetry written to %s\n", telemetryOut.c_str());
        } else {
            fprintf(stderr, "[RAWRXD-AUTOFIX] WARNING: could not write telemetry to %s\n", telemetryOut.c_str());
        }
    }

    printf("[RAWRXD-AUTOFIX] result=%s  attempts=%d  fixes=%d  duration=%llums\n",
           result.success ? "SUCCESS" : "FAILURE",
           result.iterationCount,
           result.todoItemsCompleted,
           (unsigned long long)result.totalDurationMs);
    fflush(stdout);
    return result.success ? 0 : 1;
}
