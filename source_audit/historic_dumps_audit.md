# Full Source Historic Dumps Audit

Generated: 2026-03-20 19:19:05

## Historic Dump Directory Inventory

| Path | Exists | Files | Directories | Size (MB) |
|------|--------|-------|-------------|-----------|
| history | yes | 36121 | 6296 | 708.83 |
| history/runoff | yes | 12112 | 25 | 138.71 |
| history/reconstructed | yes | 3139 | 238 | 44.16 |
| history/all_versions | yes | 13438 | 4128 | 363.01 |
| runoff | yes | 1088 | 3 | 31.46 |
| reconstructed | yes | 9615 | 8225 | 115.08 |
| Full Source/runoff | yes | 994 | 3 | 22.07 |

## Exact Duplicate Content (Across Historic Roots)

- Duplicate groups: **12077**
- Duplicate files participating: **32616**
- Reclaimable size if deduplicated: **300.75 MB**

Top duplicate groups:
1. size=250195 bytes, instances=5, reclaimable=0.95 MB
   - history/reconstructed/RawrXD_IDE_unified.asm
   - history/-1ec43a2c/WNq7.asm
   - history/all_versions/RawrXD_IDE_unified.asm/1770703917990_WNq7.asm
   - history/runoff/part_0010/d__rawrxd_RawrXD_IDE_unified.asm
2. size=259845 bytes, instances=4, reclaimable=0.74 MB
   - history/all_versions/src/core/auto_feature_registry.cpp/1770895417886_VaID.cpp
   - history/reconstructed/src/core/auto_feature_registry.cpp
   - history/runoff/part_0012/d__rawrxd_src_core_auto_feature_registry.cpp
   - reconstructed/RawrXD/src/core/auto/feature/registry.cpp
3. size=259545 bytes, instances=4, reclaimable=0.74 MB
   - history/all_versions/gui/ide_chatbot.html/1770546978698_zVYr.html
   - history/reconstructed/gui/ide_chatbot.html
   - history/runoff/part_0009/d__rawrxd_gui_ide_chatbot.html
   - reconstructed/RawrXD/gui/ide/chatbot.html
4. size=258190 bytes, instances=4, reclaimable=0.74 MB
   - history/all_versions/src/qtapp/MainWindow.cpp/1768655849650_1R5t.cpp
   - history/reconstructed/src/qtapp/MainWindow.cpp
   - history/runoff/part_0024/e__RawrXD_src_qtapp_MainWindow.cpp
   - reconstructed/RawrXD/src/qtapp/MainWindow.cpp
5. size=249533 bytes, instances=4, reclaimable=0.71 MB
   - history/all_versions/gui/ide_chatbot_win32.html/1770639563322_iViP.html
   - history/reconstructed/gui/ide_chatbot_win32.html
   - history/runoff/part_0009/d__rawrxd_gui_ide_chatbot_win32.html
   - reconstructed/RawrXD/gui/ide/chatbot/win32.html
6. size=182262 bytes, instances=5, reclaimable=0.7 MB
   - history/-1430341f/rj9e.cpp
   - history/all_versions/src/tool_server.cpp/1770910080785_rj9e.cpp
   - history/reconstructed/src/tool_server.cpp
   - history/runoff/part_0014/d__rawrxd_src_tool_server.cpp
7. size=173356 bytes, instances=5, reclaimable=0.66 MB
   - history/-152bca6/Rfpd.cpp
   - history/all_versions/src/core/missing_handler_stubs.cpp/1770911690106_Rfpd.cpp
   - history/reconstructed/src/core/missing_handler_stubs.cpp
   - history/runoff/part_0012/d__rawrxd_src_core_missing_handler_stubs.cpp
8. size=197099 bytes, instances=4, reclaimable=0.56 MB
   - history/all_versions/gui/ide_chatbot_standalone.html/1770636343724_muvI.html
   - history/reconstructed/gui/ide_chatbot_standalone.html
   - history/runoff/part_0009/d__rawrxd_gui_ide_chatbot_standalone.html
   - reconstructed/RawrXD/gui/ide/chatbot/standalone.html
9. size=260668 bytes, instances=3, reclaimable=0.5 MB
   - history/all_versions/src/engine/react_ide_generator.cpp/1770627252913_KgKU.cpp
   - history/reconstructed/src/engine/react_ide_generator.cpp
   - history/runoff/part_0013/d__RawrXD_src_engine_react_ide_generator.cpp
10. size=245101 bytes, instances=3, reclaimable=0.47 MB
   - history/all_versions/src/win32app/Win32IDE.h/1771606283213_OyHI.h
   - history/reconstructed/src/win32app/Win32IDE.h
   - history/runoff/part_0014/d__rawrxd_src_win32app_Win32IDE.h
11. size=145210 bytes, instances=5, reclaimable=0.55 MB
   - history/-2771dbdc/dU8I.cpp
   - history/all_versions/3rdparty/ggml/src/ggml-hexagon/ggml-hexagon.cpp/1771127328162_dU8I.cpp
   - history/reconstructed/3rdparty/ggml/src/ggml-hexagon/ggml-hexagon.cpp
   - history/runoff/part_0009/d__rawrxd_3rdparty_ggml_src_ggml-hexagon_ggml-hexagon.cpp
12. size=176936 bytes, instances=4, reclaimable=0.51 MB
   - history/all_versions/src/win32app/Win32IDE.cpp/1771269534809_DPrX.cpp
   - history/reconstructed/src/win32app/Win32IDE.cpp
   - history/runoff/part_0014/d__rawrxd_src_win32app_Win32IDE.cpp
   - reconstructed/RawrXD/src/win32app/Win32IDE.cpp

## Source Tree Parity (Path + Content)

| Pair | Common Paths | Same Content | Different Content |
|------|--------------|--------------|-------------------|
| full_source_src__vs__history_reconstructed_src | 1380 | 789 | 591 |
| full_source_src__vs__src | 2586 | 60 | 2526 |
| history_reconstructed_src__vs__src | 1893 | 299 | 1594 |

## Cross-Tree Drift Samples

- Paths present in both historic trees but not in `src` (sample size: 12):
  - QtGUIStubs.hpp
  - core/analyzer_distiller_stubs.cpp
  - core/debug_engine_stubs.cpp
  - core/enterprise_license_stubs.cpp
  - core/monaco_core_stubs.cpp
  - core/streaming_orchestrator_stubs.cpp
  - core/swarm_network_stubs.cpp
  - rawrxd_inference_core_stubs.h
  - rawrxd_inference_stubs.hpp
  - stubs.cpp
  - telemetry/ai_metrics_stub.cpp
  - win32app/digestion_engine_stub.cpp

- Paths present in `src` but absent from historic intersection (sample size: 100):
  - AdvancedFeatures.h
  - AppState.h
  - Audit.txt
  - BareMetal_PE_Writer.asm
  - BeaconClient.asm
  - BeaconClient.cpp
  - BeaconClient.h
  - CLI_CONTRACT_v1.0.md
  - CLOUD_IMPLEMENTATION_SUMMARY.md
  - CLOUD_INTEGRATION_GUIDE.md
  - CLOUD_QUICK_REFERENCE.md
  - CMakeLists_RealInference.txt
  - COMPLETION_REPORT.md
  - CodebaseContextAnalyzer.cpp
  - CodexUltimate_fixed.asm
  - CompilerAgentBridge.h
  - EventBus.h
  - EventBus_Wiring.cpp
  - FINAL_1KB_AUDIT_REPORT.md
  - GlobalContextExpanded.h

## Audit Notes

- This audit is exact-hash based for duplicate detection and parity checks.
- Samples are truncated for readability; full data is in the JSON report.
