# Runtime Integration & First Token — Bootstrap and Validation

**Goal:** Move from "compiles clean" to "runs alive." Confirm the 7 modules initialize in the correct order, the main window appears, and (optionally) the first-token pipeline runs within the 1.92GB memory target.

---

## 1. Bootstrap sequence (canonical init order)

`WinMain` (or ASM `main.asm` entry) must initialize in this **strict dependency order**:

| Step | Call | Purpose |
|------|------|--------|
| 1 | **HeapInit** (or `HeapCreate`) | Process infrastructure — arena for all subsystems (e.g. 64MB). |
| 2 | **BeaconRouterInit** | Ring buffers allocated; slots 0–15 cleared. All other modules signal through this. |
| 3 | **ModelLoaderInit** | File-mapping subsystem; no deps except heap. |
| 4 | **InferenceEngineInit** | AVX-512 detection, Vulkan init, KV cache; depends on ModelLoader, Beacon slot 2. |
| 5 | **LSPBridgeInit** | Depends on Beacon slot 3; spawns child process (e.g. clangd). |
| 6 | **AgentCoreInit** | Depends on Inference + Beacon; worker thread spawned, waits on BeaconRecv. |
| 7 | **UIMainLoop** | Depends on all; creates window, blocks on message pump. Does not return until WM_QUIT. |

Shutdown is the **reverse order**: AgentShutdown → LSPBridgeShutdown → InferenceCleanup → (then heap/window cleanup).

### Current implementations

- **ASM monolithic** (`src/asm/monolithic/main.asm`): Uses `HeapCreate` (step 1), then `BeaconRouterInit`, `InferenceEngineInit`, `LSPBridgeInit` (stub), `AgentCoreInit`, `UIMainLoop`. ModelLoader not yet in the ASM chain.
- **C++ IDE** (`src/win32app/main_win32.cpp`): Full Win32IDE stack (no separate ASM Beacon/Inference); equivalent init is inside the C++ startup path.

---

## 2. Runtime smoke test

**Script:** `scripts/smoke_runtime.ps1`

- Resolves EXE: `build\monolithic\RawrXD.exe`, `build\bin\RawrXD-Win32IDE.exe`, or similar.
- **Test 1:** Launch; process must survive 800 ms (no init crash).
- **Test 2:** Find main window by class: `RawrXD_Main`, `RawrXD_Monolithic`, or `RawrXD_IDE_MainWindow` within 5 s.
- **Optional -CheckEditor:** After Test 2, find an Edit or RICHEDIT50W child (Phase X+1 editor surface).
- **Test 3:** Memory footprint within 1.92 GB target.
- **Test 4:** Clean shutdown via WM_CLOSE (no zombie).

```powershell
cd D:\rawrxd
.\scripts\smoke_runtime.ps1
.\scripts\smoke_runtime.ps1 -ExePath ".\build\monolithic\RawrXD.exe" -CheckEditor
.\scripts\smoke_runtime.ps1 -NoWindow
```

If this fails, the wiring between modules likely has a calling-convention or init-order issue (e.g. Beacon function pointers).

---

## 3. First-token validation

**Script:** `scripts/test_first_token.ps1`

- Starts IDE with `--load-model <path>` (default `D:\models\tiny.gguf`).
- Waits for model load (default 2 s).
- Asserts working set &lt; 1.92 GB; then terminates the process.

```powershell
.\scripts\test_first_token.ps1
.\scripts\test_first_token.ps1 -ModelPath "D:\models\tiny.gguf" -LoadWaitSeconds 3 -MaxMemoryGB 1.92
.\scripts\test_first_token.ps1 -SkipModelLoad -SkipMemoryCheck   # Fast: launch only, no model, no memory fail
```

Place a TinyLlama-1.1B GGUF (e.g. q4_0) at `D:\models\tiny.gguf` for a real first-token pipeline check. **Optional:** `-SkipModelLoad` (launch without `--load-model`), `-SkipMemoryCheck` (do not fail on memory limit).

---

## 4. Phase X+1: Editor surface (done for ASM monolithic)

- **C++ IDE:** Full editor already present (RichEdit in `Win32IDE_Core.cpp` / `Win32IDE.cpp` — createEditor, line numbers, syntax coloring).
- **ASM monolithic** (`src/asm/monolithic/ui.asm`):
  - **WM_CREATE:** Creates a standard **Edit** control (multiline, scroll) as the editor pane; stores `hEditorWnd`; sets focus to it.
  - **WM_SIZE:** Resizes the editor to fill the client area (MoveWindow).
  - **CreateEditorPane:** Uses wide class name `Edit` and parent `hMainWnd`; returns the edit HWND.

So the monolithic build now has a working text buffer (cursor, scroll, typing). No DirectWrite/Uniscribe yet — standard GDI Edit control.

---

## 5. Next phase roadmap

| Phase | Target | Deliverable |
|-------|--------|-------------|
| **X+1** | Editor surface | ✅ Done (ASM: Edit control; C++: RichEdit). |
| **X+2** | LSP live | ✅ Wired: Start LSP → open file → didOpen → publishDiagnostics → annotations. See `docs/PHASE_X2_LSP_LIVE.md`. |
| **X+3** | Agent loop | Autonomous code generation (e.g. `/agent` command). |
| **X+4** | GGUF hotpatch | Runtime model switching without EXE restart. |
| **X+5** | Distributed swarm | Multi-GPU inference sharding. |

---

## 6. Optional: Editor surface validation

**Script:** `scripts/validate_editor_surface.ps1`

Standalone check that the main window has an Edit or RichEdit child (Phase X+1). Exit 0 if found, 1 otherwise. Useful after smoke passes to confirm the monolithic editor pane is present.

```powershell
.\scripts\validate_editor_surface.ps1
.\scripts\validate_editor_surface.ps1 -ExePath ".\build\monolithic\RawrXD.exe" -TimeoutSeconds 5
```

---

## 7. Troubleshooting and optional flags

| Issue | What to try |
|-------|-------------|
| EXE path not found | Set `-ExePath` to full path (e.g. `.\build\bin\RawrXD-Win32IDE.exe` or `.\build\monolithic\RawrXD.exe`). |
| Window never appears | Increase `-TimeoutSeconds`. Check RegisterClassExW/CreateWindowExW (wide strings, `g_hInstance` set). |
| No editor child | Run `.\scripts\smoke_runtime.ps1 -CheckEditor` or `.\scripts\validate_editor_surface.ps1`. Monolithic uses class `Edit`; C++ IDE uses `RICHEDIT50W`. |
| Model file missing | Use `.\scripts\test_first_token.ps1 -SkipModelLoad` for launch-only check; or add `-SkipMemoryCheck` to avoid memory failure. |
| Run without showing window | `-NoWindow` on both smoke and test_first_token (process still runs; useful for CI). |

---

## 8. One-command validation and foundation status

**Single script:** `scripts/run_all_validation.ps1` runs smoke (with optional `-CheckEditor`), then optional editor check, then optional first-token. Default first-token run is quick (`-SkipModelLoad -SkipMemoryCheck`). Use `-FullFirstToken` for full model + memory check.

```powershell
.\scripts\run_all_validation.ps1
.\scripts\run_all_validation.ps1 -FullFirstToken
.\scripts\run_all_validation.ps1 -SkipFirstToken
```

**Foundation summary:** See `docs/FOUNDATION_STATUS.md` for what’s in place and the next steps (LSP live → agent loop → GGUF hotpatch → swarm).

---

## 9. Immediate next action

1. **Run validation:** `.\scripts\run_all_validation.ps1` (or smoke only: `.\scripts\smoke_runtime.ps1`).
2. If it fails: fix init order or calling convention (Beacon/Inference/UI); see §7 Troubleshooting.
3. If it passes: proceed to **Phase X+2: LSP live**.
