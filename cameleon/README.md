# RawrXD Cameleon – Dual-Mode Extension Host

**Exactly middle:** Same DLL runs injected into VS Code/Cursor (Parasite) OR as native RawrXD extension host (Predator). Neither host is privileged; both are equal citizens.

## Architecture

1. **Dual entry** – Same DLL works injected or native.
2. **Context detection** – `CameleonInit` detects parent (RawrXD.exe vs Code.exe/Cursor.exe) and sets mode.
3. **State persistence** – Extension state can be serialized and restored when migrating hosts.
4. **Hot-swap** – `HotSwapMode(MODE_RAWRXD)` / `HotSwapMode(MODE_PARASITE)` to migrate without losing state.
5. **Bridge mode** – Both hosts active; commands synchronized (debugging/comparison).

## Usage

```powershell
# Build DLL and generate Deploy script
.\RawrXD_Cameleon.ps1

# Deploy (auto-detect: Parasite if VS Code/Cursor running, else Native)
.\Deploy_Cameleon.ps1 -Mode Auto

# Force Parasite or Native
.\Deploy_Cameleon.ps1 -Mode Parasite
.\Deploy_Cameleon.ps1 -Mode Native

# Bridge (both hosts)
.\Deploy_Cameleon.ps1 -Mode Bridge
```

## From the IDE

- **Command palette (Ctrl+Shift+P):** `Cameleon: Deploy dual-mode host (Auto)`
- **Build menu:** `Cameleon: Deploy (Auto)`

Runs `cameleon\Deploy_Cameleon.ps1 -Mode Auto` from the workspace root.

## Files

- `RawrXD_Cameleon.ps1` – Builds CameleonCore.asm (and optional NativeHost/ParasiteHost/StateSerializer), compiles with ml64, links RawrXD_Cameleon.dll, writes Deploy_Cameleon.ps1.
- `CameleonCore.asm` – Generated; mode detection, HotSwapMode, BridgeCommand (ML64-compatible).
- `Deploy_Cameleon.ps1` – Launcher; Auto/Parasite/Native/Bridge.

## Reverse-engineering mapping

- VS Code extHost protocol ↔ RawrXD native commands (bidirectional).
- Chrome_Embedder hooks ↔ Win32 WndProc (transparent proxy).
- TypeScript extension API ↔ MASM64 native implementations.
