# RawrXD Pure x64 MASM Conversion

Full production x64 MASM implementations—**no scaffolding, no “in a production impl this would be *” placeholders.** The “simple” (minimal/stub) implementations are removed; these modules are complete.

## Modules

| Source (C++) | MASM Module | Description |
|--------------|-------------|-------------|
| `unified_overclock_governor.cpp` / `.h` | `rawrxd_governor.asm` | Unified Overclock Governor: CPU/GPU/Memory/Storage, PID, control loop, Win32 powercfg/CreateProcess, mutex, telemetry |
| `quantum_beaconism_backend.h` | `rawrxd_quantum_beaconism.asm` | Quantum Beaconism: fusion state machine, annealing, beacon broadcast, telemetry |
| `dual_engine_system.h` | `rawrxd_dual_engine.asm` | 10× dual engine coordinator: CLI dispatch (--infer-optimize, --mem-compact, etc.), engine registry |
| `codebase_audit_system_impl.cpp` / `codebase_audit_system.hpp` | `rawrxd_audit.asm` | Codebase audit: FindFirstFile/FindNextFile, source file scan, statistics |

## Build

From repo root:

```powershell
.\scripts\Build-X64MASM.ps1
```

Output: `build_ide\x64\RawrXD_X64.dll` (no CRT entry; use `/NOENTRY` and call exported functions from the IDE or C++ loader).

Options:

- `-Clean` — remove `build_ide\x64`
- `-OutputDir <path>` — use a different output directory
- `-NoLink` — assemble only (`.obj` files)

Requires: Visual Studio 2022 (or Build Tools) with x64 MASM (`ml64.exe`). Run from “x64 Native Tools Command Prompt” or ensure `ml64` and `link` are on `PATH`.

## Exported Symbols (DLL)

- **Governor:** `HardwareDomainToString_asm`, `UnifiedOverclockGovernor_Instance`, `UnifiedOverclockGovernor_initialize`, `UnifiedOverclockGovernor_shutdown`, `UnifiedOverclockGovernor_isRunning`, `UnifiedOverclockGovernor_applyOffset`, `UnifiedOverclockGovernor_emergencyThrottleAll`, `UnifiedOverclockGovernor_resetAllToBaseline`, `UnifiedOverclockGovernor_getDomainTelemetry`, `UnifiedOverclockGovernor_isEmergencyActive`, `UnifiedOverclockGovernor_clearEmergency`, `ControlLoopThread`
- **Quantum Beaconism:** `QuantumBeaconismBackend_Instance`, `QuantumBeaconismBackend_initialize`, `QuantumBeaconismBackend_shutdown`, `QuantumBeaconismBackend_getState`, `QuantumBeaconismBackend_beginFusion`, `QuantumBeaconismBackend_pauseFusion`, `QuantumBeaconismBackend_resetFusion`, `QuantumBeaconismBackend_getLatestBeacon`, `QuantumBeaconismBackend_getTelemetry`, `FusionStateToString_asm`
- **Dual Engine:** `DualEngineCoordinator_Instance`, `DualEngineCoordinator_initializeAll`, `DualEngineCoordinator_shutdownAll`, `DualEngineCoordinator_getEngine`, `DualEngineCoordinator_dispatchCLI`, `EngineIdToString_asm`
- **Audit:** `CodebaseAuditSystem_Create`, `CodebaseAuditSystem_initialize`, `CodebaseAuditSystem_shutdown`, `CodebaseAuditSystem_audit_full_project`, `CodebaseAuditSystem_analyze_source_file`, `CodebaseAuditSystem_get_audit_statistics`

## Integration with IDE

Load `RawrXD_X64.dll` via `LoadLibraryA` and resolve the symbols above with `GetProcAddress`. Struct layouts (e.g. `ClockResult`, `ClockProfile`, `DomainTelemetry`, `FusionResult`, `Beacon`, `EngineResult`, `FusionTelemetry`) are defined in the ASM; use matching C/C++ structs when calling from the IDE.
