# RawrXD Foundation Status

**One-line:** Bootstrap, editor, LSP live, agent path, and GGUF hotpatch (X+4) are in place. Next: X+5 Distributed Swarm.

---

## What's in place

| Layer | Status | Notes |
|-------|--------|------|
| **Bootstrap (ASM)** | ✅ | `main.asm`: HeapCreate → BeaconRouterInit → InferenceEngineInit → LSPBridgeInit (stub) → AgentCoreInit → UIMainLoop. See `docs/RUNTIME_BOOTSTRAP_AND_VALIDATION.md`. |
| **Bootstrap (C++)** | ✅ | `main_win32.cpp` WinMain → Win32IDE path; `parseCmdLine` fixed; DPI/crash containment wired. |
| **Layout / Core** | ✅ | `Win32IDE_LayoutCanon.h` in `src/win32app`; `Win32IDE_Core.cpp` builds; four-pane canon documented. |
| **Runtime smoke** | ✅ | `scripts/smoke_runtime.ps1`: launch, window class, memory, WM_CLOSE; optional `-CheckEditor`. |
| **First-token** | ✅ | `scripts/test_first_token.ps1`: `--load-model`, memory cap; optional `-SkipModelLoad`, `-SkipMemoryCheck`. |
| **Editor surface** | ✅ | C++: RichEdit in Win32IDE. ASM: custom editor (ui.asm) or Edit control per build. Optional: `validate_editor_surface.ps1`. |
| **One-command validation** | ✅ | `scripts/run_all_validation.ps1`: smoke → editor check → first-token (quick or full). |
| **LSP live (X+2)** | ✅ | `openFile` sends didOpen (auto-start LSP); diagnostics → annotations; Tools → LSP: Go to Def (F12), Find Refs (Shift+F12), Rename, Hover, Signature Help, Quick Fix (Ctrl+.), Highlight Refs; Format Document (Shift+Alt+F), Format on Save. See `docs/PHASE_X2_LSP_LIVE.md`. |
| **Agent loop (X+3)** | ✅ | `/agent` and `/ask` run via AgenticBridge when initialized; fallback to NativeAgent. See `docs/PHASE_X3_AGENT_LOOP.md`. |
| **GGUF hotpatch (X+4)** | ✅ | ASM monolithic: HotSwapModel, menu "Hotswap model...", SRWLOCK, working set &lt; 1.92GB. See `docs/PHASE_X4_GGUF_HOTPATCH.md`. |

---

## Single command to validate

```powershell
cd D:\rawrxd
.\scripts\run_all_validation.ps1
```

- Quick (no model, no memory fail): default.
- Full first-token: `.\scripts\run_all_validation.ps1 -FullFirstToken`
- Skip first-token: `.\scripts\run_all_validation.ps1 -SkipFirstToken`

---

## Next (in order)

1. **Phase X+5 — Distributed swarm**  
   Multi-GPU inference sharding. Design + stub: `docs/PHASE_X5_DISTRIBUTED_SWARM.md`, `include/inference_shard_coordinator.h`. Next: adapter enumeration, then layer split.

---

## If something breaks

- **Build:** `Win32IDE_Core.cpp` "too many errors" → ensure `src/win32app/Win32IDE_LayoutCanon.h` exists (layout canon).
- **Runtime:** Run `.\scripts\smoke_runtime.ps1`; if it fails, see `docs/RUNTIME_BOOTSTRAP_AND_VALIDATION.md` §7 Troubleshooting.
- **Editor:** `.\scripts\smoke_runtime.ps1 -CheckEditor` or `.\scripts\validate_editor_surface.ps1`.
- **LSP:** Start LSP (e.g. Tools → LSP → Start All), then open a C++ file; clangd on PATH. See `docs/PHASE_X2_LSP_LIVE.md`.
- **Agent:** `/agent` / `/ask` use AgenticBridge if initialized, else NativeAgent if model loaded. See `docs/PHASE_X3_AGENT_LOOP.md`.
- **Hotpatch (ASM):** Menu "Hotswap model..." or script `.\scripts\test_x4_hotswap.ps1`. See `docs/PHASE_X4_GGUF_HOTPATCH.md`.

You're on a solid foundation; next step is Phase X+5 (Distributed swarm).
