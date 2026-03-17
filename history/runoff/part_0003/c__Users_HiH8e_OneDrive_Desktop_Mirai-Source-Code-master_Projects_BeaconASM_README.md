# 🔥 BeaconASM - Assembly Language IDE & Toolkit

**Phase 3+ Extension Project**  
**Status:** In Development  
**Technologies:** NASM, MASM (ml64), C/C++, x86-64 Assembly

---

## 🎯 PROJECT OVERVIEW

BeaconASM is a professional assembly language development toolkit that combines:
- **NASM** (Netwide Assembler) - Cross-platform assembly
- **MASM** (Microsoft Macro Assembler) - Windows-optimized assembly
- **C/C++ Integration** - Hybrid high-level + low-level code
- **Visual Studio 2022** - Professional development environment

---

## 🏆 PHASE 3 INTEGRATION

This project builds on your exceptional Phase 3 achievements:

### Leverages BotBuilder (622 lines C# WPF):
- GUI patterns and architecture
- Tab-based interface design
- Configuration management

### Leverages DLR (8 architectures verified):
- x86/x64 knowledge
- Windows API integration
- Multi-architecture support

### Leverages Beast Swarm (258KB Python):
- Performance optimization principles
- Memory efficiency techniques
- Error handling patterns

---

## 📁 PROJECT STRUCTURE

```
BeaconASM/
├── README.md                  # This file
├── src/
│   ├── nasm/                  # NASM source files
│   │   ├── hello.asm         # Hello World (NASM syntax)
│   │   ├── shellcode.asm     # Shellcode generator
│   │   └── crypto.asm        # Cryptographic routines
│   ├── masm/                  # MASM source files
│   │   ├── hello.asm         # Hello World (MASM syntax)
│   │   ├── winapi.asm        # Windows API calls
│   │   └── inject.asm        # Code injection primitives
│   ├── cpp/                   # C++ integration
│   │   ├── main.cpp          # Main program
│   │   └── asm_wrapper.h     # Assembly function wrappers
│   └── include/               # Header files
│       └── common.inc        # Common assembly macros
├── build/
│   ├── build_nasm.bat        # NASM build script
│   ├── build_masm.bat        # MASM build script
│   └── build_hybrid.bat      # C++ + ASM hybrid build
├── bin/                       # Output executables
├── docs/
│   ├── NASM_GUIDE.md         # NASM programming guide
│   ├── MASM_GUIDE.md         # MASM programming guide
│   └── EXAMPLES.md           # Code examples
└── tests/
    └── test_asm.cpp          # Unit tests for assembly code
```

---

## 🚀 QUICK START

### 1. NASM Development
```bash
# Compile NASM code (64-bit Windows)
nasm -f win64 src/nasm/hello.asm -o bin/hello.obj
link bin/hello.obj /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:bin/hello.exe
```

### 2. MASM Development
```bash
# Compile MASM code (Visual Studio environment)
ml64 /c src/masm/hello.asm /Fo bin/hello.obj
link bin/hello.obj /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:bin/hello.exe
```

### 3. Hybrid C++ + Assembly
```bash
# Build C++ with inline assembly
cl /c src/cpp/main.cpp /Fo bin/main.obj
ml64 /c src/masm/crypto.asm /Fo bin/crypto.obj
link bin/main.obj bin/crypto.obj /OUT:bin/hybrid.exe
```

---

## 💡 KEY FEATURES

### NASM Features:
- ✅ **Cross-platform** - Works on Windows, Linux, macOS
- ✅ **Intel syntax** - Clear and readable assembly
- ✅ **Macros** - Powerful preprocessing capabilities
- ✅ **Multiple formats** - Supports many object file formats

### MASM Features:
- ✅ **Windows-optimized** - Native Windows API support
- ✅ **64-bit support** - Full x64 assembly (ml64.exe)
- ✅ **VS integration** - Works seamlessly with Visual Studio
- ✅ **Structured** - High-level constructs (PROC, INVOKE, etc.)

### Hybrid Advantages:
- ✅ **Best of both** - C/C++ for structure, ASM for performance
- ✅ **Flexibility** - Choose the right tool for each task
- ✅ **Optimization** - Critical paths in assembly, rest in C++

---

## 🎓 LEARNING RESOURCES

### Assembly Basics:
1. **Registers** - RAX, RBX, RCX, RDX (64-bit) / EAX, EBX, ECX, EDX (32-bit)
2. **Instructions** - MOV, ADD, SUB, JMP, CALL, RET
3. **Calling Conventions** - Windows x64 (RCX, RDX, R8, R9)
4. **System Calls** - Kernel mode vs User mode

### Windows API in Assembly:
- CreateFileA/W
- WriteFile
- VirtualAlloc
- CreateThread
- LoadLibraryA

---

## 🔒 SECURITY APPLICATIONS

### Malware Analysis:
- Shellcode development
- Exploit primitives
- Anti-debugging techniques
- Code obfuscation

### Reverse Engineering:
- Understanding compiled code
- Patch development
- Binary analysis
- Vulnerability research

### Performance Critical:
- Cryptographic algorithms
- Compression routines
- Network protocol parsers
- Memory management

---

## 📊 COMPARISON: NASM vs MASM

| Feature | NASM | MASM |
|---------|------|------|
| **Syntax** | Intel (simple) | Intel (structured) |
| **Platform** | Cross-platform | Windows only |
| **Macros** | Powerful | Very powerful |
| **VS Integration** | Manual | Built-in |
| **Learning Curve** | Easier | Moderate |
| **Windows API** | Manual declarations | Built-in support |
| **64-bit** | nasm -f win64 | ml64.exe |
| **Best For** | Portability | Windows development |

---

## 🛠️ DEVELOPMENT TOOLS

### Required:
- **NASM** - Latest version (already installed)
- **MASM** - Part of Visual Studio 2022 (ml64.exe)
- **Linker** - Microsoft linker (link.exe from VS 2022)
- **Visual Studio 2022** - Full IDE support

### Optional:
- **Visual Studio Code** - Lightweight code editor
- **x64dbg** - Debugger for assembly
- **IDA Free** - Disassembler
- **PE-bear** - PE file analysis

---

## 🎯 PROJECT GOALS

### Phase 1 (Week 1):
- ✅ Project structure setup
- ✅ Hello World in NASM
- ✅ Hello World in MASM
- ✅ Basic build scripts

### Phase 2 (Week 2):
- ⏳ Windows API integration
- ⏳ Shellcode generator
- ⏳ Cryptographic routines
- ⏳ C++ wrapper library

### Phase 3 (Week 3):
- ⏳ GUI IDE (C++ with Qt or WPF)
- ⏳ Syntax highlighting
- ⏳ Integrated debugger
- ⏳ Code templates

### Phase 4 (Week 4):
- ⏳ Advanced features
- ⏳ Plugin system
- ⏳ Documentation
- ⏳ Production release

---

## 📝 CODE EXAMPLES

### NASM (Hello World):
```nasm
section .data
    msg db 'Hello from NASM!', 0xA
    len equ $ - msg

section .text
    global main
    extern printf

main:
    sub rsp, 28h          ; Shadow space
    lea rcx, [msg]        ; First param
    call printf
    add rsp, 28h
    xor rax, rax          ; Return 0
    ret
```

### MASM (Hello World):
```asm
.code
main proc
    sub rsp, 28h
    lea rcx, msg
    call printf
    add rsp, 28h
    xor rax, rax
    ret
main endp

.data
msg byte 'Hello from MASM!', 0Ah, 0

end
```

---

## 🔗 INTEGRATION WITH PHASE 3

### With BotBuilder:
- Shared GUI patterns
- Configuration management
- Build system integration

### With DLR:
- x86/x64 architecture knowledge
- Windows API expertise
- Multi-platform support

### With Beast Swarm:
- Performance optimization
- Memory efficiency
- Error handling

---

## 📚 DOCUMENTATION

- **NASM Manual**: [NASM Documentation](https://www.nasm.us/doc/)
- **MASM Reference**: Microsoft MASM Reference (in VS 2022 docs)
- **x64 Calling Convention**: [Microsoft Docs](https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention)
- **Windows API**: [Windows API Index](https://docs.microsoft.com/en-us/windows/win32/api/)

---

## 🏆 SUCCESS METRICS

- **Code Size** - Minimal executables (KB not MB)
- **Performance** - Direct hardware access
- **Learning** - Deep understanding of computer architecture
- **Security** - Advanced malware analysis capabilities

---

**Created:** November 21, 2025  
**Status:** Active Development  
**Maintainer:** Phase 3 Team  
**License:** Research & Educational Use
