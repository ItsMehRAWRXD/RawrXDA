# IDE Wiring and VSIX Loader — Complete Reference

This document covers (1) **menu wiring** for File Explorer, Chat panel, and Agent in all IDE builds, and (2) **VSIX loader** completion and **agentic testing** with Amazon Q and GitHub Copilot.

---

## 1. Full Win32 IDE (RawrXD-Win32IDE, `src/win32app`)

### Menu → feature wiring (already implemented)

| Menu / Shortcut | Command ID | Action |
|-----------------|------------|--------|
| **View > File Explorer** | `IDM_VIEW_FILE_EXPLORER` (2030) | Shows primary sidebar with **Explorer** view; if sidebar hidden, toggles it on. Ctrl+Shift+E. |
| **View > AI Chat** | `IDM_VIEW_AI_CHAT` (3007) | Toggles **secondary sidebar** (AI Chat / Agent panel). Ctrl+Alt+B. |
| **View > Agent Chat (autonomous)** | `IDM_VIEW_AGENT_CHAT` (3009) | Same panel as AI Chat — for autonomous/agentic use. |
| **Activity bar “Chat”** | `IDC_ACTIVITY_CHAT` (6007) | Same as View > AI Chat — `toggleSecondarySidebar()`. |
| **Agent menu** | `IDM_AGENT_START_LOOP`, `IDM_AGENT_BOUNDED_LOOP`, etc. | Routed via `routeCommand` / `routeCommandUnified`; handlers in `Win32IDE_Commands.cpp` and agent panels. |

### How commands get to handlers

1. **WM_COMMAND** from menu or accelerator → `Win32IDE::onCommand(hwnd, id, ...)` (`Win32IDE_Core.cpp`).
2. **View/legacy** → `routeCommand(id)` → `handleViewCommand()` in `Win32IDE_Commands.cpp` (cases 2030, 3007, etc.).
3. **Unified path** → `routeCommandUnified(id, this, hwnd)` for commands in the central registry.

So **File Explorer** and **AI Chat** are not placeholders: they call `setSidebarView(SidebarView::Explorer)` + `toggleSidebar()` and `toggleSecondarySidebar()`.

**Activity Bar:** Clicking **Files** (or Search, Source, etc.) now calls `toggleSidebar()` if the sidebar is hidden, so the panel is shown when switching views.

### “GitHub chat” / breadcrumbs

- There is **no separate “GitHub chat”** product in the app. Docs that mention “GitHub Copilot” are comparisons.
- **AI Chat** = secondary sidebar (View > AI Chat, Ctrl+Alt+B, or Activity bar “Chat”). That is the single chat/agent panel for autonomous and agentic use.

### Build fix (Sidebar)

- `s_sidebarContentOldProc` and `SidebarContentProc` are now class-scoped: **static member** `Win32IDE::s_sidebarContentOldProc` in `Win32IDE.h`, defined in `Win32IDE_Sidebar.cpp`, and `SetWindowLongPtr(..., Win32IDE::SidebarContentProc)` so the compiler resolves the callback.
- If **LNK1104** (cannot open `RawrXD-Win32IDE.exe`) appears, close any running instance of the IDE and rebuild.

---

## 2. VSIX Loader — Completed Behavior

### Where it lives

- **Loader:** `src/modules/vsix_loader_win32.cpp` (Win32 IDE build), header `src/modules/vsix_loader.h`.
- **Initialization:** `main_win32.cpp` calls `VSIXLoader::GetInstance().Initialize("plugins")`.
- **UI:** Extensions panel in `Win32IDE_Sidebar.cpp` lists loaded plugins and **.vsix files** in `plugins/`; Install on a .vsix entry calls `LoadPlugin(path)`.

### What was finished

1. **.vsix extraction (no libzip)**  
   `ExtractVSIX()` uses **PowerShell `Expand-Archive`** to unpack a .vsix into `plugins_dir_/<stem>`.

2. **LoadPlugin(.vsix path)**  
   - If the path is a **.vsix file**: extract to `plugins_dir_/<stem>`, then detect VS Code layout (`extension/package.json` vs root `package.json`) and call `LoadPluginFromDirectory(load_root)`.
   - If the path is a **directory**, load from that directory as before.

3. **VS Code package.json support**  
   `LoadPluginFromDirectory()` now:
   - Tries **manifest.json** first (custom RawrXD manifest).
   - Else uses **extractJsonString** + **LoadPluginFromPackageJson** to parse `package.json` (project nlohmann::json::parse is a stub that doesn't parse objects). Extracts `name`, `version`, `publisher`, `displayName`, `description` and builds a manifest for `id` = `publisher.name` or `name`.

4. **Sidebar: .vsix listed as installable**  
   In `loadInstalledExtensions()`, **.vsix files** in `plugins/` are listed with `id` = stem (e.g. `amazonq`, `github-copilot`); **Install** uses `LoadPlugin(pluginsDir + "\\" + id + ".vsix")`, which triggers extract + load.

---

## 3. Agentic Test (Amazon Q and GitHub Copilot)

### 3.1 `--vsix-test` mode (no GUI)

- **Usage:** run the IDE exe with `--vsix-test` (e.g. `RawrXD-Win32IDE.exe --vsix-test`).
- **Behavior:**  
  - Scans `plugins/` for `*.vsix`.  
  - Calls `LoadPlugin(path)` for each.  
  - Writes **`plugins\vsix_test_result.json`** with `loaded` (extension ids) and `help` (id → help string).  
  - Process **exits** (no window).

### 3.2 PowerShell script: `tests/Test-VSIXLoaderAgentic.ps1`

- **Auto-discovery:** If `-AmazonQVsix` / `-GitHubCopilotVsix` are not provided, the script auto-discovers Amazon Q and GitHub Copilot from `%USERPROFILE%\.vscode\extensions\` (amazonwebservices.amazon-q-vscode-*, github.copilot-*), copies them to `plugins/`, and runs the loader.
- **Parameters:**  
  `-AmazonQVsix`, `-GitHubCopilotVsix` (or env `AMAZONQ_VSIX`, `GITHUB_COPILOT_VSIX`), optional `-IdeExe`, `-BuildFirst`, `-SkipCopy`.
- **Steps:**  
  1. Locate or build `RawrXD-Win32IDE.exe`.  
  2. Ensure `plugins` exists next to the exe.  
  3. Copy provided .vsix into `plugins` as `amazonq.vsix` and `github-copilot.vsix` (unless `-SkipCopy`).  
  4. Set `RAWRXD_ALLOW_UNSIGNED_EXTENSIONS=1`.  
  5. Run `RawrXD-Win32IDE.exe --vsix-test`.  
  6. Read `plugins\vsix_test_result.json` and print loaded ids + help; pass if expected patterns (e.g. `amazon*`, `*copilot*`) appear.

**Example:**

```powershell
# Download or obtain Amazon Q and GitHub Copilot .vsix, then:
$env:RAWRXD_ALLOW_UNSIGNED_EXTENSIONS = "1"
.\tests\Test-VSIXLoaderAgentic.ps1 -AmazonQVsix "C:\path\to\amazon-q.vsix" -GitHubCopilotVsix "C:\path\to\github-copilot.vsix" -BuildFirst
```

### 3.3 Manual test in the IDE

1. Put `amazonq.vsix` and `github-copilot.vsix` (or your copies) into `bin/plugins/` (next to `RawrXD-Win32IDE.exe`).
2. Launch the IDE, open **Extensions** (Activity bar → Exts).
3. You should see **amazonq** and **github-copilot** as installable (description “(Install .vsix)”).
4. Click **Install** for each; the loader will extract and load; they appear in the loaded list.

---

## 4. Summary checklist

| Item | Status |
|------|--------|
| View > File Explorer (full IDE) | Wired → sidebar Explorer + toggle (2030, Ctrl+Shift+E). |
| View > AI Chat (full IDE) | Wired → secondary sidebar (3007, Ctrl+Alt+B). |
| View > Agent Chat (autonomous) (full IDE) | Wired → same panel (3009). |
| Activity bar Chat button | Wired → same as View > AI Chat. |
| Agent menu (full IDE) | Wired via routeCommand / routeCommandUnified. |
| VSIX loader: .vsix extract | Implemented (PowerShell Expand-Archive). |
| VSIX loader: package.json | Implemented (VS Code manifest). |
| Sidebar: list .vsix as installable | Implemented. |
| --vsix-test + result JSON | Implemented. |
| Test-VSIXLoaderAgentic.ps1 | Implemented. |
| Sidebar build (s_sidebarContentOldProc) | Fixed (class static member). |

---

## 5. Ship IDEs (simplified builds)

- **RawrXD_Win32_IDE.cpp** and **Win32_IDE_Complete.cpp** (in `Ship/`) have their own menu sets and wiring; see **Ship/MENU_WIRING_DIAGNOSIS.md** for View > File Explorer, Chat panel, Fullscreen, and AI > Chat fixes there.
