# Top-50 Readiness: Missing Code & Features

**Goal:** Position RawrXD among the best security tools, AI frameworks, IDEs, compilers, and debuggers. This document lists **concrete missing code** and features, with locations and priority.

**Reference:** Top-tier tools in each category (e.g. SonarQube/Checkmarx/Snyk for security; VS Code/Visual Studio for IDE; LLVM/MSVC for compilers; WinDbg/x64dbg for debuggers; Cursor/Copilot for AI IDE) set the bar.

---

## 1. SECURITY TOOLS (SAST / DAST / SCA / Secrets)

### 1.1 What Exists

| Feature | Location | Status |
| --------- | ---------- | -------- |
| RE vulnerability detection (binary) | `Win32IDE_ReverseEngineering.cpp` → `handleReverseEngineeringDetectVulns()` | Real — runs on loaded binary |
| Codebase audit (regex patterns, memory leak hints) | `src/audit/codebase_audit_system.cpp` | Real — not SAST-grade |
| Static analysis engine (CFG/SSA) | `src/core/static_analysis_engine.cpp`, `Win32IDE_StaticAnalysisPanel.cpp` | Real — CFG build, no vulnerability rules |
| Security feature stubs | `src/security/` (HSM, FIPS, audit_log_immutable, sovereign_keymgmt) | Partial — placeholders |
| Tamper detection | `src/security/tamper_detection.cpp` | Real — checksum/SHA-256 |

### 1.2 Missing Code (Security)

| # | Missing | Expected in | Priority | Notes |
| --- | --------- | ------------- | ---------- | -------- |
| **S1** | **SAST rule engine** — C/C++ vulnerability rules (buffer overflow, format string, use-after-free, SQLi patterns) | New: `src/security/sast_rule_engine.cpp` + rules in `src/security/sast_rules/` | P0 | Wire to `static_analysis_engine` IR or AST; emit diagnostics to LSP/Problems panel. |
| **S2** | **Secrets scanning** — detect API keys, passwords, tokens in repo (e.g. regex + entropy) | New: `src/security/secrets_scanner.cpp`; hook in File Open/Save or Tools menu | P0 | Top tools (GitGuardian, TruffleHog) do this; single pass over buffer. |
| **S3** | **SCA / dependency audit** — parse `package.json`, `requirements.txt`, `CMakeLists.txt`, vcpkg; match against CVE DB | New: `src/security/dependency_audit.cpp`; optional JSON from NVD/OSV API | P0 | No dependency graph or CVE lookup today. |
| **S4** | **SBOM export** — Software Bill of Materials (CycloneDX/SPDX) from build manifest or digest | New: `src/security/sbom_export.cpp`; call from Build menu or CLI | P1 | Reuse `Digest-SourceManifest.ps1` output + add component list. |
| **S5** | **DAST hook** — optional integration point for external DAST (e.g. run ZAP/Burp and ingest results into Problems) | New: `src/security/dast_bridge.cpp` or settings + "Import DAST report" | P2 | UI: Import → show findings in shared Problems list. |
| **S6** | **Security dashboard** — single view: SAST + SCA + secrets counts, trend | New: `src/win32app/Win32IDE_SecurityDashboard.cpp` or tab in Audit | P1 | Aggregate from S1–S3. |
| **S7** | **Real HSM/FIPS/audit log** — replace XOR/placeholder in `hsm_integration.cpp`, `fips_compliance.cpp`, `audit_log_immutable.cpp` | `src/security/*.cpp` | P2 | Per UNFINISHED_FEATURES.md stubs. |

---

## 2. AI FRAMEWORKS / AI-NATIVE IDE

### 2.1 What Exists

| Feature | Location | Status |
| --------- | ---------- | -------- |
| Multi-backend LLM (Ollama, OpenAI, Claude, Gemini) | `Win32IDE_LLMRouter.cpp`, `Win32IDE_BackendSwitcher.cpp` | Real |
| Agent loop, tool-calling, SubAgent | `Win32IDE_AgentCommands.cpp`, `Win32IDE_SubAgent.cpp` | Real |
| Ghost Text / inline completion | `Win32IDE_GhostText.cpp` | Real |
| LSP + AI bridge | `Win32IDE_LSP_AI_Bridge.cpp` | Real |
| Deep thinking, deep research, no-refusal | `Win32IDE_AgentCommands.cpp` | Real |
| Failure detection / intelligence | `Win32IDE_FailureDetector.cpp`, `Win32IDE_FailureIntelligence.cpp` | Real |
| Autonomy (goal, memory, rate limit) | `Win32IDE_Autonomy.cpp` | Real |

### 2.2 Missing Code (AI)

| # | Missing | Expected in | Priority | Notes |
| --- | --------- | ------------- | ---------- | -------- |
| **A1** | **Agent panel host** — `initAgentPanel()` is 1-line stub; no visible chat/diff host HWND | `Win32IDE_AgentPanel.cpp` | P0 | COMPREHENSIVE_AUDIT: "Agent Diff logic exists, no host HWND". |
| **A2** | **Structured tool schema** — tools exposed as JSON schema for agent (name, params, types) | `Win32IDE_AgentCommands.cpp` or `RawrXD_ToolRegistry` | P1 | Enables "best of" agentic IDEs: tools discoverable and type-safe. |
| **A3** | **Codebase indexing for RAG** — embed repo chunks; query for "where is X" / context for prompts | New: `src/ai/codebase_rag.cpp` or extend Semantic panel | P0 | Top AI IDEs use embeddings + vector search. |
| **A4** | **Prompt templates / saved instructions** — user-defined system prompts per project or global | New: `src/win32app/Win32IDE_PromptTemplates.cpp` or Settings | P1 | Common in Cursor/Copilot. |
| **A5** | **Streaming to agent panel** — token stream to visible agent chat (not just Output tab) | `Win32IDE_AgentPanel.cpp` + streaming callback | P0 | Depends on A1. |
| **A6** | **Multi-agent orchestration UI** — assign roles, handoffs, debate mode | Extend `Win32IDE_SubAgent.cpp` or new panel | P2 | Different from current chain/swarm. |

---

## 3. IDE (Editor, Navigation, Build, UX)

### 3.1 What Exists

| Feature | Location | Status |
| --------- | ---------- | -------- |
| RichEdit editor, tabs, syntax highlight | `Win32IDE.cpp`, `Win32IDE_Tier1Cosmetics.cpp` | Real |
| File explorer, terminal, sidebar | `Win32IDE_Sidebar.cpp`, terminal in Commands | Real |
| Fuzzy command palette, file/symbol search | `Win32IDE_Tier1Cosmetics.cpp` | Real |
| LSP client, diagnostics | `Win32IDE_LSPClient.cpp` | Real |
| Git (status, commit, push, pull) | `Win32IDE_Commands.cpp` | Real |
| Session save/restore | `Win32IDE_Session.cpp` | Real |
| Marketplace (VS Code), plugins (DLL) | `Win32IDE_MarketplacePanel.cpp`, `Win32IDE_Plugins.cpp` | Real / Partial |
| Swarm build (CMake DAG) | `Win32IDE_SwarmPanel.cpp` | Real |

### 3.2 Missing Code (IDE)

| # | Missing | Expected in | Priority | Notes |
| --- | --------- | ------------- | ---------- | -------- |
| **I1** | **Go to definition / references** — LSP provides; ensure UI shows in editor (click symbol → definition) | `Win32IDE_LSPClient.cpp` + editor handler | P0 | Many IDEs rank on this. |
| **I2** | **Inline diagnostics** — squiggles and margin markers from LSP/SAST in editor | `Win32IDE_Annotations.cpp` or editor paint | P0 | displayDiagnosticsAsAnnotations exists; verify full path. |
| **I3** | **Unified Problems panel** — single list (LSP + SAST + SCA + secrets) with file/line/severity/source | New: `Win32IDE_ProblemsPanel.cpp` or extend existing Output tab | P0 | Top IDEs have one "Problems" view. |
| **I4** | **Build pipeline integration** — run configured build (e.g. CMake/Ninja) from IDE; capture output; parse errors into Problems | `Win32IDE_Commands.cpp` or new Build runner | P0 | Currently Swarm is distributed; local "Build" menu may not parse errors. |
| **I5** | **Test explorer** — discover tests (e.g. gtest, pytest); run/debug from tree | `Win32IDE_TestExplorerTree.cpp` | Partial — expand discovery and run | P1 |
| **I6** | **Refactor: rename symbol** — LSP rename; apply edits across files | LSP client + WorkspaceEdit handler | P1 | |
| **I7** | **Minimap / scrollbar code preview** | May exist in Tier1; verify and complete | P2 | |
| **I8** | **Breadcrumbs** — path in file (e.g. namespace > class > function) | `Win32IDE_Breadcrumbs.cpp` | Verify full wiring | P2 |

---

## 4. COMPILER / BUILD

### 4.1 What Exists

| Feature | Location | Status |
| --------- | ---------- | -------- |
| Phase 1 assembler, Phase 2 linker (PE) | `toolchain/from_scratch/phase1_assembler`, `phase2_linker` | Real |
| MASM64 / RE compile | `Win32IDE_ReverseEngineering.cpp` → `handleReverseEngineeringCompile()`; RawrCompiler | Real |
| Swarm (CMake DAG, distribute) | `Win32IDE_SwarmPanel.cpp`, SwarmCoordinator | Real |
| Game Engine build (Unity/Unreal) | `Win32IDE_GameEnginePanel.cpp` | Real |
| CLI compiler | `rawrxd_cli_compiler.cpp` | Real |

### 4.2 Missing Code (Compiler/Build)

| # | Missing | Expected in | Priority | Notes |
| --- | --------- | ------------- | ---------- | -------- |
| **C1** | **Single "Build" command** — configure + build (e.g. CMake + Ninja) for current folder; show output in Build tab; parse errors → Problems | New: `Win32IDE_BuildRunner.cpp` or extend Commands | P0 | Expected in top IDEs. |
| **C2** | **Build configurations** — Debug/Release, optional custom (e.g. ASan, coverage) | Settings or project config file | P1 | |
| **C3** | **Compile_commands.json** — ensure generated and consumed for LSP/clangd | LSP + Swarm/CMake path | P1 | Already referenced in Swarm. |
| **C4** | **Incremental build** — only rebuild changed; Swarm has DAG; local build should use ninja/msbuild incremental | Build runner | P2 | |

---

## 5. DEBUGGER

### 5.1 What Exists

| Feature | Location | Status |
| --------- | ---------- | -------- |
| Native debug engine (DbgEng) | `src/core/native_debugger_engine.cpp` | Real |
| Launch, attach, breakpoints, step, registers, stack, memory, disasm | `Win32IDE_NativeDebugPanel.cpp`, `Win32IDE_Debugger.cpp` | Real |
| RE integration (set binary from target, Disassemble at RIP) | `Win32IDE_ReverseEngineering.cpp`, `Win32IDE_NativeDebugPanel.cpp` | Real (recent) |

### 5.2 Missing Code (Debugger)

| # | Missing | Expected in | Priority | Notes |
| --- | --------- | ------------- | ---------- | -------- |
| **D1** | **Source-level stepping** — map RIP to source file:line; show current line in editor | `native_debugger_engine` + symbol resolution + editor highlight | P0 | enableSourceStepping in config; verify full path. |
| **D2** | **Variables view** — show locals/globals at current frame (from debug engine + symbols) | `Win32IDE_Debugger.cpp` → updateVariables() | Partial — ensure populated from engine | P0 |
| **D3** | **Watch expressions** — engine has addWatch/updateWatches; UI to add/edit/remove watches | Debugger UI (watch list control) | P1 | |
| **D4** | **Conditional breakpoints** — engine has addConditionalBreakpoint; UI to set condition | Debugger UI | P1 | |
| **D5** | **Memory breakpoints** — engine has addDataBreakpoint; UI for address + size | Debugger UI | P2 | |
| **D6** | **Call stack with source** — stack frames with file:line; click frame → open file at line | Win32IDE_Debugger stack list + editor | P1 | |
| **D7** | **Disassembly view** — dedicated pane (not only Output); sync with RIP | New pane or reuse RE disasm view | P1 | |

---

## 6. REVERSE ENGINEERING / BINARY

### 6.1 What Exists

| Feature | Location | Status |
| --------- | ---------- | -------- |
| Analyze, Disassemble, DumpBin, CFG, Functions, SSA, Recursive disasm, Type recovery, Data flow | `Win32IDE_ReverseEngineering.cpp` | Real |
| Decompiler view (D2D), Export IDA/Ghidra, Detect vulns | Same + `Win32IDE_DecompilerView.cpp` | Real |
| Set binary from debug target / build output / active doc; Disassemble at RIP | Same (recent) | Real |
| RawrCodex, RawrDumpBin, RawrCompiler, RawrReverseEngine | `src/reverse_engineering/` | Real |

### 6.2 Missing Code (RE)

| # | Missing | Expected in | Priority | Notes |
| --- | --------- | ------------- | ---------- | -------- |
| **R1** | **ELF / Mach-O** — PE-only today; add loaders for ELF/Mach-O for cross-platform RE | `src/reverse_engineering/` (e.g. RawrCodex, RawrDumpBin) | P1 | RE_ARCHITECTURE.md. |
| **R2** | **Omega suite / deobfuscator** — consolidate; MASM64 port; in-IDE "Deobfuscate" action | `src/reverse_engineering/deobfuscator/`, menu | P2 | UNFINISHED_FEATURES. |
| **R3** | **Vulnerability rules** — RE detect vulns: link to SAST-style rules (e.g. dangerous APIs, unchecked bounds) | `handleReverseEngineeringDetectVulns` + rule set | P1 | |

---

## 7. TESTING / QUALITY / DEVOPS

### 7.1 What Exists

| Feature | Location | Status |
| --------- | ---------- | -------- |
| Codebase audit (regex, memory leak hints) | `codebase_audit_system.cpp` | Real |
| Test explorer (partial) | `Win32IDE_TestExplorerTree.cpp` | Partial |
| Fuzz/coverage subsystems (CLI) | `rawrxd_subsystem_api.cpp` (GapFuzz, BBCov, CovFusion, etc.) | Real — not in IDE menu |
| Fuzz roadmap | `TIER_2_TO_TIER_3_ROADMAP.h` T3-F | Planned |

### 7.2 Missing Code (Testing)

| # | Missing | Expected in | Priority | Notes |
| --- | --------- | ------------- | ---------- | -------- |
| **T1** | **Fuzz harnesses** — libFuzzer/AFL harnesses for GGUF, agent tools, hotpatch | `test/fuzz/` per T3-F | P1 | TIER_2_TO_TIER_3_ROADMAP.h. |
| **T2** | **Coverage in IDE** — run tests with coverage; show % per file in UI or report | New: coverage runner + viewer | P1 | |
| **T3** | **Unit test runner in IDE** — run gtest/catch2 from Test Explorer; show results | `Win32IDE_TestExplorerTree.cpp` + run delegate | P1 | |
| **T4** | **CI fuzz gate** — e.g. `ci/fuzz_nightly.yml` | New or in `.github/workflows` | P2 | |

---

## 8. PRIORITY MATRIX (What to Build First)

**P0 (Must-have for "top 50" claim)**

- **Security:** S1 (SAST rules), S2 (secrets scan), S3 (SCA/dependency audit)
- **AI:** A1 (Agent panel host), A3 (codebase RAG), A5 (streaming to agent panel)
- **IDE:** I1 (Go to def/refs), I2 (inline diagnostics), I3 (Unified Problems), I4 (Build pipeline → Problems)
- **Debug:** D1 (source stepping), D2 (variables view)

**P1 (Strong differentiators)**

- S4 (SBOM), S6 (Security dashboard), A2 (tool schema), A4 (prompt templates), I5–I6, C1–C3, D3–D4, D6–D7, R1, R3, T1–T3

**P2 (Polish)**

- S5, S7, A6, I7–I8, C4, D5, R2, T4  

---

## 9. FILES TO ADD OR HEAVILY EXTEND

| Path | Purpose | Status |
| ------ | -------- | -------- |
| `scripts/rawrxd-space.sh` + `docs/MAC_LINUX_WRAPPER.md` | Mac/Linux Wine wrapper (bootable space for IDE/CLI) | **Added** |
| `src/security/secrets_scanner.cpp` | Secrets in buffers (API key, AWS, high-entropy) | **Added** — scan() + patterns + entropy; push to ProblemsAggregator from IDE/CLI |
| `src/core/problems_aggregator.hpp/.cpp` | Single list for LSP, SAST, SCA, Secrets, Build | **Added** — add/get/clear by source; thread-safe |
| `src/security/sast_rule_engine.cpp` | SAST rules (regex/source, in-house) | **Added** — reports to ProblemsAggregator |
| `src/security/dependency_audit.cpp` | SCA: manifest parsing (in-house, rawrxd_json) | **Added** — package.json, requirements.txt, CMakeLists |
| `src/security/sbom_export.cpp` | SBOM export | Not started |
| `src/security/dast_bridge.cpp` | Optional DAST import | Not started |
| `src/win32app/Win32IDE_SecurityDashboard.cpp` | Security dashboard UI | Not started |
| `src/ai/codebase_rag.cpp` | Codebase indexing + RAG | **Added** |
| `src/win32app/Win32IDE_PromptTemplates.cpp` | Saved prompts/templates | Not started |
| `src/win32app/Win32IDE_ProblemsPanel.cpp` | Unified Problems (or extend existing) | **Added** |
| `src/win32app/Win32IDE_BuildRunner.cpp` | Single Build command + error parse | **Added** |
| `test/fuzz/fuzz_gguf_parser.cpp` | Fuzz GGUF path | Not started |
| `test/fuzz/fuzz_agent_tools.cpp` | Fuzz agent tools | Not started |
| `test/fuzz/fuzz_hotpatch.cpp` | Fuzz hotpatch | Not started |

---

## 10. FROM-SCRATCH POLICY

All new security and core modules use **in-house code only** (no external SAST/SCA libs). See **docs/FROM_SCRATCH_POLICY.md**.

- **JSON:** New code uses `src/core/rawrxd_json.hpp` (minimal parse/dump) instead of adding nlohmann.

- **SCA:** `dependency_audit.cpp` parses package.json (rawrxd_json), requirements.txt, CMakeLists with STL + regex.

- **SAST:** `sast_rule_engine.cpp` runs regex/source rules; no external SAST engine.

---

## 11. SUMMARY

- **Security:** Add SAST rule engine, secrets scanner, SCA/dependency audit, SBOM, and optional Security dashboard to match top AppSec tools.  
- **AI:** Complete agent panel UI, add codebase RAG, streaming to panel, and tool schema/prompt templates.  
- **IDE:** Unify diagnostics (Problems), ensure Go to def/refs and inline squiggles, and add a single Build pipeline with error parsing.  
- **Compiler/Build:** One-click Build with configuration and error → Problems.  
- **Debugger:** Guarantee source stepping, variables, watches, conditional BP, and call stack with source.  
- **RE:** Keep current strength; add ELF/Mach-O and RE vuln rules.  
- **Testing:** Add fuzz harnesses, IDE coverage, and test runner integration.

Implementing the **P0** items above would materially close the gap to "top 50" in each category; **P1** would solidify differentiation.
