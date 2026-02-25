# RawrXD Backend Server (MASM x64)

Production **pure MASM x64** server on port **8080**. Serves static GUI; **proxies all API traffic to the Win32 tool_server / IDE on port 11435**. The HTML frontend uses the **same real features** as the CLI and Win32 GUI (no Python, no Node for backend).

## Architecture

| Layer | Port | Role |
|-------|------|------|
| **MASM server_8080** | 8080 | Serves launcher + HTML/JS from `gui/`; forwards every other request to backend |
| **tool_server / Win32 IDE** | 11435 | Real features: `/status`, `/health`, `/api/model/profiles`, `/api/cli`, `/api/read-file`, agents, hotpatch, etc. |

### Launcher flow (`launch_rawrxd.bat`)

- If **`gui\server_8080.exe`** exists: **Pure MASM backend** — no Node. Launcher starts `src\tool_server.exe --port 11435`, then `gui\server_8080.exe` on 8080; if `gui\webserver.exe` exists, also starts MASM GUI on 3000. Browser opens to 3000 or 8080 accordingly.
- Otherwise: **Node backend** — requires Node.js and `server.js`; starts `node server.js` on 8080. GUI is MASM on 3000 (if `webserver.exe` exists) or Node on 8080.

## Route behaviour

| Request | MASM (8080) |
|---------|-------------|
| GET `/`, `/launcher`, `/chatbot`, `/agents`, … | Serve static HTML from current dir |
| GET `/gui/*` | Serve static file (path after `gui/`) |
| GET `/status`, `/health`, `/api/*`, `/models`, … | **Proxy to 127.0.0.1:11435** (tool_server) |
| POST any | **Proxy to backend** (same) |
| OPTIONS | 200 + CORS |

## Build

From **x64 Native Tools Command Prompt** (or after `vcvars64.bat`):

```bat
cd gui
build_server_8080.bat
```

Produces `server_8080.exe`.

## Run

- **From `gui/`:** `server_8080.exe` — serves `launcher.html`, `ide_chatbot.html`, etc. from current dir.
- **From repo root:** Ensure `gui/*.html` and `gui/*.js` are reachable (or run from `gui/`).
- **One-click (no Node):** From repo root run `launch_rawrxd.bat`. If `gui\server_8080.exe` exists, the launcher uses the **pure MASM backend** path: it starts `tool_server.exe` (from `src\`) on port 11435, then `server_8080.exe` on port 8080, and (if `webserver.exe` exists) the MASM GUI on port 3000. No Node.js required.

Then open **http://localhost:8080** (or **http://localhost:3000/launcher.html** when MASM GUI is used).

## Dependencies

- **tool_server** (or Win32 IDE with built-in server) must be running on **port 11435**. Start it first, e.g. `tool_server.exe --port 11435` from `src\` (build with `src\build_tool_server.bat`), or `RawrXD-Shell.exe --headless --port 11435`. If the backend is down, API requests return 502. When using `launch_rawrxd.bat` with `server_8080.exe` present, the launcher starts `src\tool_server.exe` automatically.

## Design

- Single-threaded accept loop; one request at a time.
- Static: only GUI routes and `/gui/*` are served from disk; everything else is **forwarded unchanged** to `127.0.0.1:11435`.
- Eliminates Python serve dependency; backend is the same C++/Win32 stack the CLI and IDE use.

## Zero-Qt / MASM stack

- **No Qt, no Node (backend):** When using `server_8080.exe` + `tool_server`, the entire GUI→API path is **pure x64 MASM** (server_8080, webserver) + **C++20 Win32** (tool_server, Win32 IDE). No Qt or Electron in the main IDE path.
- **Agent / Titan kernels:** Backend is frameworked with x64 MASM kernels and Titan; see `docs/CLI_GUI_WIN32_QT_MASM_AUDIT.md` for the full audit.
