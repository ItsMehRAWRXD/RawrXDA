# VS Code / Cursor: log locations, “patch” errors, and freezes

Continuation of ad-hoc debugging notes when the editor hangs or AI tools spam **patch** failures.

## Where logs actually are (Windows)

| Product | Folder |
|--------|--------|
| **VS Code** | `%APPDATA%\Code\logs` → e.g. `C:\Users\<you>\AppData\Roaming\Code\logs` |
| **Cursor** | `%APPDATA%\Cursor\logs` |

Each run is a **timestamped subfolder** (e.g. `20260321T031852`). Inside a session:

| Log | Typical path |
|-----|----------------|
| Main process | `...\logs\<session>\main.log` |
| Window / renderer (UI + much agent noise) | `...\logs\<session>\window1\renderer.log` or `renderer.1.log` |
| Extension host | `...\logs\<session>\window1\exthost\exthost.log` |
| Git | `...\logs\<session>\window1\exthost\vscode.git\Git.log` |

**In UI:** Command Palette → **Developer: Open Logs Folder**.

**Install folder** (e.g. `D:\VS Code`) is **not** where session logs live. `debug.log` next to `Code.exe` is usually **Chromium** stderr (see `docs/WEBVIEW2_CHROMIUM_STDERR.md`).

## “Patch” messages in logs

Search renderer logs for `patch` / `Patch`:

```powershell
Select-String -Path "$env:APPDATA\Code\logs\*\window*\renderer*.log" -Pattern "patch|Patch" |
  Select-Object -Last 50 Path, LineNumber, Line
```

Common pattern:

`Failed to generate patch file for untracked file: file:///...`

That usually means the tool tried to diff **untracked** or **ignored** paths (often **huge** files: model blobs, `sqlite3.c`, heap dumps). Repeating failures can **load the renderer** and feel like a freeze.

**Mitigations:**

1. **Open only the repo folder** you need — avoid multi-root workspaces that include `D:\`, `OllamaModels`, duplicate trees, etc.
2. Add **excludes** (workspace `.vscode/settings.json` or User settings) — see below.
3. **Git:** track or ignore consistently; don’t leave giant trees **untracked** under the same workspace root.

## Sample workspace excludes (copy into `.vscode/settings.json`)

Adjust paths to your machine. Goal: fewer watchers + fewer accidental AI/index targets.

```json
{
  "files.watcherExclude": {
    "**/OllamaModels/**": true,
    "**/.git/objects/**": true,
    "**/build*/**": true,
    "**/node_modules/**": true
  },
  "search.exclude": {
    "**/OllamaModels/**": true,
    "**/*.gguf": true,
    "**/build*/**": true
  },
  "files.exclude": {
    "**/.git/objects/**": true
  }
}
```

Use **`files.exclude`** sparingly (hides from tree); prefer **`search.exclude`** and **`files.watcherExclude`** first.

## If the window is already frozen

1. **Task Manager** → end **Code.exe** / **Cursor.exe** (and stuck **Helper** processes if needed).
2. Start with logging to console:  
   `"D:\VS Code\Code.exe" --verbose`  
   (or your Cursor path) and watch the terminal.
3. Open the **newest** folder under `%APPDATA%\Code\logs` and inspect `renderer*.log` + `exthost.log` from the **last good run**.

## Related docs

- `docs/WEBVIEW2_CHROMIUM_STDERR.md` — ICU / Crashpad lines vs RawrXD.
- `docs/VSCODE_BLANK_WINDOW_FIX.md` — blank WebView / GPU / user-data hints.
- `docs/PROMPT_REVERSE_ENGINEER_IDE_LAUNCH_AUDIT.md` — Win32 IDE launch audit prompt (different product, same “find the real log” mindset).
