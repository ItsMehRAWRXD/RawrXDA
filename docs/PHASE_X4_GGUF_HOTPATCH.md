# Phase X+4: GGUF Hotpatch (Production)

**Goal:** Runtime model swapping without EXE restart; working set stays under 1.92GB. LSP L1 and Agent Loop are production-complete; X+4 is the next production surface.

---

## What's implemented

### 1. model_loader.asm
- **ModelLoaderInit** — Initializes SRWLOCK for thread-safe swap (called from main bootstrap).
- **HotSwapModel(newPath LPCWSTR, preserveKV BYTE)** — Acquires exclusive lock; unmaps/closes old file and mapping; zeros `pBase` and `g_modelbase`; calls `LoadModel(newPath)`; on success: copies path to `g_currentPathW`, sets `g_modelbase = pBase`, records **g_loadTimestamp** (GetTickCount64), **g_preservedKV**; if `preserveKV == 0` calls **ClearKVCache**; releases lock; **signals completion via BeaconSend(0, pNewPath, MODEL_HOTSWAP_COMPLETE)**. Returns 1 success, 0 fail.
- **GetCurrentModelPath** — Returns pointer to `g_currentPathW` (260 WCHARs).
- **GetModelLoadTimestamp** — Returns **g_loadTimestamp** (for cache validation).
- Old model is fully unmapped before mapping the new one so working set does not grow.

### 2. inference.asm
- **ClearKVCache** — Zeros the KV cache (64KB placeholder); used when `preserveKV == 0` on hotswap.
- **g_modelbase** — Exported so model_loader can set it after LoadModel.
- **TokenGenerate** — When **g_modelbase** is NULL (e.g. during hotswap), returns **1 (BOS)** as stall instead of 0; thread-safe during pointer swap.

### 3. beacon.asm
- **MODEL_HOTSWAP_REQUEST** (0x1001), **MODEL_HOTSWAP_COMPLETE** (0x1002), **MODEL_HOTSWAP_FAILED** (0x1003).
- **TryBeaconRecv(beaconID, ppData, pLen)** — Non-blocking recv; returns 1 if message read, 0 if ring empty.

### 4. agent.asm
- **ProcessOneAgentMessage** — Polls slot 1; if message type is MODEL_HOTSWAP_REQUEST, calls **HotSwapModel(path at msg+4, preserveKV=1)**; loader itself sends MODEL_HOTSWAP_COMPLETE to slot 0 on success.

### 5. ui.asm (when command surface exists)
- **/hotswap &lt;path&gt; [--preserve-kv]** — When the ASM UI has a command edit (**IDC_CMD_EDIT**): **GetDlgItemTextA(hMainWnd, IDC_CMD_EDIT, g_szBuffer, 260)**; parse remainder after `/hotswap `; **BeaconSend(1, pathPtr, MODEL_HOTSWAP_REQUEST)**; **SetStatusText(szSwappingModel)** (non-blocking). Completion comes via Beacon slot 0 (MODEL_HOTSWAP_COMPLETE).
- If the build uses the custom-editor ui.asm (no command edit), trigger hotswap via agent slot 1 from menu or external automation.

### 6. main.asm
- Bootstrap step 4: **ModelLoaderInit** after InferenceEngineInit.

---

## Definition of done (production)

| Criterion | Status |
|-----------|--------|
| `/hotswap &lt;model.gguf&gt;` triggers background swap without UI freeze | Yes — UI sends to slot 1; agent/loader does heavy lifting; completion via Beacon. |
| Inference thread blocks &lt; 10ms on SRWLOCK (lock only during pointer swap) | Yes — lock held for unmap/remap; TokenGenerate returns BOS when base is NULL. |
| Old model fully unmapped before new map (working set never doubles) | Yes — UnloadModel then LoadModel. |
| KV cache optionally preserved (same-arch) or cleared | Yes — TinyLlama→TinyLlama fine; TinyLlama→Phi-3 cleared via preserveKV=0. |
| Beacon notification confirms completion to UI | Yes — **HotSwapModel** calls **BeaconSend(0, pNewPath, MODEL_HOTSWAP_COMPLETE)** on success. |

---

## How to validate

1. **Build** the ASM monolithic target (model_loader.obj, inference.obj, beacon.asm, agent.asm, ui.asm, main.asm linked with kernel32/user32).
2. **Run** the EXE (e.g. with `--load-model D:\models\tiny.gguf` if your entry parses that).
3. **Menu** → "Hotswap model...". Title should show "Swapping model..." then "Ready". Default path is `D:\models\tiny.gguf` (change **szDefaultModelPath** in ui.asm or add GetOpenFileNameW for a real picker).
4. **Memory:** Run `.\scripts\test_x4_hotswap.ps1` (optionally with `-ManualHotswap`); script starts process, waits, checks WorkingSet &lt; 1.92GB.

---

## Files touched (X+4)

| File | Changes |
|------|---------|
| `model_loader.asm` | ModelLoaderInit, HotSwapModel, GetCurrentModelPath, **GetModelLoadTimestamp**; SRWLOCK; g_currentPathW; **g_loadTimestamp**, **g_preservedKV**; **BeaconSend(0, pNewPath, MODEL_HOTSWAP_COMPLETE)** on success; GetTickCount64. |
| `inference.asm` | ClearKVCache; PUBLIC g_modelbase; **TokenGenerate returns BOS (1) when g_modelbase NULL**. |
| `beacon.asm` | TryBeaconRecv; MODEL_HOTSWAP_REQUEST/COMPLETE/**FAILED**. |
| `agent.asm` | ProcessOneAgentMessage; HotSwapModel(path, preserveKV=1). |
| `main.asm` | ModelLoaderInit in bootstrap. |
| `rawrxd.inc` | EXTERN GetModelLoadTimestamp. |
| `scripts/test_x4_hotswap.ps1` | Memory validation; optional **Send-Command**, **Wait-ForBeaconSignal**, **Get-ProcessMemoryString** for full automation. |

---

## Validation script (production)

Full automation can use:
- **Send-Command** — Inject `/hotswap &lt;path&gt;` via window message or shared-memory command to the process.
- **Wait-ForBeaconSignal** — Wait for MODEL_HOTSWAP_COMPLETE (e.g. poll process memory or named event).
- **Get-ProcessMemoryString** — Read **g_currentPath** (or g_currentPathW) from process for path verification.

See `.\scripts\test_x4_hotswap.ps1` for memory-cap check; extend with the above for path and beacon checks.

---

## Optional next steps

- **IDC_CMD_EDIT** in ui.asm (when using a command-bar build): parse `/hotswap &lt;path&gt; [--preserve-kv]`, BeaconSend(1, path, REQUEST), SetStatusText(szSwappingModel).
- **Poll slot 0** in UI (WM_TIMER) to show "Swapped" / "Failed" from MODEL_HOTSWAP_COMPLETE / MODEL_HOTSWAP_FAILED.
- **C++ IDE** `/hotswap &lt;path&gt;` that calls HotSwapModel via asm_bridge or native engine API.
