# One-to-One File Match Audit

Generated: 2026-03-20 19:31:56

## Pair Summary

| Pair | Left Total | Right Total | Matched | Unmatched Left | Unmatched Right | Renamed/Mismatched Names |
|------|------------|-------------|---------|----------------|-----------------|--------------------------|
| src__vs__history_reconstructed_src | 4393 | 1929 | 1895 | 2498 | 34 | 1 |
| src__vs__Full_Source_src | 4393 | 3663 | 2883 | 1510 | 780 | 19 |
| history_reconstructed_src__vs__Full_Source_src | 1929 | 3663 | 1532 | 397 | 2131 | 23 |

## src__vs__history_reconstructed_src

Match types:
- basename: 1
- exact_path: 1893
- fuzzy_name: 1
- content drift on exact-path matches: 1594

Renamed / differently named samples:
- `codec/brutal_gzip.cpp` -> `codec/brutal_gzip_stub.cpp` (type=fuzzy_name, same_content=False)

## src__vs__Full_Source_src

Match types:
- basename: 193
- exact_content: 79
- exact_path: 2586
- fuzzy_name: 21
- normalized_stem: 4
- content drift on exact-path matches: 2526

Renamed / differently named samples:
- `RawrXD_SidebarCore.h` -> `backup_include/include/RawrXD_Sidebar_Core.h` (type=normalized_stem, same_content=False)
- `ReactServerGenerator.h` -> `backup_include/include/react_server_generator.h` (type=normalized_stem, same_content=False)
- `inference/PerformanceMonitor.h` -> `backup_include/include/performance_monitor.h` (type=normalized_stem, same_content=False)
- `ui/swarm_orchestrator.h` -> `backup_include/include/SwarmOrchestrator.h` (type=normalized_stem, same_content=False)
- `agent/agentic_copilot_bridge_new.hpp` -> `backup_include/include/agentic_copilot_bridge.hpp` (type=fuzzy_name, same_content=False)
- `agent/gguf_proxy_server_new.hpp` -> `backup_include/include/gguf_proxy_server.hpp` (type=fuzzy_name, same_content=False)
- `agent/agent_hot_patcher_new.hpp` -> `backup_include/include/agent_hot_patcher.hpp` (type=fuzzy_name, same_content=False)
- `agent/ide_agent_bridge_new.hpp` -> `backup_include/include/ide_agent_bridge.hpp` (type=fuzzy_name, same_content=False)
- `asm/RawrXD_Sidebar_x64.h` -> `backup_include/include/RawrXD_Sidebar.h` (type=fuzzy_name, same_content=False)
- `cpu_inference_engine_Clean.h` -> `backup_include/include/cpu_inference_engine.h` (type=fuzzy_name, same_content=False)
- `RawrXD_SignalSlot_Wiring.h` -> `backup_include/include/RawrXD_SignalSlot.h` (type=fuzzy_name, same_content=False)
- `agent/sentry_integration_new.hpp` -> `backup_include/include/sentry_integration.hpp` (type=fuzzy_name, same_content=False)
- `RawrXD_EditorCore.h` -> `backup_include/include/RawrXD_Editor.h` (type=fuzzy_name, same_content=False)
- `enterprise_license.cpp` -> `core/enterprise_license_stubs.cpp` (type=fuzzy_name, same_content=False)
- `agent/auto_bootstrap_new.hpp` -> `backup_include/include/auto_bootstrap.hpp` (type=fuzzy_name, same_content=False)
- `ReverseEngineeringSuite.hpp` -> `backup_include/include/ReverseEngineering.hpp` (type=fuzzy_name, same_content=False)
- `feature_registry_panel.h` -> `backup_include/include/feature_registry.h` (type=fuzzy_name, same_content=False)
- `agent/auto_update_new.hpp` -> `backup_include/include/auto_update.hpp` (type=fuzzy_name, same_content=False)
- `agent/code_signer_new.hpp` -> `backup_include/include/code_signer.hpp` (type=fuzzy_name, same_content=False)

## history_reconstructed_src__vs__Full_Source_src

Match types:
- basename: 70
- exact_content: 52
- exact_path: 1380
- fuzzy_name: 26
- normalized_stem: 4
- content drift on exact-path matches: 591

Renamed / differently named samples:
- `RawrXD_SidebarCore.h` -> `backup_include/include/RawrXD_Sidebar_Core.h` (type=normalized_stem, same_content=False)
- `ReactServerGenerator.h` -> `backup_include/include/react_server_generator.h` (type=normalized_stem, same_content=False)
- `model_loader/model_loader.cpp` -> `model_loader/ModelLoader.cpp` (type=normalized_stem, same_content=False)
- `gui/CommandPalette.hpp` -> `backup_include/include/command_palette.hpp` (type=normalized_stem, same_content=False)
- `ai_integration_hub_new.cpp` -> `ai_integration_hub.cpp` (type=fuzzy_name, same_content=False)
- `agent/agentic_copilot_bridge_new.hpp` -> `backup_include/include/agentic_copilot_bridge.hpp` (type=fuzzy_name, same_content=False)
- `agent/agent_hot_patcher_new.hpp` -> `agent_hot_patcher.hpp` (type=fuzzy_name, same_content=False)
- `agent/gguf_proxy_server_new.hpp` -> `backup_include/include/gguf_proxy_server.hpp` (type=fuzzy_name, same_content=False)
- `asm/RawrXD_Sidebar_x64.h` -> `gui/RawrXD_Sidebar.h` (type=fuzzy_name, same_content=False)
- `minimal_test.cpp` -> `minimal_qt_test.cpp` (type=fuzzy_name, same_content=False)
- `cpu_inference_engine_Clean.h` -> `backup_include/include/cpu_inference_engine.h` (type=fuzzy_name, same_content=False)
- `agent/auto_update_new.cpp` -> `agent/auto_update.cpp` (type=fuzzy_name, same_content=False)
- `agent/auto_update_new.hpp` -> `agent/auto_update.hpp` (type=fuzzy_name, same_content=False)
- `engine/react_ide_generator_fixed.cpp` -> `modules/react_ide_generator.cpp` (type=fuzzy_name, same_content=False)
- `RawrXD_SignalSlot_Wiring.h` -> `backup_include/include/RawrXD_SignalSlot.h` (type=fuzzy_name, same_content=False)
- `agent/agentic_puppeteer_new.hpp` -> `backup_include/include/agentic_puppeteer.hpp` (type=fuzzy_name, same_content=False)
- `agent/hot_reload_new.hpp` -> `agent/hot_reload.hpp` (type=fuzzy_name, same_content=False)
- `agent/zero_touch_new.hpp` -> `agent/zero_touch.hpp` (type=fuzzy_name, same_content=False)
- `agent/zero_touch_new.cpp` -> `agent/zero_touch.cpp` (type=fuzzy_name, same_content=False)
- `RawrXD_EditorCore.h` -> `backup_include/include/RawrXD_Editor.h` (type=fuzzy_name, same_content=False)
- `agent/auto_bootstrap_new.hpp` -> `backup_include/include/auto_bootstrap.hpp` (type=fuzzy_name, same_content=False)
- `codec/brutal_gzip_stub.cpp` -> `codec/brutal_gzip.cpp` (type=fuzzy_name, same_content=False)
- `ReverseEngineeringSuite.hpp` -> `backup_include/include/ReverseEngineering.hpp` (type=fuzzy_name, same_content=False)

## Artifacts

- Full machine-readable mapping is in `file_one_to_one_match_audit.json`.
- Per-pair CSV maps are in `source_audit/file_match_maps/`.
