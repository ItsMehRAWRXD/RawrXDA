# RawrXD MASM Toolchain — Standalone x64/x86 Compiler, Linker, Builder

Single toolchain under `D:\rawrxd\toolchain\masm` (or `%RAWRXD_ROOT%\toolchain\masm`) for building MASM and NASM assembly into `.exe` or `.dll`. **x64** and **x86** (32-bit) are supported; **x32** means 32-bit — use **x86**.

## Requirements

- **Windows** (x64 host).
- **Visual Studio 2022** (or **Build Tools**) with:
  - Desktop development with C++
  - MSVC x64/x86 build tools
  - Windows 10/11 SDK
- No dependency on E: drive; optional NASM under `D:\rawrxd\compilers\nasm` or `toolchain\nasm`.

## Layout

```
toolchain/masm/
├── Unified-PowerShell-Compiler-RawrXD.ps1   # Compiler + linker (single .asm → exe/dll)
├── Build-MASM-Standalone.ps1                # Builder (single or multi-file, x64/x86)
├── build_masm_standalone.bat                # Batch launcher (x64/x86 + source)
├── README.md                                # This file
├── samples/
│   ├── hello_masm.asm                       # MASM x64 sample
│   └── hello_nasm.asm                       # NASM sample (if present)
└── bin/                                     # Default output (bin\x64, bin\x86)
```

## Usage

### 1. Single file (compiler + linker)

**MASM, x64, console exe:**
```powershell
.\Unified-PowerShell-Compiler-RawrXD.ps1 -Source samples\hello_masm.asm -Tool masm -Architecture x64 -SubSystem console -Entry main
```

**MASM, x86, console exe:**
```powershell
.\Unified-PowerShell-Compiler-RawrXD.ps1 -Source my.asm -Tool masm -Architecture x86 -SubSystem console -Entry main
```

**MASM, x64, GUI exe (WinMain):**
```powershell
.\Unified-PowerShell-Compiler-RawrXD.ps1 -Source app.asm -Tool masm -Architecture x64 -SubSystem windows
```

**NASM, x64:**
```powershell
.\Unified-PowerShell-Compiler-RawrXD.ps1 -Source prog.asm -Tool nasm -Architecture x64 -SubSystem console -Entry start
```

**Output to custom dir, build DLL:**
```powershell
.\Unified-PowerShell-Compiler-RawrXD.ps1 -Source lib.asm -Architecture x64 -OutputType dll -OutDir build\x64
```

### 2. Builder (single or multiple .asm files)

**Single file:**
```powershell
.\Build-MASM-Standalone.ps1 -Source samples\hello_masm.asm -Architecture x64 -Entry main
```

**Directory of .asm → one exe:**
```powershell
.\Build-MASM-Standalone.ps1 -Source D:\rawrxd\src\masm -Architecture x64 -OutputType exe -OutDir build\x64
```

**Directory → one DLL (x86):**
```powershell
.\Build-MASM-Standalone.ps1 -Source src\masm -Architecture x86 -OutputType dll -OutDir build\x86
```

**Clean then build:**
```powershell
.\Build-MASM-Standalone.ps1 -Source samples\hello_masm.asm -Architecture x64 -Clean
.\Build-MASM-Standalone.ps1 -Source samples\hello_masm.asm -Architecture x64 -Entry main
```

### 3. Batch wrapper

From `toolchain\masm`:

```bat
build_masm_standalone.bat x64
build_masm_standalone.bat x86 path\to\file.asm
```

Builds `samples\hello_masm.asm` by default if no source is given.

## Parameters (Unified-PowerShell-Compiler-RawrXD.ps1)

| Parameter       | Values        | Default | Description |
|----------------|---------------|---------|-------------|
| Source         | path          | (required) | .asm file |
| Tool           | masm, nasm    | masm    | Assembler |
| Architecture   | x64, x86      | x64     | Target (x32 = use x86) |
| SubSystem      | console, windows | console | PE subsystem |
| OutDir         | path          | toolchain\masm\bin | Output directory |
| Entry          | symbol        | WinMain (exe), DllMain (dll) | Entry point |
| Runtime        | path          | —       | Extra .obj to link first |
| OutputType     | exe, dll      | exe     | Output kind |

## Parameters (Build-MASM-Standalone.ps1)

| Parameter       | Values        | Default | Description |
|----------------|---------------|---------|-------------|
| Source         | file or dir   | (required) | Single .asm or folder of .asm |
| Architecture   | x64, x86      | x64     | Target |
| OutputType     | exe, dll      | exe     | Output kind |
| OutDir         | path          | bin\<arch> | Output directory |
| SubSystem      | console, windows | console | PE subsystem |
| Entry          | symbol        | WinMain / DllMain | Entry point |
| Clean          | switch        | —       | Remove OutDir before build |

## Tool resolution

- **MSVC:** `D:\VS2022Enterprise\VC\Tools\MSVC`, then `C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC`, then Build Tools. Uses latest version; **Hostx64\x64** for 64-bit, **Hostx64\x86** for 32-bit (ml64/ml, link, lib).
- **Windows Kits:** `C:\Program Files (x86)\Windows Kits\10\Lib`, latest version, **ucrt** and **um** for x64 or x86.
- **NASM:** `D:\rawrxd\compilers\nasm\nasm-2.16.01\nasm.exe`, then `toolchain\nasm\nasm.exe`, then `E:\nasm\...` if present.

## Summary

- **Compiler/linker:** `Unified-PowerShell-Compiler-RawrXD.ps1` — one .asm → one exe or dll, MASM or NASM, **x64 or x86**.
- **Builder:** `Build-MASM-Standalone.ps1` — one or many .asm → one exe or dll, **x64 or x86**.
- **Batch:** `build_masm_standalone.bat` — quick x64/x86 build from cmd.
- **x32:** Use **Architecture x86** for 32-bit Win32.
