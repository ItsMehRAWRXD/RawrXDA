# Win32 GUI IDE — Features Audit (What’s Fully Working)

**Purpose:** Identify which features in the Win32 GUI IDE are **fully working** end-to-end vs partially wired or non-functional.  
**Sources:** Code inspection, `reports/win32_ide_wiring_manifest.md`, `docs/WIN32_IDE_FULL_DIR_AUDIT.md`, `docs/WIN32IDE_CPP_AUDIT.md`.  
**Date:** 2026-02-12.

---

## 1. Executive Summary

| Status | Count | Meaning |
|--------|--------|--------|
| **Fully working** | Core file/edit/terminal/agent paths, many menus and panels | Wired, implemented, and functional |
| **Partially working** | Some View items, some panels | UI exists but handler uses wrong ID or path is incomplete |
| **Not working / missing handler** | View (2020–2027), Git (3020–3024), some Tools/Modules, many controls | Menu or control exists; no or wrong handler |
| **Build / optional** | Some panels (Network, CrashReporter, ColorPicker, etc.) | May fail to compile or depend on missing members |

**Wiring snapshot (from `win32_ide_wiring_audit.py`):** ~181 connected, ~70 missing handler, ~305 “missing source” (handler without menu/control; many are range-based or control notifications).

---

## 2. Fully Working Features

### 2.1 File menu (2001–2005, 1030, etc.)

| Feature | ID / range | Handler | Notes |
|---------|------------|---------|--------|
| New | IDM_FILE_NEW 2001 | handleFileCommand → newFile() | ✅ |
| Open | IDM_FILE_OPEN 2002 | openFile() | ✅ |
| Save | IDM_FILE_SAVE 2003 | saveFile() | ✅ |
| Save As | IDM_FILE_SAVEAS 2004 | saveFileAs() | ✅ |
| Exit | IDM_FILE_EXIT 2005 | PostQuitMessage | ✅ |
| Load Model (GGUF) | IDM_FILE_LOAD_MODEL 1030 | openModel() | ✅ |
| Recent files | IDM_FILE_RECENT_BASE + index | openRecentFile(index) | ✅ |

File ops (openFileDialog, saveAll, closeFile, promptSaveChanges) are implemented in `Win32IDE_FileOps.cpp`. File menu is fully working.

### 2.2 Edit menu (2007–2019 range)

| Feature | Status | Notes |
|---------|--------|--------|
| Undo, Redo, Cut, Copy, Paste | ✅ | handleEditCommand; RichEdit messages |
| Find, Replace | ✅ | IDM_EDIT_FIND 2016, IDM_EDIT_REPLACE 2017 |
| Select All | ✅ | Via IDM_EDIT_SELECT_ALL (handler exists; menu may use same or different ID in 2000–3000 range) |

Edit commands in the 2000–3000 range are routed to handleEditCommand; core editing is working. Some entries (Find Next/Prev, Snippet, Copy Format, Paste Plain, Clipboard History) appear in the “missing handler” list — menu present, no dedicated case in the audited path.

### 2.3 Terminal menu (4001–4009 range)

| Feature | Status | Notes |
|---------|--------|--------|
| Start PowerShell | 4001 | startPowerShell() | ✅ |
| Start CMD | 4002 | startCommandPrompt() | ✅ |
| Stop Terminal | 4003 | stopTerminal() | ✅ |
| Clear All | 3006 | handleTerminalCommand | ✅ |
| Split H/V | 4007, 4008 | ✅ |

Terminal commands in 4000–4100 go to handleTerminalCommand; implementation in Win32IDE.cpp and Win32IDE_PowerShell.cpp. Terminal is fully working for main actions.

### 2.4 Agent menu (4100–4400 range)

| Feature | Status | Notes |
|---------|--------|--------|
| Start Loop, Stop, Execute Cmd, Configure Model, View Tools/Status | ✅ | handleAgentCommand |
| Bounded Loop, AI modes (Deep Think, etc.), Context 4K–1M | ✅ | Wired in Commands + AgentCommands |
| Hotpatch (Memory/Byte/Server/Proxy) | ✅ | IDM_HOTPATCH_* in manifest as connected |

Agent and hotpatch commands are routed and implemented (Win32IDE_AgentCommands.cpp, Win32IDE_ReverseEngineering.cpp, hotpatch panel).

### 2.5 Tools menu (9800–9840, 10200–10206, etc.)

| Feature | Status | Notes |
|---------|--------|--------|
| Voice Automation (Toggle, Stop, Next/Prev voice, Rate) | ✅ | IDM_VOICE_AUTO_*; 10200–10300 → Win32IDE_HandleVoiceAutomationCommand |
| Backups (Create, Restore, Auto, List, Prune) | ✅ | IDM_QW_BACKUP_* |
| Alert & Monitoring | ✅ | IDM_QW_ALERT_* |
| Shortcut Editor, SLO Dashboard | ✅ | IDM_QW_SHORTCUT_EDITOR, IDM_QW_SLO_DASHBOARD |

### 2.6 Help menu (4001–4004)

| Feature | Status | Notes |
|---------|--------|--------|
| About, Command Ref, PSDocs, Search | ✅ | handleHelpCommand (4000–4100 routed to Help; IDs 4001–4004 in manifest as connected) |

### 2.7 Audit menu (9500–9506)

| Feature | Status | Notes |
|---------|--------|--------|
| Show Dashboard, Run Full, Detect Stubs, Check Menus, Run Tests, Export Report, Quick Stats | ✅ | handleAuditCommand |

### 2.8 Voice Chat (9700–9706)

| Feature | Status | Notes |
|---------|--------|--------|
| Toggle Panel, Record, PTT, Speak, Join Room, Show Devices, Metrics | ✅ | handleVoiceChatCommand |

### 2.9 UI and layout

| Component | Status | Notes |
|-----------|--------|--------|
| Menu bar | ✅ | createMenuBar; m_hMenu created; SetMenu |
| Toolbar / title bar | ✅ | createToolbar, layoutTitleBar |
| Activity bar + sidebar | ✅ | createActivityBar, createPrimarySidebar; setSidebarView |
| Tab bar (editor tabs) | ✅ | createTabBar; switch/close tabs |
| Breadcrumb bar | ✅ | createBreadcrumbBar; updateBreadcrumbs/OnCursorMove; View → Breadcrumbs toggle |
| Line numbers | ✅ | createLineNumberGutter; updateLineNumbers |
| Editor (RichEdit) | ✅ | createEditor; theme, syntax, ghost text |
| Status bar | ✅ | createStatusBar; SB_SETTEXT from many paths |
| Output tabs | ✅ | createOutputTabs; appendToOutput, severity filter |
| PowerShell panel | ✅ | createPowerShellPanel |
| File explorer (sidebar tree) | ✅ | createExplorerView; m_hwndExplorerTree; refresh, New File/Folder, Delete/Rename (context menu wired) |
| Secondary sidebar (Copilot/AI) | ✅ | Chat input/output, send/clear; model chat |

Core layout and primary panels are created and used; file explorer context menu (including Delete/Rename) is wired.

---

## 3. Partially Working or Wrong ID Routing

### 3.1 View menu (IDM_VIEW_* = 2020–2027)

| Menu item | ID | Problem | Handler expects |
|-----------|-----|--------|------------------|
| Minimap | IDM_VIEW_MINIMAP 2020 | Routed to handleEditCommand (2000–3000); no case 2020 | handleViewCommand uses 3001–3008 |
| Output Tabs | 2021 | Same | 3002 = “Toggle Output Panel” |
| Module Browser | 2022 | Same | 3005 |
| Theme Editor | 2023 | Same | 3004 |
| Floating Panel | 2024 | Same | 3003 |
| Output Panel | 2025 | Same | 3002 |
| Use Streaming Loader | 2026 | Same | — |
| Use Vulkan Renderer | 2027 | Same | — |

**Conclusion:** View menu uses 2020–2027 but routeCommand sends 3000–4000 to handleViewCommand and 2000–3000 to handleEditCommand. So 2020–2027 go to Edit and are not handled. **View toggles (Minimap, Output, Floating, Theme, Module Browser, etc.) are not working** until either (a) View menu uses 3001–3008 (and any extra IDs in 3000–4000) or (b) handleViewCommand (or a dedicated branch) handles 2020–2027.

### 3.2 Git menu (IDM_GIT_* = 3020–3024)

| Menu item | ID | Problem | Handler expects |
|-----------|-----|--------|------------------|
| Status, Commit, Push, Pull, Panel | 3020–3024 | Routed to handleViewCommand (3000–4000); no case 3020–3024 | handleGitCommand uses 8001–8005 |

**Conclusion:** Git menu uses 3020–3024 but handleGitCommand is only called for 8000–9000 and uses 8001–8005. So **Git menu does nothing**. Fix: either change Git menu to use 8001–8005 or add 3020–3024 to the routing and handleGitCommand.

---

## 4. Not Working / Missing Handler (from wiring manifest)

- **Edit:** Find Next (2018), Find Prev (2019), Snippet (2012), Copy Format (2013), Paste Plain (2014), Clipboard History (2015) — menu present, no handler in scanned path.
- **View:** All IDM_VIEW_* 2020–2027 (see above).
- **Tools:** Profile Start/Stop/Results (3010–3012), Analyze Script (3013).
- **Modules:** Refresh (3050), Import (3051), Export (3052).
- **Git:** All (3020–3024) — wrong range (see above).
- **Controls with no WM_COMMAND handler:** Many panel controls (e.g. IDC_PANEL_BTN_*, IDC_DEBUGGER_*, IDC_COPILOT_*, IDC_PS_*) are created but may be handled by parent or subclass; “missing handler” here means no explicit case in the audited command path. Some may still work via window proc or other dispatch.

---

## 5. Panels / Optional Modules (build and runtime)

| Panel / module | Build | Runtime | Notes |
|----------------|--------|--------|--------|
| Core (File, Edit, View, Terminal, Agent, Tools, Help, Audit, Voice) | ✅ | ✅ for correct IDs | View and Git broken by ID mismatch |
| Sidebar (Explorer, Search, SCM, Debug, Extensions, Recovery) | ✅ | ✅ | Explorer tree + context menu working |
| Backend switcher, LLM router | ✅ | ✅ | Backend local/Ollama/OpenAI/Claude; API key optional for local |
| Cursor parity (8 modules) | ✅ | Depends on init | Optional; verify initAllCursorParityModules + 11500–11574 |
| Network panel | ⚠️ | — | Uses m_networkPanelInitialized, handleNetworkPanelCommand, etc.; reported compile errors (missing members) in some builds |
| Crash reporter | ⚠️ | — | Similar; m_crashReporterInitialized, cmdCrash* |
| Color picker | ⚠️ | — | m_colorPickerInitialized, cmdColorPicker* |
| VulkanRenderer | ⚠️ | — | IRenderer base / include issues in some configs |
| Win32IDE_MCPHooks, OSExplorerInterceptor | ⚠️ | — | Syntax / include issues in some builds |

So: **core IDE and most menus/panels are fully working** where IDs match. **View and Git are broken by ID/routing mismatch.** Several optional panels (Network, Crash, ColorPicker) and optional modules (Vulkan, MCP, etc.) may not build or run in all configurations.

---

## 6. Recommendations

1. **Fix View menu:** Use IDs in 3000–4000 for View toggles (e.g. 3001 = Minimap, 3002 = Output Panel, …) in createMenuBar so they hit handleViewCommand, or add cases 2020–2027 in handleViewCommand and route 2020–2027 to the same logic as 3001–3008.
2. **Fix Git menu:** Use 8001–8005 for Git in createMenuBar, or extend routing so 3020–3024 call handleGitCommand and add cases 3020–3024 in handleGitCommand (mapping to same logic as 8001–8005).
3. **Wire missing Edit/View/Tools/Modules items:** Add handlers for Find Next/Prev, Snippet, Copy Format, Paste Plain, Clipboard History, Profile, Analyze Script, Modules Refresh/Import/Export (or remove menu items if not implemented).
4. **Optional panels:** Restore or add missing members (e.g. m_networkPanelInitialized, handleNetworkPanelCommand) and fix includes so Network, CrashReporter, ColorPicker (and Vulkan/MCP if desired) build and run.
5. **Re-run wiring audit:** After ID and routing fixes, run `python scripts/win32_ide_wiring_audit.py` and refresh `reports/win32_ide_wiring_manifest.md` to confirm connected/missing counts.

---

## 7. Quick Reference

- **Fully working:** File (New/Open/Save/Save As/Exit/Load Model/Recent), Edit (Undo/Redo/Cut/Copy/Paste/Find/Replace/Select All), Terminal (PowerShell/CMD/Stop/Clear/Split), Agent (loop, tools, status, hotpatch), Tools (Voice Automation, Backups, Alert, Shortcuts, SLO), Help, Audit, Voice Chat, layout (menu, toolbar, sidebar, tabs, breadcrumb, editor, status bar, output, PowerShell, file explorer, secondary sidebar).
- **Broken (wrong ID or range):** View (all 2020–2027), Git (all 3020–3024).
- **Missing handler (menu/control exists):** Edit (Find Next/Prev, Snippet, Copy Format, Paste Plain, Clipboard History), Tools (Profile, Analyze Script), Modules (Refresh/Import/Export), and various panel controls unless handled by parent/subclass.
- **Optional / may not build:** Network panel, Crash reporter, Color picker, Vulkan renderer, MCP hooks, OSExplorerInterceptor — fix members and includes for full build.
