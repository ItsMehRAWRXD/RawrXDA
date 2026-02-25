# RawrXD Extension Host

**Unified native extension loader.** Replaces 9 scattered Node.js extensions with one MASM64 host that loads `.rawr` DLLs.

## Layout

| File | Purpose |
|------|--------|
| `RawrXD_ExtensionHost.asm` | Host: scans `Extensions\*.rawr`, loads DLLs, routes commands/chat |
| `RawrXD_ExtensionHost_Hijacker.asm` | **Beacon-style injector**: replaces VS Code/Cursor ExtensionHost in-process with RawrXD native modules (no JS) |
| `Build_ExtensionHost_Hijacker.ps1` | Build script: ML64 + LINK → `build\RawrXD_ExtensionHost_Hijacker.exe` |
| `RawrXD_LSP.asm` | **LSP bridge** (replaces rawrxd-lsp-client): CreatePipe, CreateProcess clangd, JSON-RPC Content-Length framing, init/completion commands |
| `RawrXD_Copilot.asm` | **AI/Copilot** (replaces bigdaddyg-copilot): WinHTTP POST to localhost:11434/api/generate, stream tokens to pfnCallback |
| `RawrXD_Agentic.asm` | **Agent** (replaces rawrz-agentic): agentic.compile → CreateProcess build, HandleChat → Ollama streaming |
| `package.json` | Unified VSIX manifest (BigDaddyG, extensionPack) |
| `Wire_Extensions.ps1` | Sanitize `undefined_publisher` → BigDaddyG, create .def stubs |

## Root scripts (D:\rawrxd)

- `ExtensionHarvester.ps1` – Copy JS + package.json from Cursor/VS Code extensions to `harvested\`
- `FixBigDaddyGTypo.ps1` – Fix `opencaht` → `openChat` in installed bigdaddyg-copilot
- `UninstallExtensionBloat.ps1` – Uninstall undefined_publisher and your-name extensions
- `Run_Extension_Autopsy.ps1` – Run Harvest + FixTypo + Wire (use `-All`)

## Commands

```powershell
# Full autopsy (harvest, fix typo, wire)
D:\rawrxd\Run_Extension_Autopsy.ps1 -All

# Wire only + .asm file association hijack
D:\rawrxd\extension-host\Wire_Extensions.ps1 -Native

# Uninstall bloat
D:\rawrxd\UninstallExtensionBloat.ps1

# Build extension OBJs (LSP, Copilot, Agentic)
D:\rawrxd\extension-host\Build_Extensions.ps1

# Build Extension Host Hijacker (MASM64 injector)
D:\rawrxd\extension-host\Build_ExtensionHost_Hijacker.ps1
D:\rawrxd\extension-host\Build_ExtensionHost_Hijacker.ps1 -Deploy   # copy exe to D:\rawrxd\build

# Run hijacker (requires admin) – injects into Code Helper / Cursor Helper
# .\extension-host\build\RawrXD_ExtensionHost_Hijacker.exe
```

## Architecture

```
RawrXD-AgenticIDE.exe (MASM64)
  └── ExtensionHost (this code)
       ├── RawrXD_Agentic_Extension.rawr (DLL)
       ├── BigDaddyG_Copilot.rawr
       └── RawrXD_LSP_Native.rawr
```

Extensions export: `ExtensionInit`, `ExtensionActivate`, `ExtensionExecuteCommand`, `ExtensionHandleChat`.

---

## IDE pane

The Extension Host is surfaced in the RawrXD IDE as a **pop-up panel** (not one of the four main panes). Components that depend on it — LSP completions, Copilot, Agentic commands — open or reveal this panel when needed.

| Who needs it | Purpose |
|--------------|---------|
| Editor | LSP completions, syntax feedback |
| AI Chat | Copilot tokens, Agentic.handleChat |
| Terminal / Debug | Agentic.compile, build integration |
| Command palette | `agentic.compile`, `agentic.chat` and other extension commands |

The panel shows: loaded `.rawr` extensions, status (active / errored), and basic controls. It is a pop-up (like Git panel, hotpatch panel) overlaying the four-pane layout per `ARCHITECTURE.md` §6.5.

---

## Extension Host Hijacker (beacon-style)

**RawrXD_ExtensionHost_Hijacker.asm** is a pure MASM64 reflective loader that:

1. **Finds** VS Code’s or Cursor’s Extension Host process (`Code Helper.exe` / `Cursor Helper`).
2. **Allocates** executable memory in the target and **writes** the RawrXD payload (PIC).
3. **Starts** a remote thread at the payload; the payload sets up a base-relative command registry and loop.
4. **Starts** per-extension threads (ASM completion, LSP bridge) inside the target process.

Result: VS Code’s Node.js extension host is **replaced** by ~20KB of ASM; commands and completions are handled by RawrXD native modules. No DLL on disk for the payload (fileless within the injector’s image until copied into target).

- **Build**: `Build_ExtensionHost_Hijacker.ps1` (requires VS 2022 x64 / ML64).
- **Run**: Execute the built `.exe` **as Administrator** so it can open the target process and create remote threads.
- **Extension format**: RawrXD extensions use a custom header (`RAWRXD_EXT_MAGIC`), entry-point RVA, and command table; they are embedded in the payload and run in the same address space as the hijacked host.

---

## Offline / debug guarantee

The Extension Host and Hijacker are **fully usable offline** and **without any loaded AI model**:

- **Status, debug, and troubleshoot** are available via the Win32 marketplace and AgenticBridge tools: `hijacker_status`, `hijacker_debug`, `hijacker_troubleshoot`. These commands return plain-text information and do not require network, cloud, or model parameters.
- Local agents (or a user at the console) can inspect state, diagnose failures, and follow troubleshooting steps in any environment: off-grid, no internet, no Ollama/API, no model weights.
- The same behavior and guarantees apply in all environments; there is no “online-only” or “model-required” path for extension-host status or debugging.
