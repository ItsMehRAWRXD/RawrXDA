# RawrXD Unified MASM64 IDE - Quick Reference

## Quick Start
```cmd
# Build
cd "D:\lazy init ide"
RawrXD_IDE_BUILD.bat

# Run GUI Mode
RawrXD_IDE_unified.exe

# Run CLI Mode
RawrXD_IDE_unified.exe -compile
```

## All CLI Commands
| Command | Description |
|---------|-------------|
| `-compile` | Self-compiling trace engine |
| `-encrypt` | Camellia-256 encryption/decryption |
| `-inject` | Process injection (VirtualAllocEx, CreateRemoteThread) |
| `-uac` | UAC bypass (fodhelper, eventvwr, sdclt) |
| `-persist` | Persistence (registry, tasks, WMI) |
| `-sideload` | DLL sideloading via signed binaries |
| `-avscan` | Local AV scanner (entropy, IAT, RWX) |
| `-entropy` | Entropy manipulation and obfuscation |
| `-stubgen` | Generate self-decrypting stub |
| `-trace` | Source-to-binary trace mapping |

## Key Features
✅ Polymorphic builder with multi-vector inversion
✅ Camellia-256 encryption with low-entropy variants
✅ Mirage engine for heuristic bypass
✅ Process injection with remote thread execution
✅ UAC bypass via registry hijacking
✅ Multiple persistence mechanisms
✅ DLL sideloading automation
✅ Local AV signature scanner
✅ Entropy manipulation for stealth
✅ Self-decrypting stub generation (PIC)
✅ CLI dispatcher with full argument parsing
✅ Interactive GUI menu system
✅ Self-compiling trace engine

## File Locations
- **Source**: `D:\lazy init ide\RawrXD_IDE_unified.asm`
- **Build Script**: `D:\lazy init ide\RawrXD_IDE_BUILD.bat`
- **Documentation**: `D:\lazy init ide\RawrXD_IDE_INTEGRATION.md`
- **Output Binary**: `D:\lazy init ide\RawrXD_IDE_unified.exe`

## Build Requirements
- Visual Studio 2019+ (x64 Native Tools Command Prompt)
- MASM (ml64.exe)
- Windows SDK libraries (kernel32.lib, user32.lib, advapi32.lib, shell32.lib)

## Integration Status
✅ **Fully Audited & Implemented**
✅ **All Features Wired & Functional**
✅ **CLI & GUI Modes Complete**
✅ **Located in D:\lazy init ide**
✅ **Ready for Direct Assembly & Use**

## Architecture Highlights
- **Pure MASM x64**: No C/C++ dependencies
- **Position-Independent Code**: Self-decrypting stub supports PIC
- **Runtime API Resolution**: GetProcAddress by hash
- **Polymorphic Macros**: Anti-analysis primitives
- **Low-Entropy Obfuscation**: Mirage engine for AV evasion

## Security Warning
⚠️ For authorized security research and penetration testing ONLY. Unauthorized use may violate laws.
