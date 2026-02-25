# Continue Forward

Quick pointer for "where we are and what to do next."

---

## Current state

- **Foundation:** Bootstrap, editor, LSP live, agent path, GGUF hotpatch (X+4) — see `FOUNDATION_STATUS.md`.
- **LSP/editor:** Auto-start LSP on open; Format Document (Shift+Alt+F), Format on Save; Tools → LSP: Go to Definition (F12), Find References (Shift+F12), Rename, Hover, Signature Help, Quick Fix (Ctrl+.), Highlight References; diagnostics in editor — see `PHASE_X2_LSP_LIVE.md`.
- **Agent:** `/agent` and `/ask` via AgenticBridge; see `PHASE_X3_AGENT_LOOP.md`.
- **Hotpatch (ASM):** Runtime model swap, Beacon completion, GetModelLoadTimestamp; see `PHASE_X4_GGUF_HOTPATCH.md`.
- **Top 25 audit:** 23 present, 1 partial (DAP), 1 stub; see `TOP_25_AI_IDE_FEATURES_AUDIT.md`.

---

## Validate

```powershell
cd D:\rawrxd
.\scripts\run_all_validation.ps1
```

- Quick: default. Full model + memory: `-FullFirstToken`. Skip first-token: `-SkipFirstToken`.
- X+4 hotpatch memory check: `.\scripts\test_x4_hotswap.ps1` (optional).

---

## Next (in order)

1. **Phase X+5 — Distributed swarm**  
   Multi-GPU inference sharding: design + stub in `docs/PHASE_X5_DISTRIBUTED_SWARM.md` and `include/inference_shard_coordinator.h`. Next: implement adapter enumeration and layer split.

2. **Polish (done)**  
   - Stray `return true` / duplicate returns in `Win32IDE_Commands.cpp`, `Win32IDE_TestExplorerTree.cpp`, `Win32IDE_Tier5Cosmetics.cpp`, `Win32IDE.cpp`.  
   - Rename Symbol: modal "New name:" with selection pre-filled (Tools → LSP → Rename Symbol).  
   - **Bulk polish (by hand):** Duplicate `return true` and stray returns fixed by hand (script disabled). See **`docs/POLISH_NOTES.md`** for the one fix that failed initially (Sidebar `showExtensionDetails` — file was locked/EBUSY) and why; it was applied on retry. Latest: `Win32IDE_Tier1Cosmetics` (WndProc + for-loops), `Win32IDE_Settings`, `Win32IDE_ShortcutEditor`, `Win32IDE_Telemetry`, `Win32IDE_SwarmPanel`. Earlier: Sidebar, TodoManager, Plugins, SourceFilePicker, FileOps, AgentCommands, BackendSwitcher, VSCodeUI, AutonomousAgent, LocalServer, FailureDetector, Tier2 (partial), Commands, main_win32, Core, FlagshipFeatures, SubAgent, AgenticBridge. **Tier2Cosmetics** still has many in-block mangles (see POLISH_NOTES). Do not run `scripts/fix_duplicate_return_true.ps1`.

3. **Further LSP/editor**  
   - Hover on mouse hover (idle timer → `textDocument/hover`).  
   - Ctrl+K Ctrl+I chord for Hover.

4. **DAP end-to-end**  
   - Wire `dap_client` to adapter process + JSON-RPC; populate stack/watch/variables (see `TOP_25_AI_IDE_FEATURES_AUDIT.md`).

---

## Key docs

| Doc | Purpose |
|-----|---------|
| `FOUNDATION_STATUS.md` | What's in place, single validation command, next phase. |
| `PHASE_X2_LSP_LIVE.md` | LSP features, shortcuts, validation. |
| `PHASE_X3_AGENT_LOOP.md` | Agent command path, bridge, validation. |
| `PHASE_X4_GGUF_HOTPATCH.md` | Hotpatch production spec, DoD, validation. |
| `PHASE_X5_DISTRIBUTED_SWARM.md` | Multi-GPU inference sharding (X+5), design, first milestone. |
| `RUNTIME_BOOTSTRAP_AND_VALIDATION.md` | Bootstrap order, smoke, first-token, troubleshooting. |
| `TOP_25_AI_IDE_FEATURES_AUDIT.md` | Feature audit, DAP gap, recommended next. |

---

## Final conclusion on forward

**Production-ready:** Foundation (bootstrap, editor, LSP live, agent loop, GGUF hotpatch), one-command validation, and the LSP/editor surface (Format, Go to Def, Find Refs, Rename, Hover, Quick Fix, diagnostics, completion, etc.) are in place. Top 25 audit: 23 features present, DAP partial (scaffold + panels), one stub (DAP end-to-end).

**Recommended next (pick one to lock focus):**

| Option | Focus | Outcome |
|--------|--------|--------|
| **A. X+5 Distributed Swarm** | Multi-GPU sharding (e.g. RX 7800 XT + second GPU) | 120B+ model inference, scale. |
| **B. DAP end-to-end** | Adapter process + JSON-RPC → stack/watch/variables | Full debugging parity. |
| **C. LSP/editor polish** | Hover on idle, Ctrl+K Ctrl+I chord | Smoother daily use. |

**Single validation command:** `.\scripts\run_all_validation.ps1` (from repo root). Use this to confirm the foundation before starting the chosen next track.
