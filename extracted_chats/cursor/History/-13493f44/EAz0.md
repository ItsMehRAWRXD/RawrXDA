# Assembly Language Support

## Overview

RawrXD now includes comprehensive support for assembly language development with detection and project templates for both **NASM** (Netwide Assembler) and **MASM** (Microsoft Macro Assembler).

## Supported Assemblers

### 1. NASM (Netwide Assembler)
- **Detection**: Automatically detects `nasm` command
- **Platforms**: Windows, Linux, macOS
- **Syntax**: Intel syntax
- **File Extension**: `.asm`

### 2. MASM (Microsoft Macro Assembler)
- **Detection**: Automatically detects:
  - `ml.exe` (32-bit)
  - `ml64.exe` (64-bit)
  - Visual Studio 2022 installation paths
  - Visual Studio 2019 installation paths
- **Platforms**: Windows (Visual Studio)
- **Syntax**: Microsoft syntax
- **File Extension**: `.asm`

## Detection

The system automatically detects installed assemblers:

```powershell
# Agent can use:
detect_languages

# Returns assembly languages if detected:
# {
#   asm: { compiler: "nasm", version: "2.15.05", available: true },
#   nasm: { compiler: "nasm", version: "2.15.05", available: true },
#   masm: { compiler: "ml64", version: "Visual Studio MASM (64-bit) - 14.37", available: true }
# }
```

### Visual Studio MASM Detection

The system checks common Visual Studio installation paths:
- `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\`
- `C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC\`
- `C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\`
- `C:\Program Files (x86)\Microsoft Visual Studio\2022\...`
- `C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\`

It automatically finds the latest MSVC version and detects both 32-bit (`ml.exe`) and 64-bit (`ml64.exe`) assemblers.

## Project Creation

### Create NASM Project
```powershell
# Agent can use:
create_project --project_name "my-asm-app" --language "nasm"

# Creates:
# my-asm-app/
#   ├── main.asm
#   ├── build.sh (Linux)
#   ├── build.bat (Windows)
#   └── README.md
```

### Create MASM Project
```powershell
# Agent can use:
create_project --project_name "my-masm-app" --language "masm"

# Creates:
# my-masm-app/
#   ├── main.asm
#   ├── build.bat
#   └── README.md
```

### Create Generic Assembly Project
```powershell
# Agent can use:
create_project --project_name "my-asm-app" --language "asm"

# Creates project with both NASM and MASM build scripts
```

## Project Templates

### NASM Template
- **main.asm**: Hello World program (x86-64 Linux)
- **build.sh**: Linux build script
- **build.bat**: Windows build script
- **README.md**: Build instructions

### MASM Template
- **main.asm**: Hello World program (Windows x86-64)
- **build.bat**: Visual Studio build script
- **README.md**: Build instructions with VS paths

### Generic Assembly Template
- **main.asm**: Cross-assembler Hello World
- **build.bat**: Windows build script (NASM)
- **build.sh**: Linux build script (NASM)
- **build_masm.bat**: MASM build script
- **README.md**: Instructions for both assemblers

## Build Instructions

### NASM (Linux)
```bash
nasm -f elf64 main.asm -o main.o
ld main.o -o main
./main
```

### NASM (Windows)
```bash
nasm -f win64 main.asm -o main.obj
gcc main.obj -o main.exe
main.exe
```

### MASM (Visual Studio Developer Command Prompt)
```bash
# 64-bit
ml64 /c main.asm
link /subsystem:console main.obj kernel32.lib /entry:main
main.exe

# 32-bit
ml /c /coff main.asm
link /subsystem:console main.obj kernel32.lib
main.exe
```

## Language Aliases

The system recognizes these aliases for assembly:
- `asm` → assembly
- `assembly` → asm
- `.asm` → asm
- `nasm` → NASM
- `masm` → MASM

## Integration with Other Tools

### Clang and MinGW Detection
RawrXD also detects:
- **Clang**: `clang`, `clang++` (for C/C++)
- **MinGW**: `gcc`, `g++` (for C/C++)
- **Visual Studio**: `cl.exe` (MSVC compiler)

These are automatically detected and can be used for linking assembly programs.

## Example: Complete Assembly Workflow

1. **Detect assemblers**:
   ```powershell
   detect_languages
   # Finds: nasm, masm (if installed)
   ```

2. **Create project**:
   ```powershell
   create_project --project_name "hello-asm" --language "nasm"
   ```

3. **Build and run**:
   ```bash
   # Windows
   .\build.bat
   
   # Linux
   chmod +x build.sh
   ./build.sh
   ```

## Notes

- **NASM** is cross-platform and works on Windows, Linux, and macOS
- **MASM** is Windows-only and requires Visual Studio
- Both assemblers use Intel syntax but have some differences
- MASM requires Windows API calls for I/O operations
- NASM can use system calls directly on Linux

## Visual Studio Integration

If you have Visual Studio 2022 installed:
- MASM tools are automatically detected
- No need to manually set PATH
- Use Visual Studio Developer Command Prompt for easiest setup
- Or ensure `ml64.exe`/`ml.exe` are in your PATH

## Future Enhancements

- Support for other assemblers (GAS, YASM, FASM)
- Platform-specific optimizations
- Integration with debuggers (GDB, WinDbg)
- Assembly syntax highlighting improvements

