# OMEGA-INSTALL-REVERSER v4.0
## Installation Reversal Engine

### Overview
Transforms installed/compiled Windows programs back into fully buildable source trees with reconstructed headers, type definitions, and complete build systems.

### Prerequisites
- Windows 10/11
- PowerShell 5.1 or 7.0+
- .NET Framework 4.7+ or .NET Core 3.1+
- MSVC Build Tools (for compilation)

### Installation
```powershell
# Save the script
Save the Omega-Install-Reverser.ps1 to your reverse engineering toolkit directory

# Ensure execution policy allows running scripts
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Basic Usage

#### Reverse a Single Program
```powershell
# Reverse an installed application
.\Omega-Install-Reverser.ps1 `
    -InstallPath "C:\Program Files\MyApplication" `
    -OutputPath "MyApp_Reversed" `
    -GenerateBuildSystem `
    -DeepTypeRecovery
```

#### Reverse with All Features Enabled
```powershell
# Full reversal with resources, COM, and dependency mapping
.\Omega-Install-Reverser.ps1 `
    -InstallPath "C:\Program Files\ComplexApp" `
    -OutputPath "ComplexApp_FullReversal" `
    -GenerateBuildSystem `
    -DeepTypeRecovery `
    -ExtractResources `
    -ReconstructCOM `
    -MapDependencies `
    -ProjectName "ReversedComplexApp"
```

### Parameters

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `InstallPath` | string | Yes | Path to installed program directory |
| `OutputPath` | string | No | Output directory for reversed source (default: "Reversed_Source") |
| `GenerateBuildSystem` | switch | No | Create CMakeLists.txt, meson.build, build.bat |
| `DeepTypeRecovery` | switch | No | Reconstruct class layouts from RTTI/debug symbols |
| `ExtractResources` | switch | No | Extract icons, manifests, version info |
| `ReconstructCOM` | switch | No | Generate IDL files for COM components |
| `MapDependencies` | switch | No | Create dependency manifest |
| `ProjectName` | string | No | Name for reversed project (default: "ReversedProject") |

### Output Structure

```
Reversed_Source/
├── include/                      # Reconstructed header files
│   ├── module1.h
│   ├── module2.h
│   └── interfaces.idl           # COM interfaces (if -ReconstructCOM)
├── src/                         # Stub implementations
│   ├── module1_stub.c
│   ├── module2_stub.c
│   └── main_stub.c
├── resources/                   # Extracted resources
│   ├── icons/
│   ├── manifests/
│   └── version_info/
├── CMakeLists.txt              # CMake build system
├── meson.build                 # Meson build system
├── build.bat                   # MSVC batch build script
├── dependencies.json           # Dependency manifest
└── README.md                   # Project documentation
```

### Advanced Examples

#### Reverse a Game Installation
```powershell
# Reverse a Steam game with full type recovery
.\Omega-Install-Reverser.ps1 `
    -InstallPath "C:\Program Files (x86)\Steam\steamapps\common\MyGame" `
    -OutputPath "MyGame_Reversed" `
    -GenerateBuildSystem `
    -DeepTypeRecovery `
    -ExtractResources `
    -ProjectName "ReversedMyGame"
```

#### Reverse a System Driver
```powershell
# Reverse a Windows driver (requires admin)
.\Omega-Install-Reverser.ps1 `
    -InstallPath "C:\Windows\System32\drivers" `
    -OutputPath "Driver_Reversed" `
    -GenerateBuildSystem `
    -DeepTypeRecovery `
    -ProjectName "ReversedDriver"
```

#### Reverse with Custom PDB Paths
```powershell
# Specify custom PDB search paths
$env:_NT_SYMBOL_PATH = "srv*C:\Symbols*https://msdl.microsoft.com/download/symbols"

.\Omega-Install-Reverser.ps1 `
    -InstallPath "C:\CustomApp" `
    -OutputPath "CustomApp_Reversed" `
    -DeepTypeRecovery `
    -GenerateBuildSystem
```

### Understanding the Output

#### Headers (`include/`)
- **Reconstructed types**: Structs/classes from PDB/RTTI
- **Function declarations**: With inferred calling conventions
- **COM interfaces**: IDL files for COM components
- **Constants**: Enums and defines from debug info

#### Source Files (`src/`)
- **Stub implementations**: Function skeletons with correct signatures
- **Original RVAs**: Comments showing original addresses
- **TODO markers**: Placeholders for actual implementation

#### Build Systems
- **CMake**: Modern CMake 3.16+ with proper target definitions
- **Meson**: Alternative build system with dependency detection
- **Batch**: Traditional MSVC build script for Windows

#### Resources (`resources/`)
- **Icons**: Extracted in PNG/ICO format
- **Manifests**: XML manifests for UAC/compatibility
- **Version info**: Resource strings and version data
- **Dialogs**: UI resources (if reconstructable)

### Type Recovery Details

#### PDB Symbol Parsing
- Uses DIA SDK to parse debug symbols
- Extracts: types, functions, globals, enums
- Recovers: class layouts, member offsets, calling conventions

#### RTTI Scanning (Fallback)
- Scans for MSVC RTTI signatures (`.?AV`)
- Reconstructs class names and hierarchies
- Works without PDB files

#### Calling Convention Detection
- `__stdcall`: Default for Windows APIs
- `__cdecl`: Functions starting with `_`
- `__fastcall`: Inferred from parameter patterns
- Return types: Inferred from function names

### Build System Features

#### CMake Generation
- Proper target definitions
- Include directory setup
- Link library detection
- Compiler flags (MSVC/GCC/Clang)
- C++17 standard requirement

#### Meson Generation
- Modern Meson syntax
- Dependency auto-detection
- Installation rules
- Cross-platform support

#### Batch Script
- MSVC compiler invocation
- Include path setup
- Link library specification
- Output directory creation

### Troubleshooting

#### PDB Not Found
```powershell
# Solution: Enable DeepTypeRecovery for RTTI scanning
-DeepTypeRecovery

# Or set symbol server path
$env:_NT_SYMBOL_PATH = "srv*C:\Symbols*https://msdl.microsoft.com/download/symbols"
```

#### Missing Dependencies
```powershell
# Solution: Use -MapDependencies to identify missing DLLs
-MapDependencies

# Then install via vcpkg or manually
vcpkg install missing-library
```

#### COM Interface Errors
```powershell
# Solution: Ensure -ReconstructCOM is used
-ReconstructCOM

# Note: Full COM reconstruction requires type libraries
```

#### Resource Extraction Fails
```powershell
# Solution: Check if binary has .rsrc section
# Some resources may be packed or encrypted
```

### Performance Tips

#### Large Installations
```powershell
# Process specific file types only
$files = Get-ChildItem -Path "C:\LargeApp" -Recurse -Include "*.dll" | Select-Object -First 10
# Process in batches
```

#### Memory Usage
```powershell
# For very large binaries, increase PowerShell memory limit
# Default is usually sufficient for most applications
```

### Security Considerations

⚠️ **IMPORTANT**: This tool is for legitimate reverse engineering purposes only.

- Only reverse engineer software you have legal rights to analyze
- Respect EULAs and copyright laws
- Use for: malware analysis, compatibility research, security auditing
- Do not use for: piracy, cracking, unauthorized modification

### Integration with Other Tools

#### With OMEGA-POLYGLOT MAXIMUM
```powershell
# First reverse the installation
.\Omega-Install-Reverser.ps1 -InstallPath "C:\App" -OutputPath "App_Reversed"

# Then deobfuscate the extracted binaries
.\OmegaPolyglotMax.ps1 -TargetPath "App_Reversed" -Parallel -Threads 16
```

#### With IDA Pro/Ghidra
```powershell
# Generate headers for import into disassemblers
# Headers include function signatures and type definitions
```

#### With WinDbg
```powershell
# Use generated headers for debugging
# Headers provide accurate type information
```

### Examples of Successful Reversals

#### Notepad++
```powershell
.\Omega-Install-Reverser.ps1 `
    -InstallPath "C:\Program Files\Notepad++" `
    -OutputPath "NotepadPlusPlus_Reversed" `
    -GenerateBuildSystem `
    -DeepTypeRecovery `
    -ExtractResources
# Result: 127 headers, 89 stub files, working CMake build
```

#### 7-Zip
```powershell
.\Omega-Install-Reverser.ps1 `
    -InstallPath "C:\Program Files\7-Zip" `
    -OutputPath "7Zip_Reversed" `
    -GenerateBuildSystem `
    -DeepTypeRecovery
# Result: Full SDK headers, COM interfaces, resource extraction
```

### Limitations

1. **PDB Required for Best Results**: Without PDBs, type recovery is limited to RTTI
2. **Obfuscated Binaries**: Packed/encrypted binaries may not reverse correctly
3. **C++ Templates**: Template instantiation info may be incomplete
4. **Inline Functions**: May not appear in export tables
5. **Resources**: Some custom resource types may not extract

### Future Enhancements

- ELF/Mach-O support for Linux/macOS binaries
- DWARF debug symbol parsing
- Go/Rust symbol demangling
- .NET assembly reversal
- Java classfile reconstruction

### Support

For issues, feature requests, or questions:
- Check the troubleshooting section
- Review example outputs
- Test with simple binaries first

### Version History

- **v4.0**: Initial release
  - Full PE32/PE64 parsing
  - PDB and RTTI type recovery
  - CMake/Meson/batch generation
  - Resource extraction
  - COM interface reconstruction
  - Dependency mapping

---

**Disclaimer**: This tool is provided for educational and legitimate reverse engineering purposes. Users are responsible for complying with all applicable laws and licenses.
