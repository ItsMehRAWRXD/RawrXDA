# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- **Win32 IDE — AMD unified memory toggle:** Settings GUI (**AI / Model** → **AMD Unified Memory (SAM)**) persists `amdUnifiedMemoryEnabled` in `settings.json`; `applySettings()` enables/disables `AMDGPUAccelerator` unified mode + `UnifiedMemoryExecutor` (hardware SAM when available, else 512 MiB host-backed arena). `unified_memory_executor.cpp` linked for Win32IDE and RawrEngine-family targets.
- **VS Code blank window (Windows):** `docs/VSCODE_BLANK_WINDOW_FIX.md` + `scripts/Launch-VSCode-Safe.ps1` — `CalculateNativeWinOcclusion` workaround; `-CloseExistingCode` + doc for **mutex already exists**; Chromium “unknown option” warnings; correct log path hint for `-FreshUserData`; quoted paths, GPU-disable, `argv.json`, verbose logging.
- **Tier G Sovereign examples index:** `examples/sovereign/README.md` + **`examples/sovereign/symbol_registry_smoke.c`** (FNV-1a registry smoke); cross-links from `examples/README.md`, `SOVEREIGN_*` docs, and `toolchain/sovereign_minimal/README.md`.
- **Vulkan Backend Enhancement**: Implemented accurate free VRAM reporting using `VK_EXT_memory_budget`.
  - Added `VkPhysicalDeviceMemoryBudgetPropertiesEXT` query to `GPUBackend::availableMemory()`.
  - Added fallback to `total - allocated` when the extension is unavailable.
  - Improved scheduling decisions for large model loading (e.g., BigDaddyG 40GB).
- **Thermal NVMe Array Management**: Added `Initialize-ThermalNVMeArray.ps1` for thermal-aware Virtual VRAM paging across multiple NVMe drives.
- **Stress Test Suite**: Added `StressTest-BigDaddyG.ps1` to validate 40GB model loading with thermal-aware paging and dynamic drive selection.

### Fixed
- **Win32IDE.h Windows headers:** Correct include order for networking + shell dialogs — `winsock2.h` → `ws2tcpip.h` → `windows.h` → `commdlg.h` (with `WIN32_LEAN_AND_MEAN`), fixing `commdlg.h` before `windows.h` / Winsock-after-Windows issues that could trigger cascading compile errors.
- **Metrics Collector**: Resolved recursive mutex locking in `metrics_collector.cpp` by using snapshot-based aggregation.
- **Feature Tests**: Added VRAM allocation guards in `production_feature_test.cpp` to prevent crashes on systems with limited Vulkan budget.
- **MASM Entropy**: Implemented `XorShift64*` fast entropy generation in `soft_throttle_dispatch.asm` for high-performance thermal throttling jitter.
