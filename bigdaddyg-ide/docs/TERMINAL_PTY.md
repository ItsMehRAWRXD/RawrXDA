# Integrated terminal (PowerShell / cmd / PTY)

The bottom **Terminal** panel uses **node-pty** in the Electron main process and **xterm** in the renderer.

## Shells

| Platform | Options |
|----------|---------|
| Windows | PowerShell (`powershell.exe`), Command Prompt (`cmd.exe` / `ComSpec`), PowerShell 7 (`pwsh.exe`) |
| macOS / Linux | Login `bash` or `zsh` |

Working directory is the **open project folder** when it exists on disk; otherwise the user **home** directory.

## Native module (Electron ABI)

`node-pty` is a **native addon**. It must be compiled for the **same Electron version** as the app.

After `npm install` (or when you change Electron version), run:

```bash
cd bigdaddyg-ide
npm run rebuild:pty
```

### Windows prerequisites (native build)

`node-pty` compiles C++ with **node-gyp**. You need:

1. **Python 3** — must be discoverable. If `py -3` is broken (points at a missing exe), either fix the launcher or tell npm explicitly:
   ```powershell
   npm config set python "C:\Users\YOU\AppData\Local\Programs\Python\Python312\python.exe"
   ```
2. **Visual Studio Build Tools** with **“Desktop development with C++”** (MSVC + Windows SDK).

If rebuild still fails, run from a **“x64 Native Tools Command Prompt for VS”** or ensure `PATH` includes MSVC.

If the PTY fails to load, the UI shows `NO_PTY` and copy-pasteable instructions. The embedded terminal stays disabled until rebuild succeeds.

## Elevation (Windows)

**Elevated PowerShell** / **Elevated cmd** use `Start-Process -Verb RunAs` in a helper process. That opens a **separate elevated window** after **UAC**. Output is **not** streamed into the IDE; this is intentional (security and API limits).

- The in-panel PTY always runs as the **normal user** unless you use those buttons for an external admin window.

## Packaged builds (`electron-builder`)

`package.json` includes `asarUnpack` for `node-pty` so the `.node` binary is extracted from the ASAR archive at runtime. If a platform-specific build still fails, verify `node_modules/node-pty` is present in the packaged app and that the correct arch (x64/arm64) was built.

## Security

- Treat the integrated shell like any terminal: **do not paste untrusted commands**.
- Elevation grants **administrator** rights in the **external** window only; approve UAC only when you trust the action.
