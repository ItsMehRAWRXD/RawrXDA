# Top-50 Readiness — Single Passthru

**Purpose:** One pass-through spec for closing the gap to top-tier security tools, AI IDEs, compilers, and debuggers. Each item is a concrete missing piece: ID, what, where, priority. Execute in order or by P0 → P1 → P2.

**Reference bar:** SonarQube/Checkmarx/Snyk (security); VS Code/Visual Studio (IDE); LLVM/MSVC (compilers); WinDbg/x64dbg (debuggers); Cursor/Copilot (AI IDE).

---

## 1. SECURITY — PASSTHRU

**Already in tree:** RE vuln detection (`Win32IDE_ReverseEngineering.cpp` → `handleReverseEngineeringDetectVulns`), codebase audit (`src/audit/codebase_audit_system.cpp`), static analysis CFG/SSA (`src/core/static_analysis_engine.cpp`, `Win32IDE_StaticAnalysisPanel.cpp`), security stubs (`src/security/`), tamper detection (`src/security/tamper_detection.cpp`).

- **S1 [P0]** SAST rule engine — C/C++ vuln rules (buffer overflow, format string, use-after-free, SQLi). **Add:** `src/security/sast_rule_engine.cpp` + `src/security/sast_rules/`. Wire to static_analysis_engine IR/AST; emit to LSP/Problems.
- **S2 [P0]** Secrets scanner — API keys, passwords, tokens in repo (regex + entropy). **Add:** `src/security/secrets_scanner.cpp`; hook on File Open/Save or Tools.
- **S3 [P0]** SCA / dependency audit — parse package.json, requirements.txt, CMakeLists, vcpkg; match CVE DB. **Add:** `src/security/dependency_audit.cpp`; optional NVD/OSV JSON.
- **S4 [P1]** SBOM export — CycloneDX/SPDX from build manifest. **Add:** `src/security/sbom_export.cpp`; Build menu or CLI.
- **S5 [P2]** DAST hook — import external DAST (ZAP/Burp) into Problems. **Add:** `src/security/dast_bridge.cpp` or Import DAST report in UI.
- **S6 [P1]** Security dashboard — SAST + SCA + secrets counts, trend. **Add:** `src/win32app/Win32IDE_SecurityDashboard.cpp` or Audit tab.
- **S7 [P2]** Real HSM/FIPS/audit log — replace XOR/placeholders in `src/security/*.cpp` (see UNFINISHED_FEATURES.md).

---

## 2. AI FRAMEWORK — PASSTHRU

**Already in tree:** Multi-backend LLM, agent loop, tool-calling, SubAgent, Ghost Text, LSP+AI bridge, deep thinking/research, failure detection, autonomy (Win32IDE_LLMRouter, AgentCommands, SubAgent, GhostText, LSP_AI_Bridge, Autonomy, etc.).

- **A1 [P0]** Agent panel host — `initAgentPanel()` is stub; no visible chat/diff host HWND. **Fix:** `Win32IDE_AgentPanel.cpp` (COMPREHENSIVE_AUDIT: Agent Diff logic exists, no host HWND).
- **A2 [P1]** Structured tool schema — tools as JSON schema (name, params, types) for agent. **Extend:** `Win32IDE_AgentCommands.cpp` or RawrXD_ToolRegistry.
- **A3 [P0]** Codebase RAG — embed repo chunks; "where is X" / context for prompts. **Add:** `src/ai/codebase_rag.cpp` or extend Semantic panel.
- **A4 [P1]** Prompt templates — user-defined system prompts per project/global. **Add:** `src/win32app/Win32IDE_PromptTemplates.cpp` or Settings.
- **A5 [P0]** Streaming to agent panel — token stream to visible agent chat (not only Output). **Fix:** `Win32IDE_AgentPanel.cpp` + streaming callback (depends on A1).
- **A6 [P2]** Multi-agent orchestration UI — roles, handoffs, debate. **Extend:** `Win32IDE_SubAgent.cpp` or new panel.

---

## 3. IDE — PASSTHRU

**Already in tree:** RichEdit, tabs, syntax highlight, file explorer, terminal, sidebar, command palette, file/symbol search, LSP client, Git, session save, Marketplace, Swarm build (Win32IDE, Tier1Cosmetics, Sidebar, LSPClient, Commands, Session, MarketplacePanel, SwarmPanel).

- **I1 [P0]** Go to definition / references — LSP provides; ensure UI shows in editor (click symbol → definition). **Wire:** `Win32IDE_LSPClient.cpp` + editor handler.
- **I2 [P0]** Inline diagnostics — squiggles and margin markers from LSP/SAST. **Verify:** `Win32IDE_Annotations.cpp` or editor paint; displayDiagnosticsAsAnnotations path.
- **I3 [P0]** Unified Problems panel — one list (LSP + SAST + SCA + secrets) with file/line/severity/source. **Add:** `Win32IDE_ProblemsPanel.cpp` or extend Output tab.
- **I4 [P0]** Build pipeline — run CMake/Ninja from IDE; capture output; parse errors → Problems. **Add/extend:** `Win32IDE_Commands.cpp` or `Win32IDE_BuildRunner.cpp`.
- **I5 [P1]** Test explorer — discover tests (gtest, pytest); run/debug from tree. **Extend:** `Win32IDE_TestExplorerTree.cpp`.
- **I6 [P1]** Refactor: rename symbol — LSP rename; apply WorkspaceEdit across files. **Wire:** LSP client + WorkspaceEdit handler.
- **I7 [P2]** Minimap / scrollbar code preview. **Verify:** Tier1; complete if missing.
- **I8 [P2]** Breadcrumbs — namespace > class > function. **Verify:** `Win32IDE_Breadcrumbs.cpp` full wiring.

---

## 4. COMPILER / BUILD — PASSTHRU

**Already in tree:** Phase 1 assembler, Phase 2 linker (PE), MASM64/RE compile, Swarm (CMake DAG), Game Engine build, CLI compiler (phase1_assembler, phase2_linker, Win32IDE_ReverseEngineering, SwarmPanel, GameEnginePanel, rawrxd_cli_compiler).

- **C1 [P0]** Single "Build" command — configure + build (CMake + Ninja) for current folder; output in Build tab; errors → Problems. **Add:** `Win32IDE_BuildRunner.cpp` or extend Commands.
- **C2 [P1]** Build configurations — Debug/Release, ASan, coverage. **Add:** Settings or project config file.
- **C3 [P1]** compile_commands.json — ensure generated and consumed for LSP/clangd. **Wire:** LSP + Swarm/CMake path.
- **C4 [P2]** Incremental build — only rebuild changed; use ninja/msbuild incremental. **Extend:** Build runner.

---

## 5. DEBUGGER — PASSTHRU

**Already in tree:** Native debug engine (DbgEng), launch, attach, breakpoints, step, registers, stack, memory, disasm, RE integration (native_debugger_engine, Win32IDE_NativeDebugPanel, Win32IDE_Debugger).

- **D1 [P0]** Source-level stepping — map RIP to file:line; highlight current line in editor. **Wire:** native_debugger_engine + symbol resolution + editor; enableSourceStepping.
- **D2 [P0]** Variables view — locals/globals at current frame. **Ensure:** `Win32IDE_Debugger.cpp` → updateVariables() populated from engine.
- **D3 [P1]** Watch expressions — UI to add/edit/remove; engine has addWatch/updateWatches. **Add:** Debugger UI watch list.
- **D4 [P1]** Conditional breakpoints — engine has addConditionalBreakpoint; UI for condition. **Add:** Debugger UI.
- **D5 [P2]** Memory breakpoints — engine addDataBreakpoint; UI for address + size.
- **D6 [P1]** Call stack with source — frames with file:line; click → open file at line. **Wire:** Win32IDE_Debugger stack list + editor.
- **D7 [P1]** Disassembly view — dedicated pane; sync with RIP. **Add:** New pane or reuse RE disasm.

---

## 6. REVERSE ENGINEERING — PASSTHRU

**Already in tree:** Analyze, Disassemble, DumpBin, CFG, Functions, SSA, recursive disasm, type recovery, data flow, decompiler (D2D), export IDA/Ghidra, detect vulns, set binary from debug/build/doc, Disassemble at RIP (Win32IDE_ReverseEngineering, Win32IDE_DecompilerView, RawrCodex, RawrDumpBin, RawrCompiler).

- **R1 [P1]** ELF / Mach-O loaders — PE-only today. **Add:** in `src/reverse_engineering/` (RawrCodex, RawrDumpBin). RE_ARCHITECTURE.md.
- **R2 [P2]** Omega suite / deobfuscator — consolidate; MASM64 port; in-IDE "Deobfuscate". **Add:** `src/reverse_engineering/deobfuscator/`, menu. UNFINISHED_FEATURES.
- **R3 [P1]** RE vulnerability rules — dangerous APIs, unchecked bounds; link to SAST-style rules. **Extend:** `handleReverseEngineeringDetectVulns` + rule set.

---

## 7. TESTING / QUALITY — PASSTHRU

**Already in tree:** Codebase audit, Test explorer (partial), Fuzz/coverage CLI (rawrxd_subsystem_api — GapFuzz, BBCov, CovFusion); T3-F in TIER_2_TO_TIER_3_ROADMAP.h.

- **T1 [P1]** Fuzz harnesses — libFuzzer/AFL for GGUF, agent tools, hotpatch. **Add:** `test/fuzz/` per T3-F.
- **T2 [P1]** Coverage in IDE — run tests with coverage; % per file in UI/report. **Add:** coverage runner + viewer.
- **T3 [P1]** Unit test runner in IDE — run gtest/catch2 from Test Explorer; show results. **Extend:** `Win32IDE_TestExplorerTree.cpp` + run delegate.
- **T4 [P2]** CI fuzz gate — e.g. `ci/fuzz_nightly.yml` or `.github/workflows`.

---

## 8. FILES TO ADD (CHECKLIST)

| Path | Purpose |
|------|--------|
| `src/security/sast_rule_engine.cpp` | SAST rules + run on IR/source |
| `src/security/secrets_scanner.cpp` | Secrets in buffers/repo |
| `src/security/dependency_audit.cpp` | SCA: deps + CVE |
| `src/security/sbom_export.cpp` | SBOM export |
| `src/security/dast_bridge.cpp` | Optional DAST import |
| `src/win32app/Win32IDE_SecurityDashboard.cpp` | Security dashboard UI |
| `src/ai/codebase_rag.cpp` | Codebase indexing + RAG |
| `src/win32app/Win32IDE_PromptTemplates.cpp` | Saved prompts/templates |
| `src/win32app/Win32IDE_ProblemsPanel.cpp` | Unified Problems (or extend existing) |
| `src/win32app/Win32IDE_BuildRunner.cpp` | Single Build + error parse |
| `test/fuzz/fuzz_gguf_parser.cpp` | Fuzz GGUF path |
| `test/fuzz/fuzz_agent_tools.cpp` | Fuzz agent tools |
| `test/fuzz/fuzz_hotpatch.cpp` | Fuzz hotpatch |

---

## 9. PRIORITY PASSTHRU (EXECUTION ORDER)

**P0 (must-have for "top 50"):** S1, S2, S3 · A1, A3, A5 · I1, I2, I3, I4 · C1 · D1, D2.

**P1 (differentiators):** S4, S6 · A2, A4 · I5, I6 · C2, C3 · D3, D4, D6, D7 · R1, R3 · T1, T2, T3.

**P2 (polish):** S5, S7 · A6 · I7, I8 · C4 · D5 · R2 · T4.

Implementing **P0** closes the main gap; **P1** solidifies differentiation; **P2** adds polish.
