# Agentic Reality Audit (2026-03-07)

## Scope
- `src/agentic`
- `src/agent`
- `src/core`
- `src/win32app`
- `CMakeLists.txt`

## Wiring Change Applied
- Enforced real-lane wiring for `RawrXD-Win32IDE` by default:
  - Added `RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK` (default `OFF`).
  - Excluded known stub/link-fallback translation units from Win32IDE when `OFF`.
  - File: `CMakeLists.txt`.

## High-Risk Fake/Scaffold Signals

1. Build graph historically mixes real and stub units in the same target.
- `src/stubs.cpp` was included in Win32IDE source list.
- Multiple `_stubs.cpp` and `link_stubs_*.cpp` files are present and referenced by build logic.
- This creates non-deterministic "real vs fallback" behavior across builds.

2. Headless agentic bridge exists and returns hardcoded failures.
- `src/win32app/agentic_bridge_headless.cpp` includes methods returning `false`/empty values.
- This is incompatible with full IDE agentic autonomy if selected accidentally.

3. SSOT handler fallback includes scaffold emitters.
- `src/win32app/link_stubs_ssot_handlers.cpp` emits project templates tagged as scaffold.
- Indicates handler surface still includes generated/fallback behavior for some flows.

4. Contradictory audit claims vs code reality.
- `src/agentic/AUDIT.md` claims "no stubs or placeholders".
- Source tree still contains many explicit stub/fallback units and TODO markers.

5. Monolithic MASM files include both "real" and explicit TODO/stub regions.
- `src/win32app/mcp_hooks.asm` contains many `TODO`, `stub`, `placeholder`, and "not implemented" markers alongside "zero stubs" claims.
- Similar pattern appears in large `src/agentic/*.asm` "complete/production" files.

## Explicit Stub/Fallback File Inventory (sample)
- `src/stubs.cpp`
- `src/core/stubs.cpp`
- `src/core/universal_stub.cpp`
- `src/core/link_stubs_final.cpp`
- `src/core/link_stubs_remaining_classes.cpp`
- `src/core/ai_agent_masm_stubs.cpp`
- `src/core/analyzer_distiller_stubs.cpp`
- `src/core/streaming_orchestrator_stubs.cpp`
- `src/core/swarm_network_stubs.cpp`
- `src/core/subsystem_mode_stubs.cpp`
- `src/core/monaco_core_stubs.cpp`
- `src/core/missing_handler_stubs.cpp`
- `src/win32app/agentic_bridge_headless.cpp`
- `src/win32app/Win32IDE_headless_stubs.cpp`
- `src/win32app/link_stubs_ssot_handlers.cpp`
- `src/win32app/link_stubs_win32ide_methods.cpp`
- `src/win32app/reverse_engineered_stubs.cpp`

## Current Risk Assessment
- Runtime risk: medium-high (wrong fallback selection can silently disable autonomy features).
- Audit-trace risk: high (documentation claims can diverge from executable behavior).
- Build reproducibility risk: high (stub inclusion controlled by multiple branches/filters).

## Recommended Next Hardening Steps
1. Add CI gate to fail Win32IDE configure if any `*_stub*` or `link_stubs_*` file remains in final source list when `RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK=OFF`.
2. Split fallback units into dedicated `*_fallback.cpp` naming and map each to owning subsystem with explicit build options.
3. Add startup telemetry event listing active fallback modules (empty means full-real lane).
4. Normalize agentic docs to reflect executable truth, not intended design.
