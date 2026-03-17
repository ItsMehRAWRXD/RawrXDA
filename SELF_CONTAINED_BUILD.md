# Self-Contained Build System

## Overview
RawrXD is designed to be **fully self-contained** without requiring external build toolchains, SDKs, or runtimes.

## Architecture

### Internal Tools (NO External Dependencies)
```
src/masm/masm_solo_compiler.exe  - Custom x64 MASM assembler (replaces ml64.exe)
src/masm/internal_link.exe       - Custom linker (replaces link.exe)
src/win32app/pe_parser.cpp       - Internal PE/COFF parser (replaces dumpbin.exe)
```

### Removed Dependencies
- ❌ Windows SDK (10.0.22621.0, 10.0.26100.0)
- ❌ Visual Studio Build Tools
- ❌ ml64.exe / link.exe / dumpbin.exe
- ❌ vcvarsall.bat / vswhere.exe
- ❌ CMake external toolchain detection

### Core Technologies
- **Assembly**: Pure x64 MASM with internal compiler
- **Vulkan**: Self-contained compute shaders
- **Win32 API**: Direct Windows API calls (no .NET, no MFC)
- **PE Format**: Internal PE writer/parser

## Build Process

### Fully Self-Contained Build
```powershell
# No SDK required - uses internal toolchain
.\src\masm\masm_solo_compiler.exe /c src\asm\myfile.asm /Fo myfile.obj
.\src\masm\internal_link.exe /OUT:myapp.exe myfile.obj kernel32.lib user32.lib
```

### Runtime Independence
- No DLL dependencies except Windows system DLLs (kernel32.dll, user32.dll)
- No .NET Framework
- No MSVC Runtime (statically linked or custom implementation)
- No external configuration files

## Optional External Tools

### C++ Compilation (Optional)
C++ support requires external compiler (MSVC/Clang/GCC).  
**Recommendation**: Use Assembly (.asm) for fully self-contained builds.

If C++ is needed:
- Install: Visual Studio Build Tools OR
- Install: Clang OR
- Install: MinGW-w64 GCC

### Development Tools (Optional)
- Git (for version control)
- PowerShell (for build scripts)
- VS Code (as code editor)

## Deployment

### Single Executable
The final application is a single `.exe` file with:
- No installer required
- No registry modifications
- No external dependencies
- Portable (runs from any directory)

### Distribution
```
RawrXD_IDE.exe          - Main executable (self-contained)
masm_solo_compiler.exe  - Bundled internal compiler
internal_link.exe       - Bundled internal linker
```

All tools can be embedded as resources inside the main executable for true single-file deployment.

## Verification

### Check Dependencies
```powershell
# List DLL dependencies
dumpbin /dependents RawrXD_IDE.exe

# Should only show:
# KERNEL32.dll
# USER32.dll
# GDI32.dll
# (No MSVCR*.dll, VCRUNTIME*.dll, or other external dependencies)
```

### Build Without SDK
```powershell
# Remove SDK from PATH
$env:PATH = $env:PATH -replace "Windows Kits[^;]*;", ""
$env:PATH = $env:PATH -replace "Visual Studio[^;]*;", ""

# Build should still work using internal tools
.\build_internal.ps1
```

## Architecture Benefits
1. **Zero Installation** - Just copy and run
2. **No Version Conflicts** - Self-contained toolchain
3. **Offline Operation** - No internet/SDK required
4. **Maximum Security** - Minimal attack surface
5. **Long-term Stability** - No external dependency breakage

## Implementation Status
- ✅ Internal MASM compiler
- ✅ Internal PE parser (dumpbin replacement)
- ✅ Internal linker
- ✅ Win32 API direct calls
- ✅ Vulkan compute (self-contained)
- ⚠️ C++ compiler (optional external)
- ⚠️ Standard libraries (minimal/custom implementation)

## Future Enhancements
- [ ] Embed tools as resources (single EXE)
- [ ] Custom C Runtime (eliminate UCRT dependency)
- [ ] Custom standard library (eliminate STL dependency)
- [ ] Integrated debugger (no WinDbg required)
- [ ] Built-in package manager (no external tools)
