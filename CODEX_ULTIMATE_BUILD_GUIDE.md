# CODEX ULTIMATE v7.0 - BUILD GUIDE

## Build Status: ⚠️ REQUIRES MASM64

**File:** `CodexUltimate.asm`  
**Size:** 69,164 bytes (67.5 KB)  
**Lines:** 2,327 lines  
**Architecture:** x64 (64-bit)  
**Format:** MASM64 (Microsoft Macro Assembler for x64)

## Current Status

❌ **Cannot build with MASM32** - This is a 64-bit assembly file  
❌ **MASM64 not installed** - Standard MASM64 package not found  
✅ **VS2022 available** - Visual Studio 2022 with MASM64 detected

## Requirements

### Option 1: Install MASM64 Package (Recommended)
```
1. Download MASM64 from Microsoft
2. Extract to C:\masm64\
3. Ensure include64\ and lib64\ directories exist
4. Run build script
```

### Option 2: Use VS2022 Build Tools
```
1. Open "x64 Native Tools Command Prompt for VS 2022"
2. cd /d "D:\lazy init ide"
3. ml64.exe /c /nologo CodexUltimate.asm
4. link.exe /SUBSYSTEM:CONSOLE /OPT:NOWIN98 CodexUltimate.obj
```

### Option 3: Create VS2022 Project
```
1. Open Visual Studio 2022
2. Create new "Empty Project"
3. Add CodexUltimate.asm
4. Configure for MASM64 build
5. Build solution
```

## File Analysis

### Architecture
```
Format:     MASM64 (64-bit)
Includes:   \masm64\include64\*.inc
Libraries:  \masm64\lib64\*.lib
Entry:      OPTION WIN64:3 (64-bit Windows)
```

### Features (from header)
- ✅ Complete PE32/PE32+ parsing (all 16 data directories)
- ✅ Rich Header analysis (VS_VERSION_INFO, manifest resources)
- ✅ Exception Directory parsing (x64 unwind codes, ARM64 PAC)
- ✅ TLS Directory reconstruction (callbacks, index)
- ✅ Load Config analysis (CFG, Guard CF, security cookies, SEH)
- ✅ Delay Import & Bound Import resolution
- ✅ .NET Metadata parsing (CLI Header, metadata streams)
- ✅ Resource Directory tree traversal (icon/string/version extraction)
- ✅ Export forwarding resolution (API sets, delay-load)
- ✅ Import Name Table vs IAT dual parsing
- ✅ Base Relocation processing (high/low/64-bit)
- ✅ Debug Directory (CodeView PDB70/PDB20, dwarf, misc)
- ✅ Section entropy analysis (Shannon entropy for packer detection)
- ✅ String extraction (ASCII/Unicode, min/max length filtering)
- ✅ Compiler detection patterns (MSVC/GCC/Clang/Delphi/Rust/Go)
- ✅ Function boundary detection (prologue/epilogue signatures)
- ✅ Cross-reference generation (call/jmp targets)
- ✅ Type reconstruction (MSVC RTTI Complete Object Locator)

### Comparison to OMEGA-POLYGLOT v4.0

| Feature | OMEGA v4.0 (32-bit) | Codex v7.0 (64-bit) |
|---------|---------------------|---------------------|
| Architecture | x86 (32-bit) | x64 (64-bit) |
| Code Size | 10.9 KB (463 lines) | 67.5 KB (2,327 lines) |
| PE Directories | Basic (3-4) | All 16 directories |
| Rich Header | ❌ No | ✅ Yes |
| Exception Dir | ❌ No | ✅ Yes (unwind codes) |
| TLS Directory | ❌ No | ✅ Yes (callbacks) |
| Load Config | ❌ No | ✅ Yes (CFG/Guard CF) |
| Delay Imports | ❌ No | ✅ Yes |
| .NET Metadata | ❌ No | ✅ Yes |
| Resource Tree | Basic | Full traversal |
| Export Forwarding | ❌ No | ✅ Yes |
| Base Relocations | ❌ No | ✅ Yes (all types) |
| Debug Directory | ❌ No | ✅ Yes (PDB/dwarf) |
| Entropy Analysis | Basic | Advanced |
| String Extraction | ASCII only | ASCII + Unicode |
| Compiler Detection | ❌ No | ✅ Yes (7 compilers) |
| Function Boundaries | ❌ No | ✅ Yes (prologue/epilogue) |
| Cross-References | ❌ No | ✅ Yes (call/jmp) |
| RTTI Reconstruction | ❌ No | ✅ Yes (MSVC RTTI) |

## Build Commands

### Using MASM64 (Recommended)
```batch
ml64.exe /c /nologo CodexUltimate.asm
link.exe /SUBSYSTEM:CONSOLE /OPT:NOWIN98 CodexUltimate.obj
```

### Using VS2022 Native Tools
```batch
"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe" /c /nologo CodexUltimate.asm
"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe" /SUBSYSTEM:CONSOLE /OPT:NOWIN98 CodexUltimate.obj
```

## Next Steps

1. **Install MASM64** from Microsoft
2. **Extract to C:\masm64\**
3. **Run build script:** `build_codex_ultimate.bat`
4. **Test executable:** `CodexUltimate.exe`

## Alternative: Use OMEGA-POLYGLOT v4.0

Since CodexUltimate requires MASM64, you can use the already-built:
- **OMEGA-POLYGLOT v4.0 PRO** (`omega_pro.exe`)
- Location: `D:\lazy init ide\omega_pro.exe`
- Size: 3.5 KB (32-bit, fully functional)

---

**Status:** ⚠️ Requires MASM64 installation  
**Recommendation:** Install MASM64 or use existing OMEGA-POLYGLOT tool
