# GPU policy matrix (E12 / E13)

Last updated: 2026-03-21.

## Discovery (E12)

- **Vulkan compute path:** `src/backend/vulkan_compute.cpp` enumerates physical devices via `vkEnumeratePhysicalDevices`, prefers `VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU`, falls back to the first device. Heap budgets come from `vkGetPhysicalDeviceMemoryProperties` after device selection.
- **UI diagnostics:** Surface **device name**, **device type**, and **chosen lane** in AI/Model diagnostics when the Vulkan stack is active (extend `Win32IDE_ModelDiscovery` / settings panels as wiring lands).

## Heterogeneous policy (E13)

| Lane | Status | Notes |
|------|--------|--------|
| AMD + Vulkan | Supported in-tree for compute experiments | Shaderc + SPIR-V pipelines in `vulkan_compute.cpp` |
| Intel + Vulkan | Same API surface | Validate on real hardware; no “supported” badge without CI |
| ARM | Document-only / future | No fake “green” in UI without a tested backend |
| External (Ollama / cloud) | User-selected | Never implied as local GPU |

Feature flags should live in CMake targets (e.g. `ENABLE_VULKAN`) and be mirrored in any UI capability manifest so **stubs are explicit** (bridge gap F policy).
