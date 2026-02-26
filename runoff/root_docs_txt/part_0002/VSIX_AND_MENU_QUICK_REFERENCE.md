# VSIX Loader & IDE Menu Wiring — Quick Reference

## What’s already implemented

### VSIX loader (complete)
- **Location:** `src/modules/vsix_loader_win32.cpp`, `src/modules/vsix_loader.h`
- **.vsix extraction:** PowerShell `Expand-Archive` (no libzip)
- **Load by path:** `LoadPlugin(.vsix)` → extract to `plugins/<stem>` → load from `extension/` or root using **package.json** (VS Code) or **manifest.json**
- **Sidebar:** `.vsix` files in `plugins/` show as installable; **Install** runs extract + load
- **Agentic test mode:** `RawrXD-Win32IDE.exe --vsix-test` loads all `plugins/*.vsix`, writes `plugins\vsix_test_result.json`, exits

### Full IDE menu wiring (complete)
| Action | How to use it |
|--------|----------------|
| **File Explorer** | **View > File Explorer** or **Ctrl+Shift+E** → primary sidebar with Explorer (file tree) |
| **AI Chat** | **View > AI Chat** or **Ctrl+Alt+B** → secondary sidebar (chat/agent panel) |
| **Agent Chat (autonomous)** | **View > Agent Chat (autonomous)** → same panel as AI Chat |
| **Activity bar** | Click **Chat** (right side) → same chat/agent panel |

Handlers: `Win32IDE_Commands.cpp` (2030, 3007, 3009), `Win32IDE_Core.cpp` (accelerators + onCommand). No separate “GitHub chat”; the one chat/agent panel is used for all.

---

## Test VSIX loader with Amazon Q and GitHub Copilot (agentic)

### 1. Get the .vsix files
- **Amazon Q:** Install from VS Code marketplace or download the .vsix (e.g. from [marketplace](https://marketplace.visualstudio.com/)).
- **GitHub Copilot:** Same; get the .vsix (e.g. “GitHub Copilot” or “GitHub Copilot Chat” extension).

### 2. Run the agentic test script
From repo root (e.g. `d:\rawrxd`):

```powershell
$env:RAWRXD_ALLOW_UNSIGNED_EXTENSIONS = "1"
.\tests\Test-VSIXLoaderAgentic.ps1 -AmazonQVsix "C:\path\to\amazon-q.vsix" -GitHubCopilotVsix "C:\path\to\github-copilot.vsix"
```

Optional:
- **`-BuildFirst`** — build `RawrXD-Win32IDE` before running (if exe missing).
- **`-SkipCopy`** — don’t copy .vsix into `plugins/` (use files already in `build_ide\bin\plugins\`).

### 3. What the script does
1. Finds `RawrXD-Win32IDE.exe` (e.g. `build_ide\bin\`).
2. Ensures `plugins` exists next to the exe.
3. Copies your .vsix into `plugins\` as `amazonq.vsix` and `github-copilot.vsix`.
4. Runs `RawrXD-Win32IDE.exe --vsix-test`.
5. Reads `plugins\vsix_test_result.json` and prints loaded extension ids and help.
6. Pass = script exits 0; if you passed .vsix paths, it expects those extensions (or name patterns) in `loaded`.

### 4. Manual test in the IDE
1. Put `amazonq.vsix` and `github-copilot.vsix` into `build_ide\bin\plugins\`.
2. Start **RawrXD-Win32IDE**.
3. Open **Extensions** (Activity bar → Exts).
4. Click **Install** on **amazonq** and **github-copilot**; they should extract and load.

---

## If something isn’t wired

- **File Explorer / Chat / Agent Chat not working:** Ensure you’re running **RawrXD-Win32IDE** from `build_ide\bin\` (full IDE), not a Ship standalone. Menu IDs 2030, 3007, 3009 are in `Win32IDE.cpp` and handled in `Win32IDE_Commands.cpp` and `Win32IDE_Core.cpp`.
- **VSIX test “Result file not found”:** Run the exe once from `build_ide\bin` with `--vsix-test` so `plugins\vsix_test_result.json` is created; then run the script again from repo root.
- **Build LNK1104:** Close any running RawrXD-Win32IDE and rebuild.

More detail: **IDE_WIRING_AND_VSIX.md**, **Ship/MENU_WIRING_DIAGNOSIS.md**.
