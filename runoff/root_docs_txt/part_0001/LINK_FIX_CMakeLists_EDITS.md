# Link-Fix: RawrXD-Win32IDE — COMPLETE

All link errors for RawrXD-Win32IDE have been addressed. Summary of what was done:

## Sources / stubs added (Win32 IDE target)

- **reverse_engineered_stubs.cpp** — INFINITY_Shutdown, Scheduler_Initialize/Shutdown, ConflictDetector_Initialize, Heartbeat_Initialize/Shutdown, GetHighResTick (when ASM kernel not linked).
- **FIMPromptBuilder.cpp**, **agentic_observability.cpp**, **rawrxd_subsystem_api.cpp**, **subsystem_mode_stubs.cpp** — SubsystemRegistry and *Mode no-op stubs (CompileMode, EncryptMode, …).
- **local_parity_bridge.cpp**, **layer_offload_manager.cpp**, **runtime_core.cpp**, **telemetry_collector.cpp** — LocalParity_SetModelPath, LayerOffloadManager, EngineRegistry, TelemetryCollector.
- **tool_registry.cpp** — ToolRegistry::inject_tools for runtime_core.
- **DiskRecoveryAgent.cpp** (agentic), **DiskRecoveryToolHandler.cpp** — Recovery::DiskRecoveryAsmAgent and tool registration.
- **mesh_brain_asm_stubs.cpp** — asm_mesh_crdt_lookup, asm_mesh_topology_remove, asm_mesh_topology_count, asm_mesh_topology_list (not exported by RawrXD_MeshBrain.asm).

## Duplicate symbols avoided

- **IDEDiagnosticAutoHealer_Impl.cpp** — Excluded from IDE target (duplicate DiagnosticUtils).
- **IDEAutoHealerLauncher.cpp** — Excluded (defines `main`, conflicts with ASM).

## Other fixes

- **agentic_executor.cpp** — Removed stray first line.
- **telemetry_collector.hpp/cpp** — Use `json_types.hpp` JsonValue/JsonObject to avoid redefinition.

## deflate_brutal_masm

- Provided by MASM (masm_kernels → deflate_brutal_masm.obj). No C stub in IDE target to avoid LNK2005.
- **deflate_brutal_stub.cpp** exists for targets that do not link the MASM object (no zlib).

## Qt

- No Qt in the Win32 IDE build; remaining references are in legacy `qtapp/` or comments. Core and Win32 IDE paths are Qt-free.

## RawrXD-Gold: RAWRXD_WIN32_STATIC_BUILD

- For **RawrXD-Gold** to use the same Qt-converted Win32 components (FileManager, etc.) as the IDE, add this in `CMakeLists.txt` inside `target_compile_definitions(RawrXD-Gold PRIVATE ...)` (e.g. after `RAWRXD_GOLD_BUILD=1`):
  - `RAWRXD_WIN32_STATIC_BUILD=1`
