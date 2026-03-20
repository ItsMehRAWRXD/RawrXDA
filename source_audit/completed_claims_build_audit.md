# Completed Claims vs Current Build Audit

Generated: 2026-03-20 19:27:48

## Scope

- docs/FULL_AUDIT_MASTER.md
- UNFINISHED_FEATURES.md

- WIN32IDE_SOURCES entries parsed: **523**
- ASM_KERNEL_SOURCES entries parsed: **47**
- Active CMakeLists scanned: **103**
- Claims scanned: **37**

## Mismatch Summary

| Mismatch Type | Count |
|---------------|-------|
| claimed_file_missing | 6 |
| claimed_file_missing_but_referenced_by_win32ide_sources | 2 |
| claimed_file_not_in_expected_build_graph | 3 |
| claimed_symbol_missing | 0 |
| claimed_completed_with_not_implemented_markers | 0 |

## Claimed file missing

- **multi_file_search_stub.cpp** (docs/FULL_AUDIT_MASTER.md:55)
  - claim: - Added to WIN32IDE_SOURCES: `model_registry.cpp`, `checkpoint_manager.cpp`, `universal_model_router.cpp`, `agent/project_context.cpp`, `ui/interpretability_panel.cpp`, `feature_registry_panel.cpp`, `multi_file_search_stub.cpp`, `benchmark_menu_stub.cpp`, `enterprise_licensev2_impl.cpp`, `license_enforcement.cpp`, `enterprise_feature_manager.cpp`, `feature_flags_runtime.cpp`, `context_deterioration_hotpatch.cpp`, `agentic_autonomous_config.cpp`, `multi_gpu.cpp`, `Win32IDE_LicenseCreator.cpp`, `monaco_settings_dialog.cpp`, `thermal/RAWRXD_ThermalDashboard.cpp`, `VSCodeMarketplaceAPI.cpp`.
  - CMake lines: [2976, 3358]
  - CMake files: ['CMakeLists.txt']
- **benchmark_menu_stub.cpp** (docs/FULL_AUDIT_MASTER.md:55)
  - claim: - Added to WIN32IDE_SOURCES: `model_registry.cpp`, `checkpoint_manager.cpp`, `universal_model_router.cpp`, `agent/project_context.cpp`, `ui/interpretability_panel.cpp`, `feature_registry_panel.cpp`, `multi_file_search_stub.cpp`, `benchmark_menu_stub.cpp`, `enterprise_licensev2_impl.cpp`, `license_enforcement.cpp`, `enterprise_feature_manager.cpp`, `feature_flags_runtime.cpp`, `context_deterioration_hotpatch.cpp`, `agentic_autonomous_config.cpp`, `multi_gpu.cpp`, `Win32IDE_LicenseCreator.cpp`, `monaco_settings_dialog.cpp`, `thermal/RAWRXD_ThermalDashboard.cpp`, `VSCodeMarketplaceAPI.cpp`.
- **src/win32app/benchmark_runner_stub.cpp** (UNFINISHED_FEATURES.md:216)
  - claim: | benchmark_runner_stub | src/win32app/benchmark_runner_stub.cpp | Yes | Yes | IDE build variant; minimal for unique_ptr; full impl in benchmark build. |
- **src/win32app/benchmark_menu_stub.cpp** (UNFINISHED_FEATURES.md:217)
  - claim: | benchmark_menu_stub | src/win32app/benchmark_menu_stub.cpp | Yes | Yes | IDE build variant; real dialog when menu invoked. |
- **src/win32app/multi_file_search_stub.cpp** (UNFINISHED_FEATURES.md:218)
  - claim: | multi_file_search_stub | src/win32app/multi_file_search_stub.cpp | Yes | Yes | IDE build variant; real filesystem search, regex, glob. |
  - CMake lines: [2976, 3358]
  - CMake files: ['CMakeLists.txt']
- **src/win32app/Win32IDE_logMessage_stub.cpp** (UNFINISHED_FEATURES.md:221)
  - claim: | Win32IDE_logMessage_stub | src/win32app/Win32IDE_logMessage_stub.cpp | Yes | Yes | Build variant: OutputDebugString + ide.log. |

## Claimed file missing but still referenced by WIN32IDE_SOURCES

- **multi_file_search_stub.cpp** (docs/FULL_AUDIT_MASTER.md:55)
  - claim: - Added to WIN32IDE_SOURCES: `model_registry.cpp`, `checkpoint_manager.cpp`, `universal_model_router.cpp`, `agent/project_context.cpp`, `ui/interpretability_panel.cpp`, `feature_registry_panel.cpp`, `multi_file_search_stub.cpp`, `benchmark_menu_stub.cpp`, `enterprise_licensev2_impl.cpp`, `license_enforcement.cpp`, `enterprise_feature_manager.cpp`, `feature_flags_runtime.cpp`, `context_deterioration_hotpatch.cpp`, `agentic_autonomous_config.cpp`, `multi_gpu.cpp`, `Win32IDE_LicenseCreator.cpp`, `monaco_settings_dialog.cpp`, `thermal/RAWRXD_ThermalDashboard.cpp`, `VSCodeMarketplaceAPI.cpp`.
  - CMake lines: [2976, 3358]
  - CMake files: ['CMakeLists.txt']
- **src/win32app/multi_file_search_stub.cpp** (UNFINISHED_FEATURES.md:218)
  - claim: | multi_file_search_stub | src/win32app/multi_file_search_stub.cpp | Yes | Yes | IDE build variant; real filesystem search, regex, glob. |
  - CMake lines: [2976, 3358]
  - CMake files: ['CMakeLists.txt']

## Claimed complete but not in expected build graph

- **src/win32app/digestion_engine_stub.cpp** (UNFINISHED_FEATURES.md:219)
  - path: `.archived_orphans/digestion_engine_stub.cpp`
  - path: `Full Source/src/win32app/digestion_engine_stub.cpp`
  - path: `history/reconstructed/src/win32app/digestion_engine_stub.cpp`
  - claim: | digestion_engine_stub | src/win32app/digestion_engine_stub.cpp | Yes | Yes | Production impl (metrics, JSON); filename retained. |
  - markers: {'todo': 6}
- **src/core/license_manager_panel.cpp** (UNFINISHED_FEATURES.md:229)
  - path: `src/core/license_manager_panel.cpp`
  - claim: | license_manager_panel show() | src/core/license_manager_panel.cpp | Yes | Yes | Full impl: LicenseInfoDialog and LicenseActivationDialog show Win32 modal dialogs; display* fill list from EnterpriseLicenseV2; Activate calls loadKeyFromFile. |
- **src/telemetry/logger.cpp** (UNFINISHED_FEATURES.md:253)
  - path: `src/telemetry/logger.cpp`
  - claim: | logger (telemetry) | src/telemetry/logger.cpp | Yes | Yes | Structured JSON logging stub. |

## Claimed symbol missing

- None

## Claimed complete but file still contains 'not implemented'

- None

## Notes

- This audit compares current repository state, not historical build artifacts.
- Inclusion checks are against current root `CMakeLists.txt` WIN32IDE source graph.
- Marker hits are textual signals and should be manually triaged before code changes.
