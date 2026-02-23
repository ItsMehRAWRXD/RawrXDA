# RawrXD vs Top 50: Security Tools / AI Frameworks / IDEs / Compilers / Debuggers

**Goal:** Identify missing code and features so RawrXD can compete with the best in each category.

---

## 1. Security tools (Burp, Metasploit, IDA, Ghidra, Binwalk, Semgrep, Snyk)

### What RawrXD has

- **Reverse engineering:** RawrCodex (disasm, PE/ELF/Mach-O), RawrDumpBin, RawrCompiler, omega suite (50-lang, packer detection), deobfuscator, vulnerability detection in RE menu.
- **Binary analysis:** Sections, imports, exports, strings, CFG, SSA, type recovery, decompiler view (Direct2D).
- **Security surface:** `security_policy_engine`, `policy_engine`, `classified_network`, `CliSecurityIssue`/`/bugreport`, SQL injection / buffer overflow patterns in analyzers, digestion RE (stub/TODO detection).
- **No dedicated:** SAST/DAST pipeline, CVE database, dependency/SCA, secret scanning, exploit chaining, reporting (SARIF/OWASP).

### Missing code / features

| Gap | Description | Where to add |
| --- | ----------- | ------------ |
| **SAST pipeline** | Structured static analysis (AST + rules) with SARIF output; rule set (injection, XSS, hardcoded secrets, unsafe APIs). | New `src/security/sast_engine.cpp` + rule format (JSON/YAML); integrate with `code_analyzer` / `realtime_analyzer`. |
| **Dependency / SCA** | Parse `package.json`, `requirements.txt`, `Cargo.toml`, `*.csproj`; match against CVE DB (OSV, NVD) or local cache. | New `src/security/dependency_scanner.cpp`; optional `src/security/cve_cache.cpp` (offline DB). |
| **Secret scanning** | Regex + entropy for API keys, tokens, private keys in repo; block commit or report. | New `src/security/secret_scanner.cpp`; hook in CLI and IDE save/commit. |
| **Unified security report** | Single “Security” view: SAST + SCA + secrets + RE vulns in one report (SARIF/HTML). | New `src/win32app/Win32IDE_SecurityReport.cpp` or extend Audit dashboard. |
| **Exploit chaining (RE)** | Scriptable workflow: detect vuln → suggest payload → (optional) run in sandbox. | Extend `RawrReverseEngine` / RE menu with workflow DSL or simple steps. |
| **Network / fuzz** | No built-in HTTP proxy, replay, or fuzzer. | Optional: minimal proxy in `src/security/` or document use of external tools. |

---

## 2. AI frameworks (LangChain, LlamaIndex, Semantic Kernel, Cursor, Continue, Aider)

### What RawrXD has

- **Local inference:** GGUF loader, AVX512 CPU inference, RawrBlob, multi-engine registry, streaming.
- **Agentic core:** Deep thinking (CoT), deep research (FileOps), subagents, swarm, tool dispatch, self-correction/hotpatch.
- **IDE integration:** Win32 IDE, chat panel, agent commands, MCP hooks, backend switcher (Ollama/OpenAI/Claude/Gemini).
- **No dedicated:** RAG with vector DB, persistent chat history per project, codebase indexing (embeddings), multi-doc context window, “@codebase” style references.

### Missing code / features

| Gap | Description | Where to add |
| --- | ----------- | ------------ |
| **RAG / embeddings** | Embedding model + vector store (in-memory or persisted); “answer from docs/codebase.” | New `src/ai/embedding_provider.cpp`, `src/ai/vector_store.cpp`; optional sqlite/FAISS. |
| **Codebase index** | Index symbols, files, snippets; semantic search for “where is X used.” | Extend LSP index or new `src/core/codebase_index.cpp`; feed into prompt builder. |
| **Project-scoped chat** | Chat history and context tied to workspace/project, not global. | `agent_history` / chat panel: key by workspace path; persist under `%APPDATA%\RawrXD\workspaces\<hash>\`. |
| **Structured tool schema** | Publish tool list with JSON schema (name, args, description) for agentic UI and MCP. | `ToolRegistry` / `auto_feature_registry`: emit OpenAPI-like schema; MCP already has tools. |
| **Guardrails / safety** | PII redaction, prompt injection detection, output filters. | New `src/core/agent_guardrails.cpp`; plug before/after inference and tool output. |

---

## 3. IDEs (VS Code, Visual Studio, JetBrains, Cursor)

### What RawrXD has

- **Editor:** Win32 text, syntax highlight, Direct2D, tabs, search.
- **Build/run/debug:** Compile (MASM/NASM/g++), Phase 2 linker integration, Run Binary, Debug with DbgEng (breakpoints, watch, stack, symbols).
- **RE/Codex:** Full RE menu, decompiler view, set binary from debug/build, `/api/re/set-binary`.
- **LSP:** LSP client, server, AI bridge; diagnostics, symbols.
- **No dedicated:** Multi-root workspaces, extensions marketplace (install from VSIX), terminal profiles, tasks.json/launch.json, Git UX (diff, blame, history), refactor menu (rename symbol, extract function).

### Missing code / features

| Gap | Description | Where to add |
| --- | ----------- | ------------ |
| **Workspace/project model** | Explicit workspace = folder(s); load/save “project” (open files, layout). | `project_context` / Win32IDE: workspace root + optional `.rawrxd/workspace.json`. |
| **Tasks & launch** | Parse or generate `tasks.json` / `launch.json`; “Run task,” “Debug config.” | New `src/win32app/Win32IDE_Tasks.cpp` or extend launch config; read JSON, drive compiler/debugger. |
| **Git integration** | Diff view, blame, stage/commit from IDE, branch indicator. | New `src/win32app/Win32IDE_Git.cpp`; shell to `git` or libgit2. |
| **Refactor menu** | Rename symbol (LSP rename), extract function/method, inline. | Wire LSP `textDocument/rename` and code actions; add Edit → Refactor. |
| **Extension marketplace** | List, install, update extensions (VSIX); enable/disable. | Extend `Win32IDE_Plugins` / marketplace; install to `%APPDATA%\RawrXD\extensions`. |
| **Terminal profiles** | Multiple terminals (PowerShell, CMD, Git Bash, WSL) with profiles. | `Win32TerminalManager`: profile list + “New terminal with profile.” || **Mac/Linux wrapper** | Wine-based wrapper script for running RawrXD on Mac/Linux. | `scripts/rawrxd-space.sh` + `docs/MAC_LINUX_WRAPPER.md` (Wine bootable space). |
---

## 4. Compilers / toolchains (GCC, Clang, MSVC, Zig, TinyCC)

### What RawrXD has

- **Phase 1 (assembler):** Produces COFF .obj (from_scratch).
- **Phase 2 (linker):** COFF → PE32+ (rawrxd_link), single/multi .obj, IAT, stub, relocs.
- **No C/C++ front-end:** Compilation of `.c`/`.cpp` is via external `g++`/`cl` or RawrCompiler (wrapper).
- **CLI compiler:** `rawrxd_cli_compiler` with options (optimize, target, etc.).

### Missing code / features

| Gap | Description | Where to add |
| --- | ----------- | ------------ |
| **C/C++ front-end** | Parse C/C++ → AST → IR → codegen (or emit LLVM IR and call LLVM). | Large: new `src/compiler/` (lexer, parser, AST, type checker, codegen) or wrap libclang/LLVM. |
| **Phase 3 (CRT/libc)** | Minimal C runtime: startup, atexit, stdio stubs, one-shot malloc. | `toolchain/from_scratch/phase3_crt`; link with Phase 2. |
| **.data / .rdata** | Merge and relocate .data/.rdata; globals and string literals. | Phase 2 hardening: `section_merge` + `pe_writer` (already spec’d in PHASE2_HARDENING.md). |
| **Cross-compile** | Target Linux/macOS from Windows (e.g. emit ELF/Mach-O). | Phase 2: optional ELF/Mach-O writer; or invoke external linker. |
| **Incremental / daemon** | Daemon that keeps state for incremental builds. | Optional: `rawrxd_build_daemon` that caches object graph and only recompiles changed units. |

---

## 5. Debuggers (VS, WinDbg, GDB, LLDB, x64dbg)

### What RawrXD has

- **Native debugger (DbgEng):** Launch/attach, breakpoints (software + hardware DR0–DR3), watch, stack walk, symbol resolution, disasm at address, memory read, events (breakpoint, exception, module load).
- **IDE UI:** Debug toolbar (Continue, Step Over/Into/Out, Stop), breakpoint list, watch list, variables, stack (via native_debugger_engine).
- **RE + debug:** Current binary set from launch; Disassemble/DumpBin/CFG on debugged binary.

### Missing code / features

| Gap | Description | Where to add |
| --- | ----------- | ------------ |
| **Source-level stepping** | Map instruction → source line; “step” in source terms; show current line in editor. | PDB/line info in `native_debugger_engine`; editor highlight by line from debugger; sync on step/break. |
| **Conditional / logpoint** | Break when expression true; logpoint (evaluate and log, don’t stop). | `NativeDebuggerEngine`: add condition expression to breakpoint; evaluate in debug loop; logpoint = condition + “log only.” |
| **Edit & Continue** | Apply code changes to running process (patch code section, update PDB). | Hard: DbgEng or manual VirtualProtect + write; optional “hot replace” for same-function edits. |
| **Memory/hex view** | Dedicated memory view (address → hex dump, follow pointer). | New panel `Win32IDE_MemoryView.cpp`; read memory via debug engine at current process. |
| **Reverse debugging** | Record trace and step backward. | Optional: record execution trace (e.g. via DbgEng or instrumentation), replay for “reverse step.” |
| **Core dump / post-mortem** | Open dump file, inspect crash state. | `NativeDebuggerEngine`: add `openDump(path)`; use DbgEng dump APIs. |

---

## 6. Cross-cutting (needed for “best of” in any category)

| Gap | Description | Where to add |
| --- | ----------- | ------------ |
| **Telemetry-free by default** | No phone-home; optional anonymous usage stats with explicit opt-in. | Audit all network calls; document “zero telemetry” in README; optional `usage_stats_opt_in` in settings. |
| **Offline-first** | Full value without internet: local models, local RE, local security scan. | Already strong; add offline CVE cache and “no network” mode for security scan. |
| **Performance** | Startup time, memory, inference latency. | Profile IDE startup; lazy-load heavy modules (RE, LSP); document “recommended hardware.” |
| **Accessibility** | Keyboard navigation, screen reader, high contrast. | Win32: focus order, ARIA where applicable (e.g. HTML panels); document a11y in USER_GUIDE. |
| **Docs & onboarding** | Single “Getting started” and “Feature map” so users reach security/RE/AI quickly. | `docs/QUICK_START_TOP_50.md`: one page per category (Security, AI, IDE, Compiler, Debugger) with links to features and first steps. |

---

## 7. Suggested implementation order

1. **Security:** SAST pipeline (rules + SARIF) and dependency/SCA (manifest parse + CVE lookup) — highest impact for “security tool” positioning.
2. **IDE:** Tasks/launch (tasks.json/launch.json) and refactor (LSP rename + code actions) — improves daily use.
3. **Compiler:** Phase 2 hardening (.data/.rdata) and Phase 3 CRT — enables real C programs with the fortress toolchain.
4. **Debugger:** Source-level stepping and current-line highlight — closes the loop with “best debuggers.”
5. **AI:** RAG/embeddings and project-scoped chat — differentiates from “just another chat IDE.”

Use this as a roadmap; each row in the tables above is a concrete “what code is missing” and “where to add it.”
---

## FILES TO ADD

| File | Purpose | Status |
|------|---------|--------|
| scripts/rawrxd-space.sh | Wine wrapper for Mac/Linux (sets WINEPREFIX, launches IDE/CLI) | ✅ Added |
| docs/MAC_LINUX_WRAPPER.md | Documentation for Mac/Linux Wine bootable space | ✅ Added |