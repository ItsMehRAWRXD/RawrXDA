# 🔧 Assembly Build System & OOP Analysis

A comprehensive solution for building pure assembly code and analyzing object-oriented patterns.

## 🚀 Quick Start

### 1. Build Assembly Code
```bash
# Basic build
python assembly_build_system.py your_file.asm

# With custom output name
python assembly_build_system.py your_file.asm -o my_program

# Cross-platform build
python assembly_build_system.py your_file.asm --target-os windows --target-arch amd64
```

### 2. Analyze OOP Structure
```bash
# Analyze assembly for OOP patterns
python assembly_build_system.py your_file.asm --analyze-oop

# Generate build script
python assembly_build_system.py your_file.asm --create-script
```

### 3. Troubleshoot Linker Issues
```bash
# Interactive linker troubleshooting
python linker_troubleshooter.py

# Analyze specific assembly file
python analyze_assembly_oop.py your_file.asm
```

## 🛠️ Features

### Build System
- **Cross-platform support**: Windows, Linux, macOS
- **Automatic tool detection**: NASM, GCC, LD
- **Multiple architectures**: x86, x64, ARM
- **Error handling**: Comprehensive error reporting
- **Cleanup**: Automatic temporary file cleanup

### OOP Analysis
- **Object detection**: Finds data structures that act like objects
- **Method analysis**: Identifies function patterns
- **Inheritance patterns**: Detects base class relationships
- **Polymorphism**: Finds jump tables and virtual function patterns
- **Encapsulation**: Identifies private data sections

### Linker Troubleshooting
- **Error diagnosis**: Identifies common linker issues
- **Environment checking**: Verifies tool availability
- **Fix suggestions**: Provides specific solutions
- **Build script generation**: Creates platform-specific scripts

## 📋 Supported Platforms

| Platform | Assembler | Linker | Object Format | Executable |
|----------|-----------|--------|---------------|------------|
| Windows  | NASM      | GCC    | win64/win32   | .exe       |
| Linux    | NASM      | GCC    | elf64/elf32   | (no ext)   |
| macOS    | NASM      | GCC    | macho64/macho32 | (no ext) |
| Android  | NASM      | GCC    | elf64         | (no ext)   |
| iOS      | NASM      | GCC    | macho64       | (no ext)   |

## 🔍 OOP Pattern Detection

### Object Structures
```assembly
; Object-like data structure
myObject resq 1    ; object pointer
myData   resd 4    ; object data
```

### Method Dispatch
```assembly
; Function pointer table
methodTable dq method1, method2, method3

; Jump table dispatch
jmp [methodTable + rax*8]
```

### Inheritance Patterns
```assembly
; Base class structure
baseClass:
    .vtable dq virtualMethod1, virtualMethod2
    .data   resd 4

; Derived class
derivedClass:
    .vtable dq overriddenMethod1, virtualMethod2
    .data   resd 4
```

### Polymorphism
```assembly
; Virtual function call
mov rax, [object + 0]    ; Get vtable
call [rax + method_offset*8]  ; Call virtual method
```

## 🚨 Common Linker Issues & Solutions

### 1. Undefined Reference
**Error**: `undefined reference to 'symbol'`
**Solutions**:
- Add missing object file to link command
- Check symbol name spelling
- Add required library with `-l` flag
- Verify all source files are compiled

### 2. Multiple Definition
**Error**: `multiple definition of 'symbol'`
**Solutions**:
- Use `extern` declaration in header files
- Make functions `static` if only used in one file
- Use include guards in header files
- Remove duplicate definitions

### 3. Relocation Truncated
**Error**: `relocation truncated to fit`
**Solutions**:
- Use 64-bit addressing mode
- Move large data to appropriate section
- Use RIP-relative addressing
- Check memory model settings

### 4. Cannot Find Entry Point
**Error**: `cannot find entry point`
**Solutions**:
- Define and export main function
- Specify correct entry point with `-e` flag
- Use correct calling convention
- Check function signature

## 📝 Example Usage

### Building a Simple Assembly Program
```bash
# Create a simple assembly file
echo 'BITS 64
global main
section .text
main:
    mov rax, 60
    mov rdi, 0
    syscall' > hello.asm

# Build it
python assembly_build_system.py hello.asm

# Analyze OOP patterns
python assembly_build_system.py hello.asm --analyze-oop
```

### Cross-Platform Build Script
```bash
# Generate build script
python assembly_build_system.py hello.asm --create-script

# The generated script will work on Windows, Linux, and macOS
chmod +x build_hello.sh
./build_hello.sh
```

### Troubleshooting Linker Issues
```bash
# Interactive troubleshooting
python linker_troubleshooter.py

# Enter error message when prompted
# Get specific solutions for your platform
```

## 🔧 Advanced Usage

### Custom Build Configuration
```python
from assembly_build_system import AssemblyBuildSystem

build_system = AssemblyBuildSystem()

# Build for specific target
result = build_system.build_assembly(
    "my_program.asm",
    output_name="my_program",
    target_os="windows",
    target_arch="amd64"
)

# Analyze OOP structure
analysis = build_system.analyze_oop_structure("my_program.asm")
report = build_system.generate_report(analysis)
print(report)
```

### Custom OOP Analysis
```python
from analyze_assembly_oop import AssemblyOOPAnalyzer

with open("my_program.asm", 'r') as f:
    code = f.read()

analyzer = AssemblyOOPAnalyzer(code)
analysis = analyzer.analyze()
report = analyzer.generate_report()
print(report)
```

## 🐛 Debugging Tips

### 1. Check Environment
```bash
# Verify tools are available
python linker_troubleshooter.py
# This will show available assemblers and linkers
```

### 2. Verbose Build
```bash
# Add verbose output to see exact commands
python assembly_build_system.py your_file.asm --verbose
```

### 3. Platform-Specific Issues
- **Windows**: Ensure NASM and MinGW-w64 are in PATH
- **Linux**: Install NASM and GCC development tools
- **macOS**: Install NASM via Homebrew, ensure Xcode tools are installed

## 📚 Additional Resources

- [NASM Documentation](https://www.nasm.us/docs.php)
- [GCC Linker Options](https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html)
- [Assembly OOP Patterns](https://en.wikipedia.org/wiki/Object-oriented_programming)
- [Cross-Platform Assembly](https://en.wikipedia.org/wiki/Assembly_language)

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Submit a pull request

## 📄 License

MIT License - see LICENSE file for details.

---

**Assembly Build System** - Build, analyze, and troubleshoot pure assembly code with ease!
