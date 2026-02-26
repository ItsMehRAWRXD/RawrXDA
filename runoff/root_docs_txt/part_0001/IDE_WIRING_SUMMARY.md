# IDE Wiring & VSIX Loader Summary

## Access Paths (Verified)

| Feature | How to Access |
|---------|---------------|
| **File Explorer** | Activity Bar → **Files**, View → **File Explorer** (Ctrl+Shift+E), or Ctrl+B to show sidebar |
| **AI Chat / Agent panel** | Activity Bar → **Chat**, View → **AI Chat** (Ctrl+Alt+B), View → **Agent Chat (autonomous)**, or title bar **AI** button |
| **Extensions** | Activity Bar → **Exts**, View → Extensions; Install VSIX, Install, Uninstall, Details buttons |
| **Chat Send/Clear** | Type message in AI Chat panel, click **Send** or **Clear** |
| **Audit Dashboard** | Ctrl+Shift+A, or Audit → Show Dashboard |
| **Breadcrumbs** | View → Breadcrumbs; shows File > Class > Method path |

## Placeholder Labels Fixed

- **Chat panel header**: "AI Chat" (was GitHub Copilot Chat)
- **Chat message prefix**: "AI:" (was "Copilot:")
- **Status bar**: "AI" / "AI (off)" (was Copilot)
- **Agent error messages**: "AI Chat input" (was Copilot Chat)
- **Title bar button**: "AI" (was "GH")

## VSIX Loader

### Capabilities

- **Extract .vsix**: PowerShell Expand-Archive (VSIX = ZIP)
- **Parse package.json/manifest.json**: name, version, commands
- **Register metadata**: Extensions appear in Extensions panel
- **Native DLL plugins**: LoadMemoryModule for native extensions

### Limitations (Amazon Q, GitHub Copilot)

- Amazon Q and GitHub Copilot are **full VS Code extensions** (JS/TS, Node.js)
- RawrXD QuickJS host supports only **simple JS extensions** with `main` entry
- These cloud extensions **require** VS Code Extension Host (Node.js) and vscode.* API
- VSIXLoader **can** extract and register their metadata, but **cannot run** their JS code

### Agentic Test

```powershell
# 1. Close RawrXD IDE (required for build)
# 2. Build
ninja -C build_ide RawrXD-Win32IDE

# 3. Run VSIX test (with optional Amazon Q / Copilot .vsix)
cd d:\rawrxd
.\tests\Test-VSIXLoaderAgentic.ps1 -BuildFirst

# With Amazon Q and GitHub Copilot .vsix files:
.\tests\Test-VSIXLoaderAgentic.ps1 -AmazonQVsix "C:\path\to\amazonq.vsix" -GitHubCopilotVsix "C:\path\to\copilot.vsix"
```

Test copies .vsix to `plugins/`, runs `RawrXD-Win32IDE.exe --vsix-test`, reads `vsix_test_result.json` for loaded extension IDs.

## Menu Wiring (Win32IDE_Commands.cpp)

| ID | Menu | Handler |
|----|------|---------|
| 2030 | View → File Explorer | setSidebarView(Explorer) + toggleSidebar if hidden |
| 3007 | View → AI Chat, Agent Chat | toggleSecondarySidebar() |
| 1022 | Title bar AI button | toggleSecondarySidebar() |
| Ctrl+Shift+E | File Explorer shortcut | routeCommand(2030) |
| Ctrl+Alt+B | AI Chat shortcut | routeCommand(3007) |
