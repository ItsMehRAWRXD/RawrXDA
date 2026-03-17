# RawrXD Production Toolchain v2.0

**Pure x64 Assembly - PE Generation & Instruction Encoding**

---

## 🚀 Quick Start

```powershell
# Install CLI to PATH
C:\RawrXD\Install-PATH.ps1

# Generate a PE executable
RawrXD-CLI.ps1 generate-pe myapp.exe

# Verify installation
RawrXD-CLI.ps1 verify

# Show toolchain info
RawrXD-CLI.ps1 info
```

---

## 📚 Documentation Index

| Document | Description | Location |
|----------|-------------|----------|
| **Getting Started** | Main integration guide | [README_INTEGRATION.md](README_INTEGRATION.md) |
| **CLI Reference** | Command-line tool manual | [Docs/CLI_REFERENCE.md](Docs/CLI_REFERENCE.md) |
| **Build Guide** | Compilation instructions | `D:\RawrXD-Compilers\BUILD_QUICKSTART.md` |
| **API Docs** | Production toolchain API | [Docs/PRODUCTION_TOOLCHAIN_DOCS.md](Docs/PRODUCTION_TOOLCHAIN_DOCS.md) |
| **PE Generator** | PE generation quick ref | [Docs/PE_GENERATOR_QUICK_REF.md](Docs/PE_GENERATOR_QUICK_REF.md) |
| **Examples** | Integration examples | [Examples/README.md](Examples/README.md) |

---

## 📦 What's Included

### Executables (`bin\`)
- `pe_generator.exe` - PE32+ executable generator (4.6 KB)
- `instruction_encoder_test.exe` - Encoder test suite (5 KB)

### Libraries (`Libraries\`)
- `rawrxd_encoder.lib` (30 KB) - x64 instruction encoder
- `rawrxd_pe_gen.lib` (6 KB) - PE file generator
- `instruction_encoder.lib` (19 KB) - Standalone encoder
- `x64_encoder.lib`, `x64_encoder_pure.lib`, `reverse_asm.lib`

### Headers (`Headers\`)
- `rawrxd_encoder.h` - Encoder API
- `RawrXD_PE_Generator.h` - PE generation API
- `instruction_encoder.h`, `pe_generator.h`

### CLI Tools
- `RawrXD-CLI.ps1` - PowerShell CLI interface
- `RawrXD-CLI.bat` - Batch wrapper
- `Install-PATH.ps1/.bat` - PATH installers

### Examples (`Examples\`)
- `advanced_integration_example.cpp` - Full C++ demo (300+ lines)
- `CMakeLists.txt` - CMake build configuration
- `README.md` - Integration documentation

### Documentation (`Docs\`)
- 10 comprehensive markdown files covering all aspects

---

## 🛠️ Integration Examples

### Method 1: Visual Studio

```cmd
cl.exe /EHsc myapp.cpp C:\RawrXD\Libraries\rawrxd_encoder.lib
```

### Method 2: CMake

```cmake
add_executable(myapp myapp.cpp)
target_link_libraries(myapp C:/RawrXD/Libraries/rawrxd_encoder.lib)
```

### Method 3: PowerShell

```powershell
$env:LIB += ";C:\RawrXD\Libraries"
cl.exe /EHsc myapp.cpp rawrxd_encoder.lib
```

---

## 📋 CLI Commands

| Command | Description |
|---------|-------------|
| `generate-pe [file]` | Generate PE executable |
| `test-encoder` | Run encoder tests |
| `info` | Show toolchain information |
| `list-libs` | List available libraries |
| `verify` | Verify installation |
| `help` | Show help message |

**Full Reference:** [Docs/CLI_REFERENCE.md](Docs/CLI_REFERENCE.md)

---

## 🔧 Build System

Rebuild the entire toolchain:

```powershell
cd D:\RawrXD-Compilers
.\Build-And-Wire.ps1
```

**Build Options:**
- `-ContinueOnError` - Continue on assembly errors
- `-SkipLink` - Compile only, no linking
- `-IncludeAll` - Build experimental components

**Documentation:** `D:\RawrXD-Compilers\BUILD_QUICKSTART.md`

---

## 💡 Usage Example

```cpp
#include <iostream>

extern "C" {
    int GetInstructionLength(const unsigned char* instr);
}

int main() {
    // MOV RAX, imm64 instruction
    unsigned char mov[] = { 
        0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x00, 0x00, 0x00 
    };
    
    int length = GetInstructionLength(mov);
    std::cout << "Instruction length: " << length << " bytes\n";
    return 0;
}
```

**Compile:**
```cmd
cl.exe example.cpp C:\RawrXD\Libraries\rawrxd_encoder.lib
```

---

## 📊 Technical Specifications

- **Platform:** Windows x64
- **Language:** Pure MASM x64 Assembly
- **Size:** 36 KB core libraries (highly optimized)
- **Performance:** <100 CPU cycles per instruction decode
- **Compiler:** Visual Studio 2022 (MSVC 14.50+)
- **Standards:** PE32+, x64 instruction set

---

## 🎯 Key Features

✅ **Pure Assembly** - No C runtime dependencies  
✅ **Highly Optimized** - Sub-5KB executables  
✅ **Production Ready** - Tested and verified  
✅ **Well Documented** - 10+ documentation files  
✅ **Easy Integration** - CMake, VS, manual builds  
✅ **CLI Tools** - Command-line interface included  
✅ **Examples Provided** - Full working demonstrations  

---

## 🔗 Quick Links

- **Main Docs:** [README_INTEGRATION.md](README_INTEGRATION.md)
- **Examples:** [Examples/README.md](Examples/README.md)
- **CLI Manual:** [Docs/CLI_REFERENCE.md](Docs/CLI_REFERENCE.md)
- **Build Guide:** `D:\RawrXD-Compilers\BUILD_QUICKSTART.md`

---

## 📞 Support

For issues or questions:
1. Check documentation in `Docs\` directory
2. Review examples in `Examples\` directory
3. See build logs in `D:\RawrXD-Compilers\`

---

**Version:** 2.0  
**Last Updated:** January 27, 2026  
**Status:** Production Ready ✅
