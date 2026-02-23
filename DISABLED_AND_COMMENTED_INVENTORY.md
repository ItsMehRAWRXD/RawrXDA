# Disabled, Commented-Out & Non-Implemented Inventory

Single source of truth for every disabled/commented-out source file, CMake block, and non-implemented feature across the IDE and repo. Use this to prioritize re-enabling, implementing, or formally documenting each item.

**Last updated:** 2026-02-14

---

## 1. Root CMakeLists.txt — Commented / Excluded Sources

| Location (approx line) | Item | Reason | How to re-enable / note |
|------------------------|------|--------|-------------------------|
| 189 | `src/RawrXD_Complete_ReverseEngineered.asm` | Fragment missing struct defs/includes | Fix ASM fragment and includes; then uncomment in `ASM_KERNEL_SOURCES`. |
| 226, 478 | `src/validate_agentic_tools.cpp` | Standalone test with own `main()`; would duplicate main in RawrEngine/RawrXD_CLI | **Optional target:** Built as `validate_agentic_tools` when `RAWRXD_ENABLE_TESTS` is ON (see `tests/CMakeLists.txt`). Do not add to SOURCES. |
| 456 | `src/core/auto_feature_registry.cpp` | Win32 IDE only | Included in `WIN32IDE_SOURCES` / `GOLD_SOURCES`; excluded from RawrEngine so CLI uses `ssot_handlers` (lightweight). Intentional. |
| 587 | `src/tools/multi_model_benchmark.cpp` | Duplicate `main()` symbol in RawrEngine | **Alternative:** Use `real_multi_model_benchmark` (same dir, `src/tools/CMakeLists.txt`). Do not add to SOURCES. |
| 2402, 3128 | `src/terminal/zero_retention_manager.cpp` | ~~C2664/C3861 — header sync~~ | **Re-enabled 2026-02-14:** Header now includes `../json_types.hpp`; both WIN32IDE_SOURCES and GOLD_SOURCES include this file. |
| 2378 | `IDEDiagnosticAutoHealer_Impl.cpp` | Duplicate DiagnosticUtils | Excluded from WIN32IDE_SOURCES; use IDEDiagnosticAutoHealer.cpp only. |
| 2379 | `IDEAutoHealerLauncher.cpp` | Defines `main`, conflicts with ASM | Build as separate executable if needed; do not add to WIN32IDE_SOURCES. |
| 2673–2674 | `re_model_reverse` / RE_MODEL_OBJECTS for Win32IDE | Conflicts with WinMain | Comment documents conflict; keep commented unless separate entry point. |

**Summary:** All excluded sources are either (1) built as separate executables/tests, (2) Win32-only and only in WIN32IDE_SOURCES, or (3) fixed and re-enabled (zero_retention_manager).

---

## 2. Optional / Separate Executable Targets

| Target | Source(s) | Where defined | When built |
|--------|-----------|---------------|------------|
| `validate_agentic_tools` | `src/validate_agentic_tools.cpp` | `tests/CMakeLists.txt` | When `RAWRXD_ENABLE_TESTS` ON and file exists |
| `real_multi_model_benchmark` | `src/tools/real_multi_model_benchmark.cpp` | `src/tools/CMakeLists.txt` | Always (tools subdir) |
| `test_zero_retention_manager` | `tests/test_zero_retention_manager.cpp` | `tests/CMakeLists.txt` | When `RAWRXD_HAS_GTEST` and file exists |

---

## 3. RawrXD-ModelLoader CMakeLists.txt — Commented

| Location | Item | Note |
|----------|------|------|
| 252–264 | `rawrxd_test_baseline`, `rawrxd_test_pid` | Commented add_executable + baseline_profile/settings/overclock_governor/overclock_vendor. Optional test targets; uncomment to build. |

---

## 4. Source Files — #if 0 / Disabled Blocks (in-tree, non–third-party)

| File | Purpose of #if 0 / disabled block |
|------|-----------------------------------|
| `src/core/shared_feature_dispatch.cpp` | Entire file under `#if 0` — canonical versions in `unified_command_dispatch.cpp`; file kept for reference. |
| `tests/test-backend-ops.cpp` | Multiple `#if 0` blocks (e.g. 6607, 7094, 7108, 7213, 7366, 7691) — test/experimental code. |
| `Ship/RawrXD_AgentCoordinator.cpp` | Threading disabled (“Worker thread disabled for simpler compilation”; “Threading disabled for now”). |

**Third-party / vendor:** `src/core/sqlite3.c`, `src/ggml*.cpp`, `examples/gpt-*`, `3rdparty/ggml` — many `#if 0` blocks are upstream; do not change.

---

## 5. NOT IMPLEMENTED / “Not implemented” in Code (production intent)

| Location | Usage | Intent |
|----------|--------|--------|
| `src/win32app/feature_registry_panel.cpp` | Prints “NOT IMPLEMENTED” for features from `g_FeatureManifest` with `implemented: false` | Honest reporting; planned/N/A features (CUDA, HIP, FlashAttention, SpeculativeDecoding, etc.). See UNFINISHED_FEATURES.md “Stub holders”. |
| `src/enterprise_license.cpp` | “Feature not implemented” in message | License/feature gate messaging. |
| `src/tools/enterprise_license_unified_creator.cpp` | Many manifest entries `"Not implemented"` / “Gov-only — not implemented” | Planned features; manifest is source of truth for roadmap. |
| `src/core/ide_linker_bridge.cpp` | “signing not implemented; full createKey in license-server build” | IDE build variant; production comment. |
| `src/modules/vsix_loader_win32.cpp` | “.vsix extraction not implemented on this platform” | Platform-specific; production message. |
| `src/engine/pyre_compute.cpp` | “GGUF-to-Pyre bridge not implemented” | Clear error; use .pyre or StreamingGGUFLoader. |
| `src/core/auto_feature_registry.cpp` | findstr “not implemented” count | Audit/debug output. |
| Test harness / digestion / ggml-* | Various “NOT IMPLEMENTED” in tests or backend paths | Tests: stub; ggml: upstream op/backend gaps. |

---

## 6. Ship — Disabled / Non-Implemented / Commented

| Item | Location | Note |
|------|----------|------|
| Tool disabled message | `Ship/ToolExecutionEngine.hpp` | “Tool is disabled: ” + name — runtime behavior. |
| Copilot disabled | `Ship/RawrXD_CopilotBridge.cpp` | “Copilot disabled” — runtime state. |
| Worker thread disabled | `Ship/RawrXD_AgentCoordinator.cpp` | Threading commented out for simpler build. |
| EXECUTION_ROADMAP_4WEEK.md | “Remove any #if 0 blocks” | Optional cleanup task. |

---

## 7. Cross-Reference to UNFINISHED_FEATURES.md

- **Stub holders (intentional fallbacks):** UNFINISHED_FEATURES.md § “Stub holders (intentional fallbacks)”.
- **ALL STUBS table:** UNFINISHED_FEATURES.md § “ALL STUBS — comprehensive list”.
- **50 code-structure scaffolds:** UNFINISHED_FEATURES.md § “50 code-structure scaffolds”.
- **Feature manifest “NOT IMPLEMENTED”:** UNFINISHED_FEATURES.md § “Stub holders” and “Feature manifest `implemented: false`”.

---

## 8. Completed This Pass (2026-02-14)

| Item | Change |
|------|--------|
| **zero_retention_manager** | Header `zero_retention_manager.hpp` now includes `../json_types.hpp` so `JsonObject` is defined; `zero_retention_manager.cpp` re-enabled in both WIN32IDE_SOURCES and GOLD_SOURCES in root `CMakeLists.txt`. |
| **validate_agentic_tools** | Optional executable `validate_agentic_tools` added in `tests/CMakeLists.txt` when `RAWRXD_ENABLE_TESTS` and `src/validate_agentic_tools.cpp` exists; not added to SOURCES. |
| **DISABLED_AND_COMMENTED_INVENTORY.md** | This file created; root CMake, Ship, ModelLoader, NOT IMPLEMENTED, and optional targets documented. |

---

## 9. Suggested Priority Order for Remaining Work

1. **Build/CI:** Ensure `zero_retention_manager` compiles in your build dir (include path for `../json_types.hpp` from `src/terminal/` is already set by existing `src` include dirs).
2. **Tests:** Turn on `RAWRXD_ENABLE_TESTS` and build `validate_agentic_tools`; fix any missing includes/links for `AgenticToolExecutor`.
3. **Optional:** Uncomment RawrXD-ModelLoader `rawrxd_test_baseline` / `rawrxd_test_pid` if those tests are needed.
4. **Optional:** Re-enable threading in `Ship/RawrXD_AgentCoordinator.cpp` when ready and test.
5. **Reference only:** Leave `shared_feature_dispatch.cpp` as `#if 0` (canonical in unified_command_dispatch); leave ggml/sqlite/examples `#if 0` as upstream.
