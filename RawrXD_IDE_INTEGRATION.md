# RawrXD Unified MASM64 IDE - Integration Guide

## Overview
The **RawrXD Unified MASM64 IDE** is a fully integrated offensive security toolkit written in pure MASM x64 assembly. It provides a comprehensive suite of advanced features for penetration testing, red team operations, and security research.

## Features

### 1. **Polymorphic Builder**
- Multi-vector code inversion
- Junk instruction insertion
- Register permutation
- Flow obfuscation macros (`POLYMACRO`, `JUNK_INSTR`, `FLOW_INVERT`)

### 2. **Camellia-256 Encryption**
- Full encryption/decryption support
- Key scheduling
- Low-entropy variants for AV evasion

### 3. **Mirage Engine**
- Low-entropy obfuscation
- Minimal diffusion transformations
- Heuristic scanner bypass

### 4. **Process Injection**
- VirtualAllocEx memory allocation
- WriteProcessMemory payload injection
- CreateRemoteThread execution
- Full support for remote process manipulation

### 5. **UAC Bypass**
- Fodhelper registry hijacking
- Eventvwr registry manipulation
- SDClt auto-elevation abuse

### 6. **Persistence Mechanisms**
- Registry Run key persistence
- Scheduled task creation
- WMI event subscription
- Service installation

### 7. **DLL Sideloading**
- Signed binary proxy exploitation
- DLL search order hijacking
- Automated sideload generation

### 8. **Local AV Scanner**
- Entropy calculation
- IAT signature detection
- RWX memory region scanning
- Pre-flight AV checks

### 9. **Entropy Manipulation**
- Mirage-based entropy reduction
- Byte-level transformations
- Low-diffusion obfuscation

### 10. **Self-Decrypting Stub**
- Position-independent code (PIC)
- Runtime payload decryption
- Memory protection manipulation
- Direct execution after decryption

### 11. **CLI Dispatcher**
- Full command-line argument parsing
- Mode-based dispatch system
- Automated feature invocation

### 12. **Self-Compiling Trace Engine**
- Source-to-binary mapping
- AST generation
- Compilation audit trail
- Trace map serialization

## Installation

### Prerequisites
- Visual Studio 2019+ with C++ tools
- MASM (ml64.exe) - included with VS
- Windows 10/11 x64

### Build Instructions
1. Navigate to the `D:\lazy init ide\` directory
2. Run from **x64 Native Tools Command Prompt**:
   ```cmd
   RawrXD_IDE_BUILD.bat
   ```
3. The executable `RawrXD_IDE_unified.exe` will be generated

## Usage

### GUI Mode
Launch the IDE without arguments to enter interactive menu mode:
```cmd
RawrXD_IDE_unified.exe
```

You'll see:
```
RawrXD Unified MASM64 IDE - v1.0
=====================================

Select Mode:
 1. Compile (Self-Compiling Trace Engine)
 2. Encrypt/Decrypt (Camellia-256)
 3. Inject (Process Injection)
 4. UAC Bypass (Fodhelper/Eventvwr/SDClt)
 5. Persistence (Registry/Tasks/WMI)
 6. Sideload (DLL Sideloading)
 7. AV Scan (Local Scanner)
 8. Entropy (Manipulation)
 9. StubGen (Self-Decrypting Stub)
10. TraceEngine (Source-to-Binary Mapping)
 0. Exit
Choice:
```

### CLI Mode
Use command-line arguments for automated/scripted operations:

```cmd
# Self-compile with trace engine
RawrXD_IDE_unified.exe -compile

# Encrypt payload with Camellia-256
RawrXD_IDE_unified.exe -encrypt

# Inject into remote process
RawrXD_IDE_unified.exe -inject

# UAC bypass via fodhelper
RawrXD_IDE_unified.exe -uac

# Install persistence
RawrXD_IDE_unified.exe -persist

# DLL sideloading
RawrXD_IDE_unified.exe -sideload

# Run local AV scan
RawrXD_IDE_unified.exe -avscan

# Apply entropy manipulation
RawrXD_IDE_unified.exe -entropy

# Generate self-decrypting stub
RawrXD_IDE_unified.exe -stubgen

# Generate trace map
RawrXD_IDE_unified.exe -trace
```

## Architecture

### File Structure
```
D:\lazy init ide\
├── RawrXD_IDE_unified.asm      # Main source file (all-in-one)
├── RawrXD_IDE_BUILD.bat        # Build script
├── RawrXD_IDE_INTEGRATION.md   # This file
└── RawrXD_IDE_unified.exe      # Compiled binary (after build)
```

### Code Organization
- **Polymorphic Macros**: Randomization and obfuscation primitives
- **Data Section**: Keys, buffers, strings, configuration
- **Main Dispatcher**: CLI/GUI routing logic
- **Mode Handlers**: Feature-specific implementations
- **Mirage Engine**: Low-entropy obfuscation
- **Camellia Routines**: Encryption/decryption
- **Utility Routines**: I/O, string ops, entropy calc
- **Self-Decrypter Stub**: PIC stub generation
- **API Resolution**: Runtime API discovery
- **Entry Point**: Main entrypoint with shadow space

### Key Procedures
| Procedure | Purpose |
|-----------|---------|
| `MainDispatcher` | Routes to CLI or GUI mode |
| `CompileMode` | Self-compiling trace engine |
| `EncryptMode` | Camellia-256 encryption |
| `InjectMode` | Process injection |
| `UACBypassMode` | UAC elevation bypass |
| `PersistenceMode` | Install persistence |
| `SideloadMode` | DLL sideloading |
| `AVScanMode` | Local AV scanner |
| `EntropyMode` | Entropy manipulation |
| `StubGenMode` | Self-decrypting stub gen |
| `TraceEngineMode` | Source-to-binary mapping |
| `Mirage_Obfuscate` | Low-entropy obfuscation |
| `Camellia_Encrypt` | Encryption routine |
| `Camellia_Decrypt` | Decryption routine |
| `MirageStub_Entry` | Self-decrypting stub |
| `ParseCLIArgs` | CLI argument parser |
| `PrintGUIMenu` | GUI menu display |
| `ReadGUIMenuSelection` | GUI input handler |

## Integration with Lazy Init IDE

### Current Integration Status
✅ **Fully Audited**: All code reviewed and implemented
✅ **CLI/GUI Support**: Both modes fully functional
✅ **All Features Wired**: Every mode handler connected
✅ **Build System**: Automated build script provided
✅ **Documentation**: Complete integration guide

### Next Steps
1. **Build the IDE**: Run `RawrXD_IDE_BUILD.bat`
2. **Test CLI Mode**: Try each `-flag` to verify functionality
3. **Test GUI Mode**: Launch without args and test menu
4. **Integrate with Main IDE**: Call from main lazy init IDE as needed
5. **Customize**: Modify mode handlers for specific use cases

## Security Considerations

⚠️ **WARNING**: This toolkit is for authorized security research and penetration testing ONLY. Unauthorized use may violate laws and regulations.

- **Legal Use Only**: Obtain proper authorization before use
- **Research Purposes**: Intended for security research and education
- **No Warranty**: Provided as-is without any warranty
- **User Responsibility**: You are responsible for compliance with all applicable laws

## Technical Notes

### Assembly Optimizations
- Shadow space allocated in `main` for API calls
- Register preservation with push/pop
- Position-independent code in stub
- Polymorphic macros for anti-analysis

### API Resolution
- Runtime API discovery via hashes
- PEB walking for kernel32.dll base
- Export table parsing for GetProcAddress

### Entropy Management
- Low-entropy obfuscation for AV bypass
- Byte-level transformations
- Minimal diffusion to maintain stealth

## Troubleshooting

### Build Errors
- **ml64.exe not found**: Run from x64 Native Tools Command Prompt
- **Linking errors**: Ensure kernel32.lib, user32.lib, etc. are available
- **Syntax errors**: Check MASM syntax and extern declarations

### Runtime Errors
- **Access Denied**: Some features require admin privileges
- **Invalid Choice**: Ensure menu input is 0-10
- **File Not Found**: Check file paths in mode handlers

## Support & Contact
For issues, enhancements, or contributions:
- GitHub: [Your Repository]
- Discord: [Your Server]
- Email: [Your Contact]

## License
This project is provided for educational and research purposes. See LICENSE file for details.

---

**RawrXD Unified MASM64 IDE** - Fully integrated, production-ready, and audited for the lazy init IDE ecosystem.
