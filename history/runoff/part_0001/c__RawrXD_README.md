# RawrXD PE Generator & Encoder Suite

**Status:** ✅ **Production Ready** - Build Automation Complete

Welcome to RawrXD, a comprehensive x86-64 assembly toolkit for PE generator and instruction encoder components.

## 🚀 Quick Start

### Build Everything in One Command
```batch
C:\RawrXD\Wire-RawrXD.bat
```

This builds and deploys:
- ✅ PE Generator executable
- ✅ Instruction encoder test utility
- ✅ Static libraries
- ✅ All headers and documentation

### Run the PE Generator
```batch
C:\RawrXD\bin\pe_generator.exe
```

## 📚 Documentation

- **[Build Automation Status](./Docs/BUILD_AUTOMATION_STATUS.md)** - Current build status, deliverables, and setup
- **[PE Generator Quick Reference](./Docs/PE_GENERATOR_QUICK_REF.md)** - API and usage guide
- **[Instruction Encoder Docs](./Docs/INSTRUCTION_ENCODER_DOCS.md)** - Encoder implementation details
- **[Production Delivery Index](./Docs/PRODUCTION_DELIVERY_INDEX.md)** - Complete inventory
- **[Production Toolchain Docs](./Docs/PRODUCTION_TOOLCHAIN_DOCS.md)** - Toolchain setup

## 📦 Directory Structure

```
C:\RawrXD\
├── bin/                      # Compiled executables
│   ├── pe_generator.exe
│   └── instruction_encoder_test.exe
├── Libraries/                # Static libraries
│   ├── rawrxd_encoder.lib
│   ├── rawrxd_pe_gen.lib
│   └── [legacy libs]
├── Headers/                  # Public headers for integration
│   ├── RawrXD_PE_Generator.h
│   ├── rawrxd_encoder.h
│   └── [other headers]
├── Source/                   # Reference source code
│   ├── Encoders/
│   ├── PE-Generator/
│   └── Examples/
├── Docs/                     # Complete documentation
│   └── [11 documentation files]
├── Wire-RawrXD.bat          # GUI/CLI entry point (batch)
├── Wire-RawrXD.ps1          # PowerShell wrapper
└── README.md                 # This file
```

## 🛠️ Advanced Usage

### Custom Build with PowerShell
```powershell
# Build with full feature set (includes optional components)
C:\RawrXD\Wire-RawrXD.ps1

# Build with error tolerance
PowerShell -File "D:\RawrXD-Compilers\Build-And-Wire.ps1" -ContinueOnError

# Skip linking (assemble & lib only)
PowerShell -File "D:\RawrXD-Compilers\Build-And-Wire.ps1" -SkipLink
```

### Rebuild from Source
Source files and the full build automation script are located in:
```
D:\RawrXD-Compilers\
```

Key files:
- `Build-And-Wire.ps1` - Main orchestration script
- `RawrXD_PE_Generator_PROD.asm` - PE generator implementation
- `instruction_encoder_production.asm` - Encoder core
- `x64_encoder_production.asm` - x64 ISA compliance

## ✅ Build Status

| Component | Status | Size | Notes |
|-----------|--------|------|-------|
| pe_generator.exe | ✅ | 4.5 KB | Main executable |
| instruction_encoder_test.exe | ✅ | 5.0 KB | Test utility |
| rawrxd_encoder.lib | ✅ | 30 KB | Static library |
| rawrxd_pe_gen.lib | ✅ | 5.8 KB | Static library |
| All headers | ✅ | 43 KB | API integration |
| Documentation | ✅ | Complete | 11 files |

## ⚠️ Known Issues

**RawrXD_PE_Generator_FULL.asm** - Optional experimental component has formatting issues. The production PROD version is clean and ready for use. See [Build Automation Status](./Docs/BUILD_AUTOMATION_STATUS.md) for details.

## 🔧 Requirements

- Windows x86-64 system
- Visual Studio 2022 Enterprise (with MASM64)
- Windows SDK 10.0.26100.0 or later
- PowerShell 5.0+

The build scripts auto-detect the toolchain; no manual PATH configuration needed.

## 📞 Support

For build issues, refer to:
- `D:\RawrXD-Compilers\Build-And-Wire.ps1` - Commented source
- `C:\RawrXD\Docs\` - Full documentation set

## 📝 License

See individual source files for licensing information.

---

**Last Updated:** Current Session  
**Build Status:** ✅ **Production Ready**  
**Automation:** Fully automated via Wire-RawrXD.bat
