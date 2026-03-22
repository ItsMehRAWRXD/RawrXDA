# RawrXD vs top 17 IDEs — capability audit (non-AI, OS-agnostic lens)

**Purpose:** Compare **RawrXD (Win32 native focus)** to **seventeen widely used editors/IDEs** on **core IDE product dimensions**, ignoring autonomous agents, billing models, and host OS (Xcode/macOS-only features still count as “what a top IDE often ships”).

**Sources:** **`docs/WIN32_IDE_FEATURES_AUDIT.md`**, **`docs/IDE_CODEBASE_INVENTORY.md`**, **`docs/UNIVERSAL_PLATFORM_GAP_MATRIX.md`**, **`docs/AUDIT_100_PERCENT_RAW RXD.md`** / **`docs/AUDIT_100_PERCENT_WORKING_ONLY.md`**, plus public product positioning for reference IDEs.

**Legend (RawrXD vs tier-1 on each row):** **●** on par or **unique strength** · **◐** partial / uneven · **○** clearly behind · **—** N/A (e.g. Apple-only stack). *“Gap”* is qualitative remediation cost, not the symbol.

---

## 1. Reference set — “top 17” IDEs (2025–2026 mindshare)

| # | Product | Vendor / lineage | Typical primary users |
|---|---------|------------------|------------------------|
| 1 | **Visual Studio** | Microsoft | .NET, C++, game (with workloads) |
| 2 | **VS Code** | Microsoft | Polyglot, web, extensions-first |
| 3 | **IntelliJ IDEA** | JetBrains | JVM, enterprise, polyglot |
| 4 | **PyCharm** | JetBrains | Python |
| 5 | **WebStorm** | JetBrains | JS/TS web |
| 6 | **Android Studio** | Google / JetBrains | Android |
| 7 | **Xcode** | Apple | Apple platforms |
| 8 | **Eclipse** | Eclipse Foundation | Java, embedded, multi-language |
| 9 | **CLion** | JetBrains | C/C++ (CMake, etc.) |
| 10 | **PhpStorm** | JetBrains | PHP |
| 11 | **GoLand** | JetBrains | Go |
| 12 | **Sublime Text** | Sublime HQ | Fast editor, multi-language |
| 13 | **Zed** | Zed Industries | Collaborative, Rust-native UI |
| 14 | **Fleet** | JetBrains | Lightweight multi-language |
| 15 | **Cursor** | Anysphere | VS Code fork + AI (feature set ≈ Code + extras) |
| 16 | **Neovim** | Community | Modal editing, LSP via ecosystem |
| 17 | **GNU Emacs** | FSF / distros | Extensible environment |

*Note: “Top” here means **reach + expectation of depth** in at least one domain, not a quality ranking.*

---

## 2. Comparison dimensions (what buyers expect from an “IDE”)

| Dimension | Tier-1 expectation | RawrXD (2026 snapshot) | Vs tier-1 | Gap severity |
|-----------|-------------------|-------------------------|-----------|--------------|
| **Text editing core** | Stable, fast, large files, multi-cursor, rich keymap | RichEdit/Win32 paths, find/replace, undo model improved per changelog | ◐ | Medium |
| **Syntax / semantic coloring** | Tree-sitter or equivalent, scopes | Present in Win32 stack; depth varies by language | ◐ | Medium |
| **LSP** | First-class for many languages | `Win32IDE_LSPClient`, bridges; not equal to JetBrains indices | ○ | High |
| **Debugger** | Breakpoints, threads, memory, conditional bp | Win32 debugger panel / native pipeline; not VS-level | ○ | High |
| **Build / tasks** | Tasks, problem matchers, integrated output | PowerShell, build scripts, output panel — uneven VS Code task parity | ◐ | Medium–high |
| **Git / VCS** | Diff, blame, merge, PR flows | Git panel/commands; parity with GitLens/Git UI incomplete | ○ | High |
| **Terminal** | Multiplex, shells, env | PowerShell/CMD, split — working core per audit | ◐ | Low–medium |
| **Search** | Workspace symbol, refs, text, regex | Multi-file search, semantic index shims — behind IDEA/VS Code | ○ | High |
| **Refactor** | Rename across project, extract method | Limited vs CLion/IDEA/Rust Analyzer stack | ○ | High |
| **Extensions / plugins** | Marketplace, versioning, isolation | Extension/manifest paths; not npm/VSIX ecosystem parity | ○ | High |
| **Settings sync / profiles** | Cloud or file-based | Local `settings.json` patterns — weaker than Code/JetBrains | ◐ | Medium |
| **Theming / accessibility** | Themes, high contrast, screen readers | Rose-pine style theming; a11y not tier-1 | ◐ | Medium–high |
| **Remote / containers / SSH** | Dev Containers, Gateway, etc. | Not a primary strength | ○ | High |
| **Collaboration** | Live share, CRDT (some products) | Collab modules in tree; not Google-Docs-level default | ○ | High |
| **Performance (cold start, huge repos)** | Optimized IO, incremental | Native Win32 can be lean; monorepo + GGUF paths are heavy | ◐ | Medium |
| **Installer / updates** | Signed, delta, channels | Auto-update concepts in repo; enterprise license paths | ◐ | Medium |
| **Documentation & onboarding** | First-run, docs portal | Many internal docs; fragmented for new contributors | ◐ | Medium |
| **Local ML / GGUF inference** | Usually external (CLI, Ollama, plugins) | In-process `GGUFRunner`, runtime gate, TpsSmoke, optional unified memory | **●** | *Differentiator* (not a “gap”) |

---

## 3. RawrXD positioning vs the 17 (summary)

- **Beats or matches** typical editors on: **native Win32 integration**, **custom GGUF / inference / runtime gate** (none of the 17 ship this as core), **deep menu/surface area** (command ID breadth), **MASM/RE-adjacent** tooling ambition.
- **Roughly in the band of “capable editor + panels”** with **Sublime / Zed-class** on *some* axes but **not** on marketplace, remote dev, or multi-language semantic depth.
- **Below** **VS / IDEA / Android Studio / Xcode** on **debugger + project model + refactor + VCS depth**.
- **Below** **VS Code / Cursor / Fleet** on **extension economy and LSP ubiquity** (even if RawrXD implements LSP, ecosystem mass matters).
- **Neovim / Emacs** win on **scriptability and community packages**; RawrXD wins on **single shipped Win32 binary + integrated GPU paths** *if* those are your goals.

---

## 4. Prioritized gap list (if RawrXD wants “IDE table stakes”)

1. **LSP depth + reliability** across C++, Python, TS without one-off shims.  
2. **Debugger UX parity** (call stack, watches, eval) for native code.  
3. **Git**: blame, interactive staging, merge conflict UI comparable to Code/JetBrains.  
4. **Workspace search & references** backed by a real index (not regex-only).  
5. **Extension story**: install/update/sandbox model (even a subset of VSIX).  
6. **Remote / WSL2 / SSH** optional lane — huge for “general IDE” perception.  
7. **Accessibility** (Narrator, keyboard-only parity).  
8. **First-run wizard** + link to **one** canonical “user manual” path.

---

## 5. Explicit non-goals (per your request)

- **Not scored:** autonomous agents, Copilot-like models, “AI-first” positioning.  
- **Not scored:** payment method (card vs cash) or licensing business model.  
- **OS:** Reference IDEs include macOS-only (Xcode); comparison is **feature expectation**, not “can RawrXD run on iOS”.

---

## 6. See also

| Doc | Use when |
|-----|----------|
| **`docs/UNIVERSAL_PLATFORM_GAP_MATRIX.md`** | OS / production contract vs “universal binary” fantasies |
| **`docs/WIN32_IDE_FEATURES_AUDIT.md`** | What is actually wired in Win32 GUI |
| **`docs/IDE_CODEBASE_INVENTORY.md`** | Repo size / where code lives |
| **`Electron` shell** | `bigdaddyg-ide/` + **`docs/AUTONOMOUS_AGENT_ELECTRON.md`** — orthogonal to this native comparison |

---

**Last updated:** 2026-03-13 — static audit; re-run when `WIN32_IDE_FEATURES_AUDIT.md` or LSP/debug milestones ship.
