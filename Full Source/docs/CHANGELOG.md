# Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]

### Added
- **Vulkan Backend Enhancement**: Implemented accurate free VRAM reporting using `VK_EXT_memory_budget`.
  - Added `VkPhysicalDeviceMemoryBudgetPropertiesEXT` query to `GPUBackend::availableMemory()`.
  - Added fallback to `total - allocated` when the extension is unavailable.
  - Improved scheduling decisions for large model loading (e.g., BigDaddyG 40GB).
- **Thermal NVMe Array Management**: Added `Initialize-ThermalNVMeArray.ps1` for thermal-aware Virtual VRAM paging across multiple NVMe drives.
- **Stress Test Suite**: Added `StressTest-BigDaddyG.ps1` to validate 40GB model loading with thermal-aware paging and dynamic drive selection.

### Fixed
- **Metrics Collector**: Resolved recursive mutex locking in `metrics_collector.cpp` by using snapshot-based aggregation.
- **Feature Tests**: Added VRAM allocation guards in `production_feature_test.cpp` to prevent crashes on systems with limited Vulkan budget.
- **MASM Entropy**: Implemented `XorShift64*` fast entropy generation in `soft_throttle_dispatch.asm` for high-performance thermal throttling jitter.
