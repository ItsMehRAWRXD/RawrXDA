# Feature Parity: Now vs Then — Where the IDE Sits vs the Archive Index

**Comparison:** `docs/archive/FEATURE_PARITY_INDEX.md` (Feb 6, 2026) vs current codebase.  
**Purpose:** Bridge from “then” (45-command parity snapshot) to “now”; list what is still not visible in that index.

---

## 1. Snapshot Comparison

| Metric | Then (Feb 6, 2026) | Now |
|--------|--------------------|-----|
| **CLI** | 710 lines, 45 commands | **~1,538 lines** — profile, subagent, chain, swarm, cot, search, analyze, route_command, etc. |
| **Win32 IDE** | 6,279 lines, 45+ commands | **Win32IDE.cpp ~6,748 lines**; **170+ commands**; **127 .cpp files** in `win32app/` (index said “44+”) |
| **Parity** | Single 45-command set, CLI ↔ Win32 | **Original 45-command parity still holds**; the IDE has many more commands and modules on top |

---

## 2. What the Archive Index Described (Still True)

- **45-command inventory** — File, Editor, Agentic, Autonomy, Debug, Terminal, Hotpatch, Search/Tools, Config, Status/Help.
- **CLI ↔ Win32 parity** for that core set.
- **Design principles** — shared state, thread safety, extensibility, automation-friendly.
- **Verification checklist** — implementation and testing for those 45.

So: the *baseline* parity (45 commands, same behavior in CLI and GUI for that set) is still the floor. The index is **not wrong**; it is **incomplete** relative to what exists today.

---

## 3. What’s Matured Since (Not in the Index)

These exist in the codebase but were **not** in the Feature Parity Index:

| Area | What exists now | Where |
|------|-----------------|--------|
| **Four-pane layout** | Exactly 4 main panes; everything else = pop up (Qt: widget). Pop ups float in own layer, not embedded. | `ARCHITECTURE.md` §6.5, `Win32IDE_LayoutCanon.h` |
| **Marketplace** | Extension marketplace panel, seed catalog, native “Extension Host Hijacker” entry, offline/local. | `Win32IDE_MarketplacePanel.cpp`, `cmdMarketplace*`, IDM_MARKETPLACE_* |
| **Agent tools** | hijacker_status / hijacker_debug / hijacker_troubleshoot; GetAvailableTools() extended; bridge runs hijacker commands without model/network. | `Win32IDE_AgenticBridge.cpp` |
| **Hotpatch** | Three-layer hotpatch (Memory, Byte, Server) + Proxy; menu and command IDs 9001–9017; HotpatchPanel, HotpatchWiring, Beacon integration. | `Win32IDE_HotpatchPanel.cpp`, `Win32IDE_HotpatchWiring.cpp`, `unified_hotpatch_manager`, `proxy_hotpatcher` |
| **Vision hotpatch (design)** | Give “vision” to non-vision models by injecting IDE view (accessibility tree, pane map) into context. | `ARCHITECTURE.md` §6.6 |
| **Command scale** | 170+ commands; 216+ `case IDM_*` in command router; ranges for File, Edit, View, Terminal, Agent, Autonomy, Modules, Help, Audit, Git, Hotpatch, Marketplace, etc. | `Win32IDE_Commands.cpp`, `Win32IDE.h` (IDM_*) |
| **Win32 app surface** | 127 .cpp files in win32app (LSP, MCP, SubAgent, Swarm, NativeDebug, ReverseEngineering, StreamingUX, BackendSwitcher, LLMRouter, ExecutionGovernor, etc.). | `src/win32app/*.cpp` |
| **Layout canon** | MainPane enum, MAIN_PANE_COUNT=4, IsMainPaneControlId, pop-up rules (Qt = widget, float, no overwrite). | `Win32IDE_LayoutCanon.h` |
| **Extension Host Hijacker** | Beacon-style injector; build script; deploy; documented as marketplace-accessible, debuggable offline. | `extension-host/`, `Win32IDE_MarketplacePanel.cpp` seed |

So the **IDE and its matured source** have grown well beyond the “45 commands + parity” snapshot; the index still describes only that snapshot.

---

## 4. What the Index Doesn’t Show (Still “Not Seen”)

These are **still not visible** in the archive index:

| Item | Description |
|------|-------------|
| **Four-pane layout** | Only 4 main panes; everything else is a **pop up** (Qt = widget), floating in its own layer. |
| **Full command surface** | No single doc that lists all 170+ commands and their categories. |
| **Marketplace** | Panel, seed catalog, **Extension Host Hijacker** as a debuggable native entry. |
| **Agent tools** | `hijacker_status` / `hijacker_debug` / `hijacker_troubleshoot`; **offline, no model/network required**. |
| **Hotpatch** | Three-layer + proxy, **17+ command IDs**; HotpatchPanel, Beacon wiring. |
| **Vision hotpatch** | Design in ARCHITECTURE §6.6; not in the index. |
| **Perma hotpatcher** | Desired “permanent” model-capability patching; not in index, **not implemented**. |
| **Offline / no-cloud** | Hijacker and agent tools designed for off-grid; not called out in the index. |
| **CLI vs Win32 drift** | New IDE-only commands (marketplace, many hotpatch actions, panels) **without CLI equivalents**. |

The archive index remains a correct but **partial** view of the original 45-command parity; it does not see the full IDE surface above.

---

## 5. Summary Table

| Dimension | Then (index) | Now (codebase) | Still not seen in index |
|-----------|--------------|----------------|--------------------------|
| CLI size | 710 lines | ~1,538 lines | Subagent, chain, swarm, cot, search, analyze, etc. |
| Win32 core | 6,279 lines | ~6,748+ (one file) | — |
| Win32 surface | 44+ files | 127 .cpp files | LSP, MCP, panels, hotpatch, marketplace, etc. |
| Commands | 45 shared | 45 shared + 170+ total in IDE | Full command list, categories |
| Layout | — | Four panes, pop ups = widgets, float layer | §6.5, LayoutCanon |
| Marketplace | — | Panel, seed, Hijacker entry | — |
| Agent tools | — | hijacker_*, tools list | Offline debuggability |
| Hotpatch | 2 commands | Three-layer + proxy, 17+ IDs | Full hotpatch surface |
| Vision / perma hotpatch | — | Design / desired | Implementation status |

---

## 6. Recommendation

- **Keep** the archive index (`docs/archive/FEATURE_PARITY_INDEX.md`) as the **historical 45-command parity snapshot**.
- **Use this doc** (`docs/FEATURE_PARITY_NOW_VS_THEN.md`) as the **bridge from “then” to “now”** and the list of what’s still not visible in that index.
- **Optional:** Add a “Current feature surface” doc that lists all 170+ command ranges and major features (four panes, marketplace, hotpatch, agent tools, offline) so the full IDE is visible in one place.

---

**Version:** 1.1  
**Date:** 2026-02-20  
**References:** `docs/archive/FEATURE_PARITY_INDEX.md`, `ARCHITECTURE.md` §6.5–6.6, `Win32IDE_LayoutCanon.h`, `docs/TODO_MANIFEST.md`
