# RawrXD IDE — Assessment: Finishing What Exists (No New Features)

**Goal:** Make the IDE **finally useable** by completing everything that is already present—no new features, no scope creep.

**Date:** 2026-02-20

---

## 1. Executive Summary

| Aspect | Status | Action |
|--------|--------|--------|
| **Build** | CMake + MSVC; Win32 IDE target exists | One canonical build path; fix script roots |
| **Win32 IDE** | 4-pane layout (Explorer, Terminal, Editor, AI Chat); 170+ commands | Finish critical-path stubs only |
| **Feature manifest** | ~200+ features; many REAL for Win32, many STUB/MISSING for CLI/React/PS | Ignore other variants; focus Win32 “launch → edit → terminal → chat” |
| **Validation** | VALIDATE_REVERSE_ENGINEERING.ps1, Validate-* scripts | Run from repo root; ensure 18/18 or document known skips |

**Definition of “useable”:**

- Build completes (RawrXD-Win32IDE or equivalent).
- IDE launches without crash.
- User can: open a folder/file, use editor, open terminal, use AI chat (with or without loading a model).
- No requirement to add new capabilities—only wire or fix what is already there.

---

## 2. What Is Already in Place

### 2.1 Build System

- **Root:** `CMakeLists.txt` (C++20, MSVC, Windows SDK 10.0.22621.0).
- **Main targets:**
  - `RawrEngine` — CLI + HTTP API (port 8080).
  - `RawrXD-Win32IDE` — Full Win32 GUI (four-pane rule: File Explorer, Terminal & Debug, Editor, AI Chat).
  - `rawrxd-monaco-gen` — Vite/Monaco/React IDE codegen.
- **Win32 IDE** links: `WIN32IDE_SOURCES` (1808+), ASM kernels, `sidebar_pure.obj` (from `Win32IDE_Sidebar_Pure.asm`), optional Genesis P0 MASM, QuickJS or stub.
- **Sidebar:** C++ implementation in `src/win32app/Win32IDE_Sidebar.cpp` (activity bar, tree, etc.). Optional path: `Win32IDE_SidebarBridge.cpp` + Pure ASM (`Sidebar_Init`, `Sidebar_Create`) — symbols match; no change needed for “finish.”

### 2.2 Architecture (from ARCHITECTURE.md)

- **Panes:** File Explorer (left), Terminal & Debug (bottom), Editor (main), AI Chat (right). Everything else = pop ups.
- **Agentic core:** AgenticEngine, SubAgentManager, failure recovery, policy engine.
- **Hotpatch:** 3-layer (memory, byte-level, server) + proxy; commands 9001–9017.
- **Command routing:** `routeCommand()` in `Win32IDE_Commands.cpp` by ID range (1000–9999).

### 2.3 Reports / Validation

- `reverse_engineering_reports/completeness_analysis.json` — CompletenessScore 100; paths reference old workspace (`d:\lazy init ide`).
- `reverse_engineering_reports/integration_manifest.json` — Lists AI Model Loader, Agentic Core, MCP, Chat Panel, Editor (Qt-era names); still useful as checklist.
- `VALIDATE_REVERSE_ENGINEERING.ps1` — 6 sections; expects `REVERSE_ENGINEERING_MASTER.ps1`, scripts, `src/`, `include/`, BUILD_ORCHESTRATOR, etc.

---

## 3. Gaps That Block “Useable” (Finish Only)

### 3.1 Build / Scripts

| Item | Detail | Fix |
|------|--------|-----|
| **Project root in scripts** | `BUILD_ORCHESTRATOR.ps1` and `BUILD_IDE_FAST.ps1` use `$ProjectRoot = 'd:\lazy init ide'`. | Use repo root (e.g. `Split-Path $PSScriptRoot -Parent` or `Get-Location`) so they work from `D:\rawrxd`. |
| **Single canonical build** | Multiple build scripts (BUILD_ORCHESTRATOR, BUILD_IDE_FAST, BUILD_IDE_PRODUCTION, cmake directly). | Document **one** recommended sequence (e.g. `cmake -B build -G "Visual Studio 17 2022" -A x64` then `cmake --build build --config Release --target RawrXD-Win32IDE`) and point README / validation at it. |
| **Validate script paths** | VALIDATE_REVERSE_ENGINEERING uses `$ProjectRoot`; if unset, may use wrong dir. | Ensure it uses current repo root when run from `D:\rawrxd`. |

### 3.2 Critical Path (Launch → Edit → Terminal → Chat)

| Component | Status | What “finish” means |
|-----------|--------|----------------------|
| **Launch** | Main: `main_win32.cpp` → Win32IDE. | Ensure no missing DLLs or ASM symbols at link. Sidebar: C++ sidebar + optional Pure ASM bridge already aligned (Sidebar_Create). |
| **File / Explorer** | File ops (open/save/close), tree in `Win32IDE.cpp` / `Win32IDE_FileOps.cpp`, `m_hwndFileExplorer`. | No new features; fix any crash or obvious “not wired” in open/save/close. |
| **Editor** | Tabs, syntax (Win32IDE_SyntaxHighlight), LSP client (Win32IDE_LSPClient, RawrXD_LSPServer). | Ensure open file shows in editor and basic edit works; LSP can remain partial. |
| **Terminal** | Win32TerminalManager, PowerShell panel. | Ensure “New Terminal” or panel opens and runs shell; no new terminal features. |
| **AI Chat** | Win32IDE_AgenticBridge, streaming (Win32IDE_StreamingUX), model load (File → Load GGUF / backend). | Ensure chat pane accepts input and either shows “no model” or uses loaded backend; no new AI features. |

### 3.3 Stubs / TODOs That Matter for “Useable”

- **Compiler/toolchain (e.g. `toolchain_bridge.cpp`):** TODOs about “exact wiring” for asm_lexer/parser and section_merge — only fix if a **documented** use case (e.g. “Compile ASM from IDE”) is required for “useable”; otherwise leave as-is.
- **Vision/Vulkan:** `vision_gpu_staging.cpp` “not implemented on this platform” — acceptable; no new features.
- **LSP/server:** Many features marked MISSING for CLI/React/PS; Win32 has real or partial. Focus: Win32 LSP “good enough” for open file (e.g. no crash); no requirement to implement every LSP feature.
- **Decompiler / RE / Hotpatch panels:** Can remain STUB/PARTIAL as long as they don’t crash when opened; no new features.

### 3.4 What NOT to Do

- Do **not** add new panes, new command ranges, or new subsystems.
- Do **not** implement every STUB in the feature manifest for all variants (CLI, React, PowerShell).
- Do **not** refactor architecture or add new dependencies for “finish.”

---

## 4. Recommended Finish Order

1. **Scripts & build (Day 1)**  
   - Set `$ProjectRoot` (or equivalent) to repo root in `BUILD_ORCHESTRATOR.ps1` and `BUILD_IDE_FAST.ps1`.  
   - Document in README one canonical build (e.g. CMake configure + build RawrXD-Win32IDE).  
   - Run `VALIDATE_REVERSE_ENGINEERING.ps1` from repo root and fix any path-dependent failures.

2. **Build & link (Day 1–2)**  
   - From repo root: configure and build `RawrXD-Win32IDE`.  
   - Fix any link errors (e.g. missing ASM export, wrong library).  
   - Confirm IDE executable starts (window appears, no immediate crash).

3. **Critical path (Day 2–3)**  
   - **Open file:** File → Open (or equivalent) opens a file in the editor.  
   - **Save/close:** Save and close don’t crash; state is consistent.  
   - **Terminal:** “New Terminal” or terminal panel starts a shell.  
   - **Chat:** AI chat pane accepts text; with/without model shows appropriate message (e.g. “Load a model” vs. streaming).  

4. **Stability (Day 3–4)**  
   - Run IDE for 5–10 minutes: open files, switch tabs, run terminal, type in chat.  
   - Fix crashes or freezes only in this path; leave non–critical-path stubs as-is.  

5. **Docs & validation (Day 4)**  
   - README: “How to build and run RawrXD IDE” (one path).  
   - Optional: Short “Definition of done” (e.g. “Build + launch + open file + terminal + chat”) and note known limitations (e.g. “LSP partial,” “Decompiler stub”).

---

## 5. Success Criteria (Definition of Done)

- [ ] One documented build command (or script) produces `RawrXD-Win32IDE` (or equivalent exe) from `D:\rawrxd`.  
- [ ] IDE launches; main window shows with four-pane layout (or clearly degraded but non-crashing layout).  
- [ ] User can open a file and see it in the editor.  
- [ ] User can save and close the file.  
- [ ] User can open a terminal (or terminal panel) and run commands.  
- [ ] User can type in the AI chat pane and get a defined behavior (e.g. “no model” or response from loaded backend).  
- [ ] No **new** features added; only existing code paths fixed or wired to meet the above.

---

## 6. References

- `ARCHITECTURE.md` — System overview, directory layout, four-pane rule, hotpatch, commands.  
- `src/win32app/Win32IDE_FeatureManifest.cpp` — Feature matrix (Real/Partial/Facade/Stub/Missing) per variant.  
- `reverse_engineering_reports/completeness_analysis.json` — Legacy completeness; paths may need mental map to current repo.  
- `reverse_engineering_reports/integration_manifest.json` — Integration points (AI Model Loader, Agentic Core, MCP, Chat, Editor).  
- `.cursorrules` — C++ style (std::expected, Logger, no Qt/std mix in core), project structure, build scripts.

---

*Assessment complete. Proceed with finish order above; re-evaluate only if “useable” definition changes.*
