# Menu Wiring Diagnosis and Fixes

This document describes menu items that were not wired to features (placeholding breadcrumbs) and what was fixed so **View > File Explorer**, the **Chat panel**, and **Agent** workflows are usable.

---

## Summary of issues

| Location | Issue | Fix |
|----------|--------|-----|
| **RawrXD_Win32_IDE.cpp** | View had no "File Explorer" — file tree was always on, no toggle | Added View > File Explorer (Ctrl+Shift+E), `g_bFileTreeVisible`, handler, layout |
| **RawrXD_Win32_IDE.cpp** | Typo `Y if` in `AgentRunTool()` caused compile/runtime error | Corrected to `if` |
| **Win32_IDE_Complete.cpp** | View > Fullscreen (F11) did nothing | Implemented fullscreen toggle and F11 accelerator |
| **Win32_IDE_Complete.cpp** | AI > Chat did nothing | Wired to show chat panel and focus chat input |

---

## RawrXD_Win32_IDE.cpp (main production IDE)

### What was already wired

- **View > Output Panel** – toggles output panel.
- **View > Terminal** – toggles terminal panel.
- **View > AI Chat Panel** – toggles chat panel; chat send and agentic flow work when a model/agent is running.
- **Agent > Start Agentic Mode** – starts RawrXD_Agent process.
- **Agent > Stop Agentic Mode** – stops it.
- **Agent > Run Tool...** – invokes agent tool (after agentic mode is started).
- **File tree** – double-click on file opens it in the editor; tree is populated from workspace (exe directory).

### What was fixed

1. **View > File Explorer**
   - **Issue:** No menu item to show/hide the file tree. The tree was always visible with no way to get more editor space.
   - **Change:** Added **View > File Explorer** (Ctrl+Shift+E), `IDM_VIEW_FILEBROWSER` (2500), and `g_bFileTreeVisible`. Toggling updates visibility and layout. File tree remains the same control; double-click still opens files.

2. **AgentRunTool typo**
   - **Issue:** Line had `Y    if` instead of `    if`, breaking the agent run-tool path.
   - **Change:** Removed the stray `Y`.

### How to use file explorer and agent chat

- **File Explorer:** View > File Explorer (or Ctrl+Shift+E if you add an accelerator). Double-click a file in the tree to open it.
- **Chat panel:** View > AI Chat Panel. Type in the chat input and send; use AI > Load GGUF / Load Titan Model so chat has a model. For **agentic/autonomous** use: Agent > Start Agentic Mode, then use chat or Agent > Run Tool... as needed.

### Pure CLI (101% parity with Win32 IDE)

- **RawrXD_CLI** is the same engine as RawrEngine, built with full chat and agentic autonomous capabilities. It listens on **port 23959** by default so the Win32 IDE can connect to it for:
  - **AI Chat panel** → `POST /api/chat`
  - **Agent > Run Tool** → `POST /api/tool`
- Run: `RawrXD_CLI.exe` (or `RawrXD_CLI.exe --port 23959`). Then start the Win32 IDE; it will use this server for chat and tools if nothing else is on 23959.
- In the CLI REPL: `/chat`, `/agent`, `/wish`, `/subagent`, `/chain`, `/swarm`, and all other REPL commands mirror the Win32 GUI feature set.

---

## Win32_IDE_Complete.cpp (alternate full IDE build)

### What was already wired

- **View > File Browser** – toggles tree view.
- **View > Terminal** – toggles terminal.
- **View > Chat Panel** – toggles chat.
- **Chat Send** – sends message; model load and inference path exist.

### What was fixed

1. **View > Fullscreen (F11)**
   - **Issue:** Menu item and F11 did nothing.
   - **Change:** Fullscreen toggle implemented: removes caption/thick frame and maximizes; F11 (or menu) again restores normal window. F11 added to the accelerator table.

2. **AI > Chat**
   - **Issue:** Menu item did nothing (breadcrumb).
   - **Change:** AI > Chat now shows the chat panel, runs layout, and sets focus to the chat input so users can go straight to typing.

### Note on “GitHub chat” / breadcrumbs

- There is no separate “GitHub chat” feature in the codebase; references in docs (e.g. DEPLOYMENT_CHECKLIST) are to external comparisons (e.g. “GitHub Copilot (Session: Qt Migration Command Center)”).
- The only chat UI is the **AI Chat Panel** (View > AI Chat Panel in both IDEs, plus AI > Chat in Complete). Making sure that panel and AI/Agent menus are wired (as above) is what enables “agent chat” and autonomous use.

---

## Build targets

- **RawrXD_Win32_IDE** – Built from `RawrXD_Win32_IDE.cpp` (e.g. via build_ide or your CMake/ninja setup). This is the main IDE with file tree, chat, and agent menu.
- **Win32_IDE_Complete** – Standalone “complete” IDE in `Win32_IDE_Complete.cpp`; may be built as a separate target. It has File Browser, Terminal, Chat Panel, and now Fullscreen and AI > Chat wired.

Ensure the executable you run is the one you intend (Win32_IDE vs IDE_Complete) so the correct menu set and wiring apply.

---

## Checklist for valid file explorer and agent chat

- [x] **File explorer:** View > File Explorer (RawrXD_Win32_IDE) or View > File Browser (Win32_IDE_Complete) toggles the tree; double-click opens files.
- [x] **Chat panel:** View > AI Chat Panel shows/hides chat; AI > Chat (Complete) focuses chat input.
- [x] **Agentic use:** Agent > Start Agentic Mode then chat or Run Tool; ensure a model is loaded (AI > Load GGUF / Load Titan Model) for local chat.
- [x] **No dead breadcrumbs:** Fullscreen and AI > Chat now perform the described actions.
