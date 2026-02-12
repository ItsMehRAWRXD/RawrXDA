# Win32 IDE Wiring Manifest — ESP/IE Labeled

**Purpose:** Source digestion and reverse-engineering traceability. All menus, submenus, breadcrumb menus, file explorer, and UI components are explicitly labeled (ESP = Explicit, IE = In Essence/ID) for end-to-end wiring.

## Creation Order (onCreate)

| Step | Function | ESP Label | Component | Parent |
|------|----------|-----------|-----------|--------|
| 1 | createMenuBar | ESP:m_hMenu | Main menu bar | m_hwndMain |
| 2 | createToolbar | ESP:m_hwndToolbar | Title bar + controls | m_hwndMain |
| 3 | createActivityBar | ESP:m_hwndActivityBar | Activity Bar (Files, Search, SCM, Debug, Exts, Recov) | m_hwndMain |
| 4 | createPrimarySidebar | ESP:m_hwndSidebar | Sidebar container + content | m_hwndMain |
| 5 | createTabBar | ESP:m_hwndTabBar | Editor tab bar | m_hwndMain |
| 6 | createBreadcrumbBar | ESP:IDC_BREADCRUMB_BAR | Symbol path bar (File > Class > Method) | m_hwndMain |
| 7 | createLineNumberGutter | ESP:m_hwndLineNumbers | Line number margin | m_hwndMain |
| 8 | createEditor | ESP:IDC_EDITOR | RichEdit editor | m_hwndMain |
| 9 | createTerminal | ESP:IDC_COMMAND_INPUT, terminal panes | Terminal + command input | m_hwndMain |
| 10 | createStatusBar | ESP:IDC_STATUS_BAR | Status bar | m_hwndMain |
| 11 | createOutputTabs | ESP:IDC_OUTPUT_TABS | Output panel tabs | m_hwndMain |
| 12 | createPowerShellPanel | ESP:m_hwndPowerShellPanel | PowerShell panel | m_hwndMain |

## Menu Hierarchy (ESP Labeled)

### Top-Level Menus (m_hMenu)
- **File** (hFileMenu) — IDM_FILE_* (2001–2005, 1030)
- **Edit** (hEditMenu) — IDM_EDIT_* (2007–2019)
- **View** (hViewMenu) — IDM_VIEW_*, IDM_T1_BREADCRUMBS_TOGGLE (12020)
- **Terminal** (hTerminalMenu) — IDM_TERMINAL_* (3001–3006)
- **Tools** (hToolsMenu) — IDM_TOOLS_*, Voice, Backup, Alert, Shortcuts, SLO
- **Modules** (hModulesMenu) — IDM_MODULES_* (3050–3052)
- **Help** (hHelpMenu) — IDM_HELP_* (4001–4004)
- **Audit** (hAuditMenu) — IDM_AUDIT_* (9500+)
- **Git** (hGitMenu) — IDM_GIT_* (3020–3024)
- **Agent** (hAgentMenu) — IDM_AGENT_*, AI Options, Context Window, Hotpatch
- **Cursor Parity** (createCursorParityMenu) — Composer, Mentions, Vision, Refactor, Lang, Semantic, Resource

### Submenus (ESP Labeled)
- Appearance: Theme + Transparency
- Voice Chat: IDM_VOICE_*
- Voice Automation: TTS for AI
- Backup: IDM_QW_BACKUP_*
- Alert & Monitoring: IDM_QW_ALERT_*
- AI Options: IDM_AI_MODE_*, Context (4K–1M)
- Hotpatch: Memory, Byte, Server, Proxy layers
- Autonomy: IDM_AUTONOMY_*
- Reverse Engineering: IDM_REVENG_*

## Breadcrumb Bar

| ESP Label | ID | Source | Handler |
|-----------|-----|--------|---------|
| IDC_BREADCRUMB_BAR | 9825 | Win32IDE_Breadcrumbs.cpp | BreadcrumbProc |
| IDM_T1_BREADCRUMBS_TOGGLE | 12020 | View menu | handleTier1Command |
| m_hwndBreadcrumbs | HWND | createBreadcrumbBar | updateBreadcrumbs, paintBreadcrumbs |

**Flow:** createBreadcrumbBar(hwnd) → CreateWindowEx STATIC → Subclass BreadcrumbProc → onSize positions between tab bar and editor.

## File Explorer

| ESP Label | ID | Source | Handler |
|-----------|-----|--------|---------|
| m_hwndExplorerTree | — | Win32IDE_Sidebar.cpp createExplorerView | SidebarProc, FileExplorerProc |
| IDC_ACTIVITY_EXPLORER | 6001 | Activity Bar | setSidebarView(SidebarView::Explorer) |
| IDC_FILE_EXPLORER | 1025 | Win32IDE.cpp (legacy createFileExplorer) | — |
| IDC_FILE_TREE | 1026 | TreeView child | — |

**Primary Path:** createPrimarySidebar → createExplorerView → m_hwndExplorerTree (TreeView in m_hwndSidebarContent). Activity Bar button IDC_ACTIVITY_EXPLORER switches to Explorer view.

## Control ID Ranges

| Range | Purpose |
|-------|---------|
| 1001–1026 | Core controls (Editor, Terminal, StatusBar, FileExplorer, FileTree) |
| 1100–1107 | Activity Bar (ACTBAR_EXPLORER, SEARCH, SCM, DEBUG, etc.) |
| 1200–1207 | Secondary Sidebar (Copilot, AI toggles) |
| 1300–1332 | Panel, Output Tabs, Debugger |
| 1400–1411 | Status bar items |
| 1500–1502 | Command Palette |
| 6001–6070 | Sidebar views (Explorer, Search, SCM, Debug, Extensions, Recovery) |
| 7001–7032 | Plan Approval, Retry |
| 9825 | IDC_BREADCRUMB_BAR |

## Layout (onSize)

1. **Toolbar** — Top full width
2. **Activity Bar** — Far left, contentTop to contentBottom
3. **Sidebar** — Left of editor, m_sidebarWidth
4. **Tab Bar** — Above editor area
5. **Breadcrumb Bar** — Between tab bar and editor (ESP:IDC_BREADCRUMB_BAR)
6. **Line Numbers** — Left of editor
7. **Editor** — Center
8. **Minimap** — Right of editor
9. **Output Tabs** — Bottom
10. **PowerShell Panel** — Bottom
11. **Secondary Sidebar** — Right (Copilot)
12. **Status Bar** — Bottom full width

## Wiring Verification

- **Menus:** createMenuBar (Win32IDE.cpp) → createCursorParityMenu(m_hMenu)
- **Breadcrumbs:** createBreadcrumbBar (onCreate) → onSize positions → View → Breadcrumbs toggle
- **File Explorer:** createPrimarySidebar → createExplorerView → setSidebarView(Explorer)
- **Activity Bar → Sidebar:** Activity buttons → setSidebarView → ShowWindow per view
