# RawrXD Fortress Toolchain

This folder contains the IDE’s **own** assembler toolchain and scripts, so builds work from **D:\rawrxd** without depending on E:.

## Contents

- **nasm/** – NASM 2.16.01 (nasm.exe, ndisasm.exe). Fully portable; used by the compiler script.
- **masm/** – MASM/NASM build scripts and samples. They use:
  - **NASM:** `nasm` from this folder (`toolchain\nasm\nasm.exe`).
  - **MASM/link:** System MSVC (ml64.exe, link.exe) and Windows Kits (ucrt, um).
- **mingw/** – (Optional) MinGW-w64 GCC 15.2.0 from E:. Fully self-contained C/C++; no MSVC or SDK needed. See **E_DRIVE_COMPILER_AUDIT.md** for copy instructions.
- **from_scratch/** – Fortress toolchain progression: Phase 2 linker (COFF→PE32+, complete). See **from_scratch/INDEX.md** for Phase 1/2/3 status.

## Building .asm from the IDE

- **Run** or **Build** on an `.asm` file: the IDE calls  
  `toolchain\masm\Unified-PowerShell-Compiler-RawrXD.ps1`  
  with `-Tool masm` by default, `-SubSystem console`, and `-OutDir` set to the file’s directory.

## Building .asm from the command line

```powershell
# MASM (default)
powershell -NoProfile -ExecutionPolicy Bypass -File "D:\rawrxd\toolchain\masm\Unified-PowerShell-Compiler-RawrXD.ps1" -Source "D:\rawrxd\asm-sources\working_ide.asm" -Tool masm -SubSystem console -OutDir "D:\rawrxd\asm-sources"

# NASM
powershell -NoProfile -ExecutionPolicy Bypass -File "D:\rawrxd\toolchain\masm\Unified-PowerShell-Compiler-RawrXD.ps1" -Source "D:\rawrxd\toolchain\masm\samples\hello_nasm.asm" -Tool nasm -SubSystem console -Entry start -OutDir "D:\rawrxd\toolchain\masm\bin"
```

## Environment

- **RAWRXD_ROOT** – If set (e.g. to `D:\rawrxd`), the script uses `%RAWRXD_ROOT%\toolchain\nasm\nasm.exe`. Otherwise it uses `D:\rawrxd` and then falls back to `E:\nasm\nasm-2.16.01\nasm.exe`.

See **FORTRESS_E_DRIVE_AUDIT.md** in the repo root for the full E-drive audit and migration details.
