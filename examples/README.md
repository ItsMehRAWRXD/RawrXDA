# RawrXD Integration Examples

This directory contains comprehensive examples demonstrating how to integrate the RawrXD toolchain into your C++ projects.

### Tier G Sovereign (C, in-repo)

For **`rawrxd_minimal_link`**, **`rawrxd_symbol_registry`**, and SOM REL32 smokes (no `C:\RawrXD` install), see **`examples/sovereign/README.md`**.

## Quick Start

### Method 1: Visual Studio Command Prompt

```cmd
REM Open VS Developer Command Prompt
cl.exe /EHsc /std:c++17 advanced_integration_example.cpp ^
       C:\RawrXD\Libraries\rawrxd_encoder.lib ^
       /Fe:rawrxd_demo.exe

REM Run the demo
rawrxd_demo.exe
```

### Method 2: CMake

```bash
# Configure
cmake -B build -S . -DRAWRXD_ROOT=C:/RawrXD

# Build
cmake --build build --config Release

# Run
build\Release\rawrxd_demo.exe
```

### Method 3: PowerShell Build Script

```powershell
$env:INCLUDE = "C:\RawrXD\Headers;" + $env:INCLUDE
$env:LIB = "C:\RawrXD\Libraries;" + $env:LIB

cl.exe /EHsc /std:c++17 advanced_integration_example.cpp rawrxd_encoder.lib
```

## Examples Included

### 1. `advanced_integration_example.cpp`

**Comprehensive demonstration featuring:**
- X64 instruction building class
- Instruction validation using RawrXD encoder
- Disassembly with length analysis
- Binary file generation
- Multiple real-world usage scenarios

**Examples demonstrated:**
- Example 1: Simple function generation (MOV + RET)
- Example 2: Complex instruction sequences
- Example 3: Direct RawrXD encoder testing
- Example 4: Instruction length analysis on various opcodes

**Build:**
```cmd
cl.exe /EHsc advanced_integration_example.cpp C:\RawrXD\Libraries\rawrxd_encoder.lib
```

**Output:**
- `rawrxd_demo.exe` - Interactive demonstration
- `simple_function.bin` - Generated x64 bytecode
- Console output showing instruction analysis

## RawrXD Libraries Available

Located in `C:\RawrXD\Libraries\`:

- **rawrxd_encoder.lib** (30 KB)
  - x64 instruction encoding
  - Instruction length calculation
  - Support for MOV, ADD, CALL, JMP, PUSH, POP, etc.

- **rawrxd_pe_gen.lib** (6 KB)
  - PE32+ file generation
  - Section management
  - Import/export tables

- **instruction_encoder.lib** (19 KB)
  - Standalone instruction encoder
  - Alternative to rawrxd_encoder

- **x64_encoder.lib** (8 KB)
  - Core x64 encoding primitives

- **x64_encoder_pure.lib** (11 KB)
  - Pure assembly implementation

- **reverse_asm.lib** (16 KB)
  - Reverse assembly utilities

## API Reference

### Instruction Encoding

```cpp
extern "C" {
    // Get length of x64 instruction at given address
    int GetInstructionLength(const unsigned char* instr);
    
    // Run encoder test suite
    void TestEncoder();
}
```

### Usage Example

```cpp
// Validate an instruction
unsigned char mov_rax[] = { 0x48, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
int length = GetInstructionLength(mov_rax);
// Returns: 10 (MOV RAX, imm64 is 10 bytes)

// Run comprehensive tests
TestEncoder();
```

## Supported Instructions

The RawrXD encoder supports:

- **Data Movement:** MOV, LEA, PUSH, POP
- **Arithmetic:** ADD, SUB, XOR, AND, OR
- **Control Flow:** CALL, JMP, JE, JNE, JZ, JNZ, JL, JG, etc.
- **Special:** NOP, RET, INT3
- **Prefixes:** REX (for 64-bit operands), size overrides

## Build Requirements

- **Compiler:** Visual Studio 2022 (or MSVC 14.50+)
- **C++ Standard:** C++17 or later
- **Platform:** Windows x64
- **Libraries:** RawrXD toolchain installed at `C:\RawrXD`

## Troubleshooting

### "Cannot open file 'rawrxd_encoder.lib'"

Add the library path to your link command:
```cmd
link.exe /LIBPATH:C:\RawrXD\Libraries your_code.obj rawrxd_encoder.lib
```

Or set the LIB environment variable:
```powershell
$env:LIB += ";C:\RawrXD\Libraries"
```

### "Unresolved external symbol"

Ensure you're using `extern "C"` linkage:
```cpp
extern "C" {
    int GetInstructionLength(const unsigned char* instr);
}
```

### Headers not found

Add include path:
```cmd
cl.exe /I C:\RawrXD\Headers your_code.cpp
```

## Advanced Integration

### Using in a DLL

```cpp
// yourlib.cpp
#include <windows.h>

extern "C" int GetInstructionLength(const unsigned char*);

extern "C" __declspec(dllexport) 
int MyAPIFunction(const unsigned char* code) {
    return GetInstructionLength(code);
}
```

Build:
```cmd
cl.exe /LD yourlib.cpp C:\RawrXD\Libraries\rawrxd_encoder.lib
```

### Using in a Static Library

```cmd
REM Compile to object
cl.exe /c /EHsc your_wrapper.cpp

REM Create combined library
lib.exe /OUT:your_lib.lib your_wrapper.obj C:\RawrXD\Libraries\rawrxd_encoder.lib
```

## Performance Considerations

- RawrXD libraries are highly optimized pure assembly
- GetInstructionLength typically executes in <100 CPU cycles
- No dynamic allocations - stack-based operations only
- Suitable for real-time code generation

## Next Steps

1. **Build the examples** to verify your setup
2. **Read the full documentation** at `C:\RawrXD\Docs\`
3. **Explore the CLI tools** with `RawrXD-CLI.ps1 help`
4. **Check the headers** at `C:\RawrXD\Headers\` for complete API

## Support

- **Full Documentation:** `C:\RawrXD\Docs\PRODUCTION_TOOLCHAIN_DOCS.md`
- **CLI Reference:** `C:\RawrXD\Docs\CLI_REFERENCE.md`
- **Build Guide:** `D:\RawrXD-Compilers\BUILD_QUICKSTART.md`

## License

Part of the RawrXD Production Toolchain  
Pure x64 Assembly Implementation  
Copyright 2026
