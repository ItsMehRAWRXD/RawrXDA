# RawrXD: Everything as an IDE

RawrXD is **one IDE**. Every build target and script exists to run, drive, or generate that IDE. This doc is the single place to see how to get “the IDE” in each form.

**Competitive bar:** The IDE targets **Cursor and VS Code + GitHub Copilot** — see **docs/COMPETITIVE_STANDARD_CURSOR_VSCODE_COPILOT.md**. No basic tier.

---

## What the IDE is

- **Four main panes:** File Explorer (left), Terminal & Debug (bottom), Editor (center), AI Chat (right). Everything else is a pop up (command palette, settings, Git, hotpatch, etc.). See **ARCHITECTURE.md §6.5** and **Win32IDE_LayoutCanon.h**.
- **Agentic core:** Inference (GGUF), agent loop, hotpatching, LSP, reverse engineering, swarm — all part of the same IDE product.
- **One product, multiple builds:** Full Win32 app, minimal monolithic EXE, or engine + API for web/CLI.

---

## How to get the IDE

### 1. Full Win32 IDE (recommended)

Full native IDE: four panes, 170+ commands, Direct2D, hotpatch UI, agent panel, terminal.

```powershell
cd D:\rawrxd
cmake -B build -S . -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target RawrXD-Win32IDE
# Run:
.\build\bin\RawrXD-Win32IDE.exe
```

Output: `build\bin\RawrXD-Win32IDE.exe` (or `build\Release\RawrXD-Win32IDE.exe` depending on generator).

### 2. Monolithic IDE (single EXE, no CRT)

Minimal IDE: WinMain → Beacon → Inference → Agent → UIMainLoop. One EXE, MASM64 only.

```powershell
cd D:\rawrxd
.\scripts\build_monolithic.ps1
# Run:
.\build\monolithic\RawrXD.exe
```

Requires: ml64/link (VS or Developer Command Prompt). See **docs/MONOLITHIC_MASM64_BUILD.md**.

### 3. Engine + web / CLI (API-driven IDE UIs)

Run the agentic engine and API; use any client (browser, CLI, standalone bridge) as the “IDE” front-end.

```powershell
cmake --build build --config Release --target RawrEngine
.\build\bin\RawrEngine.exe
# REST on port 8080; open gui/ide_chatbot.html or use standalone_interface.html
```

- **Standalone Web Bridge (Qt-free):** Serves HTML UIs and talks to GGUF/inference over TCP. See **docs/RAWRXD_IDE_DOCUMENTARY_OVERVIEW.md** §4.
- **rawrxd-monaco-gen:** Generates Vite/Monaco/Tailwind React IDEs; those *are* IDE front-ends driven by the same backend.

---

## Summary

| You want… | Build / run |
|-----------|-------------|
| Full IDE (four panes, all features) | `RawrXD-Win32IDE` → `build\bin\RawrXD-Win32IDE.exe` |
| Smallest IDE (one EXE) | `.\scripts\build_monolithic.ps1` → `build\monolithic\RawrXD.exe` |
| IDE in browser / API | `RawrEngine` + `gui/ide_chatbot.html` or standalone bridge |
| Generate IDE UIs | `rawrxd-monaco-gen` |

All of the above are **the same IDE** in different forms. Continue in the direction of having everything as an IDE: every new feature and build should reinforce this single product.
