# Mac/Linux Wrapper — Bootable Space for RawrXD IDE

RawrXD is a **native Win32** IDE. This wrapper allows launching it on **macOS and Linux** without porting the entire codebase, by creating an isolated **Wine** environment ("bootable space") and running the Windows build there.

## Requirements

- **Wine** (64-bit): `wine` on Linux, `wine64` or `wine` on macOS (e.g. via Homebrew: `brew install wine-stable`).
- A **Windows build** of RawrXD (e.g. from a Windows machine or CI): copy `build_ide/bin/` (or `build_ide/Release/`) containing `RawrXD-Win32IDE.exe` and `RawrXD_CLI.exe` to the Mac/Linux host.

## Quick Start

```bash
# From repo root (after copying a Windows build into build_ide/bin or setting RAWRXD_BUILD_DIR)
chmod +x scripts/rawrxd-space.sh
./scripts/rawrxd-space.sh

# CLI only (no GUI)
./scripts/rawrxd-space.sh --cli

# Custom prefix and build dir
./scripts/rawrxd-space.sh --prefix "$HOME/.wine-rawrxd" -- "$HOME/rawrxd-build/bin"
```

## Environment Variables

| Variable | Description |
|----------|-------------|
| `RAWRXD_SPACE_DIR` | Wine prefix directory (default: `$HOME/.rawrxd/rawrxd-space`). |
| `RAWRXD_BUILD_DIR` | Directory containing RawrXD-Win32IDE.exe / RawrXD_CLI.exe. |
| `LAUNCH_CLI` | Set to `1` to launch RawrXD_CLI.exe instead of the GUI. |
| `WINE` | Wine executable (default: `wine`; use `wine64` on macOS if needed). |

## What This Wrapper Does

1. **Isolated prefix**: Uses a dedicated Wine prefix so RawrXD does not share state with other Windows apps.
2. **Launch**: Runs `RawrXD-Win32IDE.exe` (GUI) or `RawrXD_CLI.exe` (CLI) under Wine.
3. **No full port**: The IDE remains Windows-native; no reverse-engineering or reimplementation of the full IDE on Mac/Linux.

## Limitations

- **Wine compatibility**: Some Win32/COM features may not behave identically to Windows.
- **Performance**: GUI under Wine can be slower than native Windows.
- **Build**: You must build RawrXD on Windows (or use a prebuilt artifact); the wrapper does not compile the IDE on Mac/Linux.

## See Also

- **IDE_LAUNCH.md** — Which executable to run on Windows.
- **README.md** — Build instructions (Windows).
