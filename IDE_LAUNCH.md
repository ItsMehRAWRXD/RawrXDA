# Which IDE / executable to run

There are **two different** executables that can be built/run from this repo. Don’t mix them up.

---

## 1. Win32 GUI IDE (your main IDE)

- **What it is:** The C++ Win32 IDE (editor, chat, agent, settings, etc.).
- **Build:** CMake target `RawrXD-Win32IDE`.
  ```powershell
  cd D:\rawrxd\build_ide
  cmake --build . --config Release --target RawrXD-Win32IDE
  ```
- **Run:**
  - **Preferred:** `.\Launch_RawrXD_IDE.bat` from repo root (starts the GUI IDE).
  - Or run the exe directly:
    - `build_ide\bin\RawrXD-Win32IDE.exe`
    - or `build_ide\Release\RawrXD-Win32IDE.exe` (depends on your generator).
- **Name:** `RawrXD-Win32IDE.exe` (hyphen).

---

## 2. RE / MASM64 toolkit (console menu — not the GUI IDE)

- **What it is:** The “RawrXD Unified MASM64 IDE” — a **console** app built from `RawrXD_IDE_unified.asm` (reverse‑engineering / codex toolkit). It shows the menu:
  - Select Mode: Compile, Encrypt/Decrypt, Inject, UAC Bypass, Persistence, etc.
- **Build:** `.\RawrXD_IDE_BUILD.ps1` (outputs `RawrXD_IDE_unified.exe` in repo root).
- **Run:** `.\RawrXD_IDE_unified.exe` (or `RawrXD_IDE.exe` if you renamed/copied it).
- **Name:** Script output is `RawrXD_IDE_unified.exe`. Any `RawrXD_IDE.exe` in the repo root is typically this RE toolkit (or a copy of it), **not** the Win32 GUI.

---

## Summary

| You want…              | Run this |
|------------------------|----------|
| **GUI IDE** (editor, chat, agent) | `Launch_RawrXD_IDE.bat` or `build_ide\bin\RawrXD-Win32IDE.exe` |
| **RE / codex console menu**       | `RawrXD_IDE_unified.exe` (or `RawrXD_IDE.exe` if that’s the RE build) |

If you see the "Select Mode: 1. Compile, 2. Encrypt/Decrypt…" menu, you're in the **RE toolkit**, not the GUI IDE. Use `Launch_RawrXD_IDE.bat` or `RawrXD-Win32IDE.exe` from `build_ide` for the GUI.

**Mac/Linux:** Run `./scripts/rawrxd-space.sh` — see **docs/MAC_LINUX_WRAPPER.md** for Wine bootable space setup and usage.
