# Menu & Feature Wiring Audit — February 2026

## Executive Summary

Two separate IDE implementations exist; both have wiring gaps that prevent users from reliably accessing:
1. **File Explorer** — No View > File Browser in Ship IDE; Win32IDE Activity Bar may not surface it
2. **Chat Panel** — Ship IDE hides it by default; Win32IDE has "GH" placeholder and `routeCommand` never invoked
3. **Agent Chat / Agentic use** — No dedicated Agent Chat; same AI Chat panel used; agent output often goes to Output panel, not chat

---

## Two IDE Codebases

| IDE | Build | Entry Point | Use Case |
|-----|-------|-------------|----------|
| **RawrXD_Win32_IDE** (Ship) | `build_ship_ide_v2.bat`, `build_production.bat` | `Ship/RawrXD_Win32_IDE.cpp` | Standalone single-file IDE |
| **Win32IDE** | CMake target `RawrXD-Win32IDE` | `src/win32app/Win32IDE*.cpp` | Full IDE with Activity Bar, sidebars |

---

## RawrXD_Win32_IDE (Ship) — Findings

### 1. No View > File Browser / File Explorer Toggle
- **Status:** File tree (`g_hwndFileTree`) is always visible (layout at 0,0,200px width)
- **Issue:** There is no menu item to toggle it. View menu only has: Output Panel, Terminal, AI Chat Panel
- **Effect:** User cannot hide file explorer; if layout makes it invisible, no way to bring it back via menu

### 2. Chat Panel Hidden by Default
- **Status:** `g_bChatVisible = false` at startup
- **Wiring:** View > AI Chat Panel (Ctrl+Shift+C) toggles it — **this is wired correctly**
- **Effect:** User must discover View > AI Chat Panel; no prominent entry point

### 3. Agent Chat vs AI Chat
- **Status:** Same chat panel used for both AI chat and agentic mode
- **Flow:** Agent > Start Agentic Mode → starts HTTP server; chat sends to that server or local inference
- **Effect:** No separate "Agent Chat" for autonomous/agentic workflows; one panel serves all

### 4. No "GitHub" Placeholder in Ship IDE
- Ship IDE does not have GH button or GitHub Copilot labels; it uses "AI Chat Panel"

---

## Win32IDE (src/win32app) — Findings

### 1. "GH" Button — Placeholder Breadcrumb
- **Location:** `Win32IDE.cpp` — `IDC_BTN_GITHUB` (1022), label "GH"
- **Behavior:** Wired to `toggleSecondarySidebar()` (AI Chat panel)
- **Issue:** Label "GH" implies GitHub Copilot; MENU_FEATURE_WIRING_AUDIT.md says labels changed from "GitHub Copilot Chat" to "AI Chat", but the **title bar GH button** remains
- **Status Bar:** `Win32IDE_VSCodeUI.cpp` line 822: `"Copilot"` / `"Copilot (off)"` — still Copilot-branded

### 2. routeCommand Never Invoked
- **Root cause:** `onCommand` in `Win32IDE_Core.cpp` calls `routeCommandUnified(id)` but **never** `routeCommand(id)`
- **Effect:** Real handlers in `routeCommand` → `handleViewCommand`, `handleTier1Command`, `handleGitCommand` are unreachable for menu-driven commands
- **Affected:** View > Breadcrumbs, View > Module Browser, Git > Git Panel, and many View/UI commands

### 3. delegateToGui Infinite Re-Entry
- **Location:** `src/core/ssot_handlers.cpp` — `handleTier1BreadcrumbsToggle`, `handleViewModuleBrowser`, etc.
- **Flow:** Handler calls `delegateToGui(ctx, 12020, ...)` → `PostMessage(hwnd, WM_COMMAND, 12020, 0)` → `onCommand(12020)` → `routeCommandUnified` → same handler again → infinite loop
- **Effect:** View > Breadcrumbs, Module Browser, Git Panel can cause hangs or crashes

### 4. File Explorer Access
- **Wiring:** Activity Bar "Files" (IDC_ACTIVITY_EXPLORER) → `setSidebarView(SidebarView::Explorer)`; sidebar visible by default
- **Gaps:** Sidebar may be collapsed on startup; View > File Explorer menu item may be missing; Ctrl+B behavior may not be obvious

### 5. AI Chat / Agent Panel
- **Wiring:** Activity Bar "Chat" (IDC_ACTIVITY_CHAT), View > AI Chat (3007), Ctrl+Alt+B, GH button (1022) → `toggleSecondarySidebar()`
- **Issue:** No dedicated Agent Chat; AI Chat is generic. Agent commands (Start Agent Loop, etc.) write to Output panel, not chat
- **Placeholder responses:** "Load a model to start chatting", "Inference not wired" when backend not ready

### 6. Breadcrumbs
- **Implementation:** `Win32IDE_Breadcrumbs.cpp` — File > Class > Method path; working when `routeCommand` is reached
- **Problem:** View > Breadcrumbs goes through `delegateToGui` → infinite re-entry; `routeCommand` fallback never called

---

## Root Causes Summary

| Area | Root Cause |
|------|------------|
| **Ship IDE: File Explorer** | No View > File Browser menu item; file tree always shown, no toggle |
| **Ship IDE: Chat** | Hidden by default; View > AI Chat Panel works but not discoverable |
| **Ship IDE: Agent Chat** | Same panel as AI Chat; Agent menu starts server, chat uses it |
| **Win32IDE: GH button** | Label "GH" is GitHub placeholder; should be "AI" or "Chat" |
| **Win32IDE: Menu handlers** | `onCommand` never calls `routeCommand`; uses only `routeCommandUnified` |
| **Win32IDE: Breadcrumbs/Module/Git** | `delegateToGui` Posts same ID → infinite loop; real logic in `routeCommand` unreached |
| **Both: Agent Chat** | No dedicated Agent Chat UI; agent output in Output panel |

---

## Recommended Fixes (implemented)

### Ship IDE (RawrXD_Win32_IDE.cpp) — DONE
1. ~~Add View > File Browser~~ — Already had View > File Explorer (Ctrl+Shift+E) wired
2. Show Chat panel by default (`g_bChatVisible = true`) for discoverability
3. Add startup hint: "View > File Explorer (Ctrl+Shift+E) | View > AI Chat Panel (Ctrl+Shift+C) for agent chat"

### Win32IDE (src/win32app) — DONE
1. Rename "GH" button to "AI" — remove GitHub placeholder (Win32IDE.cpp)
2. Add `routeCommand(id)` fallback in `onCommand` before `routeCommandUnified` — fixes delegateToGui infinite loop for View/Tier1/Git commands (Win32IDE_Core.cpp)
3. Add **View > File Explorer** (IDM_VIEW_FILE_EXPLORER 2030, Ctrl+Shift+E) — shows sidebar with Explorer view (Win32IDE.cpp, Win32IDE_Commands.cpp)

---

## References

- `MENU_FEATURE_WIRING_AUDIT.md` — Prior fixes (Chat Send/Clear, Activity Bar)
- `MENU_WIRING_DIAGNOSIS.md` — delegateToGui, routeCommand analysis
- `src/win32app/Win32IDE_Commands.cpp` — routeCommand, handleViewCommand
- `src/core/ssot_handlers.cpp` — delegateToGui handlers
