# GPU discovery + VRAM truth table (E12)

**Done when:** At startup, **enumerated devices**, **budget**, and **chosen lane** are logged and visible in **AI/Model diagnostics**.

## Implementation targets

- **Vulkan:** `vkEnumeratePhysicalDevices`, heap budgets where available.
- **AMD:** AMDGPU path already in repo — merge into one **diagnostics** struct.
- **Log line example:** `[GPU] device=0 name=… vram_mb=… lane=vulkan chosen=1`

## UI

- Win32: **Models / Diagnostics** panel row “GPU: …”.
- Electron **Models** tab: mirror summary string from future IPC.

## Related

- **E13:** `docs/HETEROGENEOUS_GPU_POLICY.md` — policy matrix.
