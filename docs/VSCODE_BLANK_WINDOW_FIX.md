# VS Code opens blank (Windows) — fix guide

Especially relevant when VS Code lives under **`D:\VS Code`** (path contains a **space**) or on a non-default drive.

## 0. Fast fix: Windows “occlusion” bug (chrome paints, editor area empty)

Chromium’s **native window occlusion** on Windows can leave the **workbench blank** while the title bar / theme still look fine.

**Try this first** (one line):

```powershell
& "D:\VS Code\Code.exe" --disable-features=CalculateNativeWinOcclusion
```

If that works, make it permanent on your **shortcut** / **taskbar** target, e.g.:

`"D:\VS Code\Code.exe" --disable-features=CalculateNativeWinOcclusion`

Or from this repo (everything + clean profile if it’s *still* blank):

```powershell
pwsh -File D:\rawrxd\scripts\Launch-VSCode-Safe.ps1 -MaxCompat
```

(`-MaxCompat` = GPU off, extensions off, fresh `%TEMP%` profile, **and** occlusion disabled.)

### Console: “not in the list of known options”

If you see:

`Warning: 'disable-features' is not in the list of known options, but still passed to Electron/Chromium.`

that is **normal**. VS Code only “knows” its own CLI flags; **`--disable-features=...` is a Chromium flag** and still applies. Same idea if you ever see a warning for other Electron/Chromium switches.

**Do not use** `--disable-software-rasterizer` unless you know you need it — current VS Code often reports it as unknown; this repo’s `Launch-VSCode-Safe.ps1` does **not** add it. If your script still prints that flag, you’re running an **old copy** or pasted an outdated script — use `D:\rawrxd\scripts\Launch-VSCode-Safe.ps1` from disk: `pwsh -File ...`.

### “Error mutex already exists”

That usually means **another `Code.exe` is already running** (visible window, tray, or stuck background process). A **second** start can hit the single-instance mutex and log this error.

1. **File → Exit** (or **Alt+F4**) on every VS Code window; check the **system tray** for Code.
2. **Task Manager** → end any remaining **`Code.exe`**.
3. Launch again (e.g. `-MaxCompat`).

Optional (⚠ **closes all VS Code; unsaved work is lost**):

```powershell
pwsh -File D:\rawrxd\scripts\Launch-VSCode-Safe.ps1 -MaxCompat -CloseExistingCode
```

---

## 1. Path with a space — quoting & shortcuts

If the install folder is `D:\VS Code`, many shortcuts and scripts break unless the path is **quoted**.

- **Wrong:** `D:\VS Code\Code.exe` in a `.bat` without quotes → splits at the space.
- **Right:** `"D:\VS Code\Code.exe"` or use the **`code.cmd`** shim in that folder (it usually handles paths correctly).

**Recommendation:** Prefer install path **without** spaces, e.g. `D:\VSCode`, or always launch via:

```powershell
& "D:\VS Code\Code.exe" .
```

## 2. Blank window = GPU / Electron (most common)

Try launching **once** with GPU disabled (use **only** flags VS Code recognizes; random Chromium flags may warn and do nothing):

```powershell
& "D:\VS Code\Code.exe" --disable-gpu
```

If that fixes it, make it permanent (see `argv.json` below).

### 2b. Title bar / theme visible — **middle of the window is empty**

1. **`CalculateNativeWinOcclusion`** — use **`--disable-features=CalculateNativeWinOcclusion`** (see [§0](#0-fast-fix-windows-occlusion-bug-chrome-paints-editor-area-empty) above). This is the most common fix when the shell renders but the center does not.

2. **Extensions** (very common after PowerShell / Copilot / theme updates) — start with everything off:
   ```powershell
   pwsh -File D:\rawrxd\scripts\Launch-VSCode-Safe.ps1 -DisableGpu -DisableExtensions -FreshUserData
   ```
   If the UI **works** with `-FreshUserData` and `-DisableExtensions`, restore your real profile gradually: copy only `User\settings.json` and `User\keybindings.json` into `%APPDATA%\Code\User\`, then re-enable extensions in small batches.

3. **Corrupt workbench cache** (without nuking the whole profile) — exit VS Code, then delete or rename:
   - `%APPDATA%\Code\CachedData`
   - `%APPDATA%\Code\Code Cache` (folder)
   - `%APPDATA%\Code\User\workspaceStorage` (fixes bad saved editor state per folder)

4. **If you can open the Command Palette** (`Ctrl+Shift+P`): run **Developer: Reload Window**, then **Developer: Show Running Extensions** and disable anything that errors.

5. **Renderer logs** — with VS Code closed, then started with `--verbose`, open the newest folder under `%APPDATA%\Code\logs\` and read `renderer*.log` for WebGL / GPU / extension host crashes.

6. **Updates disabled** — log lines like `update#setState disabled` are normal if you turned off updates; they are **not** the cause of a blank workbench.

### Option A — `argv.json` (persists)

1. Open **Command Palette** → **Preferences: Configure Runtime Arguments** (or edit manually):
   - Stable: `%APPDATA%\Code\argv.json`
   - Portable: `D:\VS Code\data\argv.json` (if you use a `data` folder next to `Code.exe`)

2. Ensure valid JSON, e.g.:

```json
{
  "disable-hardware-acceleration": true
}
```

3. Fully quit VS Code (tray icon too) and start again.

### Option B — script

From repo root:

```powershell
# Occlusion-only (try before anything else if chrome shows but center is empty)
pwsh -File .\scripts\Launch-VSCode-Safe.ps1 -DisableWinOcclusion

# GPU only
pwsh -File .\scripts\Launch-VSCode-Safe.ps1 -DisableGpu

# Still blank: full isolation (GPU + no extensions + temp profile + occlusion off)
pwsh -File .\scripts\Launch-VSCode-Safe.ps1 -MaxCompat
```

`-FreshUserData` creates a temp profile under `%TEMP%`; your normal `%APPDATA%\Code` is untouched.

## 3. Corrupted user data / cache

Rename (don’t delete yet) the user data folder so VS Code starts “fresh”:

| Type    | Typical path |
|---------|----------------|
| Stable  | `%APPDATA%\Code` |
| Insiders | `%APPDATA%\Code - Insiders` |
| Portable | `D:\VS Code\data` |

Quit VS Code, rename e.g. `Code` → `Code.backup`, start again. If the window works, copy back `User\settings.json` only if you want old settings.

## 4. Portable install layout

If you copied only `Code.exe` without resources:

- Use the **full** archive from [VS Code download](https://code.visualstudio.com/Download) (same CPU arch: x64 / ARM64).
- For portable mode, create a **`data`** folder beside `Code.exe` (see [Portable mode](https://code.visualstudio.com/docs/editor/portable)).

## 5. Verbose log (when still blank)

From **cmd** or PowerShell:

```powershell
& "D:\VS Code\Code.exe" --verbose 2>&1 | Tee-Object -FilePath "$env:TEMP\vscode-verbose.log"
```

Check the log for GPU, extension host, or path errors.

## 6. Antivirus / Controlled folder access

Allow **`Code.exe`** and the **`resources`** folder under your install path. Blocking DLL load from `resources\app` often shows a white/blank window with no clear UI error.

## Quick checklist

- [ ] Launch with quoted path: `"D:\VS Code\Code.exe"`
- [ ] **`--disable-features=CalculateNativeWinOcclusion`**
- [ ] Try `--disable-gpu` (drop unknown Chromium-only flags if VS Code warns)
- [ ] If chrome shows but center is empty: **`--disable-extensions`** + **`--user-data-dir`** (see script `-FreshUserData`)
- [ ] Delete `CachedData`, `Code Cache`, or `workspaceStorage` under `%APPDATA%\Code`
- [ ] Set `"disable-hardware-acceleration": true` in `argv.json`
- [ ] Reset/rename full user `Code` folder if still broken
- [ ] Confirm full install (not a partial copy)
- [ ] Check verbose log and AV exclusions
- [ ] If **`Error mutex already exists`**: quit all VS Code / kill **`Code.exe`**, or use `-CloseExistingCode` (see [§0](#0-fast-fix-windows-occlusion-bug-chrome-paints-editor-area-empty))

---

*This document is maintained for RawrXD contributors who also use VS Code on Windows; it does not change RawrXD IDE behavior.*
