# Main Frame / GGUF / Build — Reality Check

## Main Window and Qt

**The production Win32 IDE does not use QMainWindow or QApplication.**

| Build | Entry | Main window |
|-------|--------|-------------|
| **RawrXD-Win32IDE** (ship) | `main_win32.cpp` → `WinMain` | `Win32IDE` C++ class, `CreateWindowExW`, no Qt |
| **qtapp** (optional) | `main_qt.cpp` → `QApplication` | `MainWindow : QMainWindow` |

- **Qt lives only in `src/qtapp/`** (MainWindow.cpp, main_qt.cpp, etc.).
- **Win32 IDE** (`src/win32app/`) uses `WinMain`, `CreateWindowExW`, `RICHEDIT_CLASSW` for the editor, and HWNDs for sidebar/panels. No Qt in that path.

So “Main Window Frame hemorrhaging Qt” applies to the **Qt build** (qtapp), not to the Win32 IDE you ship. Replacing QMainWindow with an ASM frame only affects the Qt app; the Win32 main frame is already native.

---

## Why the P0 1‑liners would fail

### MainFrame ASM

- Uses **32‑bit MASM**: `.686`, `.MODEL FLAT`, `invoke`, `DefWindowProcA`, `ADDR`, etc.
- You’re calling **ml64** (64‑bit). ml64 does **not** support that syntax.
- Result: **assembly fails** (e.g. “instruction or directive not allowed in 64-bit mode”).

### GGUF Loader ASM

- Same issue: 32‑bit directives with `ml64`.
- Tensor layout (e.g. “sizeof(TensorEntry)=40”) does not match real GGUF; would need to match `ggml`/llama.cpp structures.
- `rep movsb` in the snippet uses `rbx` for count; in x64 `rep movsb` uses `rcx` for count. Logic is wrong as written.

### Build Orchestrator

- Script only collects `*.asm` and links them. It **omits all C++** (WinMain, Win32IDE, RichEdit, etc.).
- Result: you’d get an exe with no real UI, no message loop, no app. CMake is there because the bulk of the app is C++.

---

## What to do instead

1. **MainFrame**  
   - For **Win32 IDE**: no Qt to remove; frame is already Win32.  
   - For **qtapp**: if you want to experiment with a pure-ASM host, write a **ml64‑compatible** frame (`.CODE`, no `invoke`, Win64 ABI, proper `EXTERN`/imports) and a small C/C++ shim that calls it, instead of running the 32‑bit snippet.

2. **GGUF loader**  
   - Keep using the existing C++ GGUF/ggml path for correctness.  
   - If you want a **MASM64 hot path**, add a small **ml64** module that matches the real tensor layout and is called from C++ (e.g. one function: load one tensor or one dequant block). Don’t replace the whole loader with the current fictional ASM.

3. **Build**  
   - Keep CMake for the main app (C++ + ASM).  
   - Optionally add a **BUILD_NATIVE.ps1** that **invokes CMake** (e.g. configure + build) and then does extra steps (manifest, signing), rather than replacing CMake with “compile only ASM and link.”

---

## Summary

- **Don’t execute** the P0/P1 1‑liners as given: wrong ASM mode (32 vs 64), wrong GGUF layout, and build script would drop all C++.
- **Main window**: Win32 IDE is already Qt‑free; focus Qt removal on qtapp only if you still ship that.
- **Next steps**: If you want ASM, add **ml64‑compatible** modules and integrate them from the existing C++ build (CMake + MASM) instead of replacing the main frame or the GGUF loader in one shot.
