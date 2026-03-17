# MASTER RULESET.md

## Title and Scope
Master Rule Set for Systems Engineering Agent  
Comprehensive guidance for building advanced software systems: kernels, IDEs, compilers, debuggers, and hardware-level software.

## Context and Prerequisites
- Target users: Advanced engineers familiar with OS internals, compiler theory, and UI/tooling frameworks.
- Language focus: C, C++, Rust, Assembly.
- Tools: QEMU, Valgrind, Clang Static Analyzer, CMake, Cargo, etc.

## Implementation Steps

### 1. Kernel Development
- Bootloader and memory management setup (include linker scripts, cross-compiling instructions).
- Implement scheduler, syscall interface, interrupt handlers.
- Device driver architecture and virtual memory design.
- Kernel module loading/unloading.
- **Example:**  
    ````c
    // Kernel syscall stub in C
    int syscall(int number, ...) { /* full implementation with error handling */ }
    ````
- Test with QEMU and hardware.

### 2. IDE Construction
- UI architecture (Electron, Qt, Tauri, native).
- Plugin system, LSP, syntax highlighting.
- Integrated debugging, version control, build systems.
- Terminal/API integration.
- **Example:**  
    ````js
    // Electron plugin loader
    module.exports = function(plugin) { /* load and sandbox plugin */ }
    ````

### 3. Compiler Creation
- Implement lexer, parser, AST, IR, codegen, optimization passes.
- Backends for LLVM/custom VM.
- Type checking, semantic analysis.
- Incremental compilation/caching.
- **Example:**  
    ````rust
    // Rust lexer example
    pub fn tokenize(input: &str) -> Vec<Token> { /* validated, error-handling logic */ }
    ````

### 4. Testing and Documentation
- Unit/integration/fuzz testing (especially for kernels).
- API documentation via Markdown/Sphinx.
- Developer onboarding guide.
- Performance benchmarks.
- **Example:**  
    ````python
    # Python fuzzing for syscalls
    def test_syscall_fuzz():
        for args in random_args():
            assert syscall(*args) is not None
    ````

### 5. Security and Performance
- Input validation, bounds checking everywhere.
- Sandbox/privilege separation for IDE plugins.
- Profile with perf, Valgrind, gprof.
- Use static analysis tools.
- Warn against unsafe memory operations.

## Quality Gates (ALL REQUIRED)
- [ ] Zero compiler warnings (-Wall -Wextra -Werror)
- [ ] ASan/UBSan/MSan clean on Clang & GCC
- [ ] 100% line coverage on modules < 1kLOC
- [ ] Boot/QEMU logs show no panics/faults
- [ ] Valgrind/DrMemory clean for userspace
- [ ] Fuzz target: 0 crashes after 1B iterations
- [ ] Markdown docs updated (copy-paste ready)
- [ ] CI green on x86-64, AArch64, RISC-V

## Architecture Overview
- Modular, plugin-enabled, event-driven/message-passing where possible.
- Resources are managed/cleaned up rigorously.
- Separation of concerns in codebases.

## Language-Specific Rules

### Kernel (C + Assembly)
```c
// Boot sequence: boot.asm → kernel.c → userspace
// Memory: Virtual memory, page allocator, heap
// Scheduling: Preemptive, priority-based
// I/O: Interrupt-driven, DMA where possible
// Security: SMEP/SMAP, stack canaries, KASLR
```

### IDE (Rust/C++/Native)
```rust
// UI: Native (Qt/GTK) or Tauri for cross-platform
// Editor: Syntax highlighting, LSP integration
// Build: Integrated compiler toolchains
// Debug: GDB/LLDB integration, breakpoints
// VCS: Git operations, diff viewer
```

### Compiler (C++/LLVM)
```cpp
// Frontend: Lexer → Parser → AST → Semantic analysis
// Middle: IR generation, optimization passes
// Backend: Code generation, register allocation
// Runtime: GC, exception handling, FFI
// Tools: REPL, debugger integration
```

## Performance Requirements
- **Startup**: < 100ms for IDEs, < 1ms for kernels
- **Memory**: < 64MB typical usage
- **Latency**: < 16ms UI response, < 1μs syscalls
- **Throughput**: > 1GB/s I/O, > 10M ops/sec
- **Size**: < 10MB executables, < 1MB kernels

## Security Standards
- **Memory safety**: Bounds checking, stack protection
- **Privilege separation**: User/kernel boundaries
- **Input validation**: All external data sanitized
- **Crypto**: Constant-time implementations
- **Fuzzing**: AFL/libFuzzer integration

## Build System Templates

### CMake (C/C++)
```cmake
cmake_minimum_required(VERSION 3.25)
project(system-software LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra -Werror -O2)
add_link_options(-Wl,-z,noexecstack -Wl,-z,relro)
```

### Cargo (Rust)
```toml
[package]
name = "system-tool"
edition = "2021"
[dependencies]
# Minimal dependencies for systems software
```

### Make (Assembly/C)
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -static
LDFLAGS = -nostdlib -Wl,--gc-sections
```

## Testing Strategy and Examples
- Use automated CI for all builds.
- Cover every public API with unit tests.
- Add integration and fuzzing tests for kernel/driver code.

## Performance Considerations
- Profile code, optimize hot spots.
- Use efficient concurrency primitives and memory allocators.

## Security Guidelines
- Strict bounds/integrity checking.
- Run static analysis regularly.
- Harden system interfaces.

## Related Rules and References
- See: [Kernel Development Guide](kernel-classified.md)
- See: [IDE Design Guide](ide-classified.md)
- See: [Compiler Construction Guide](compiler-classified.md)
- See: [Testing Strategies](testing-classified.md)
- See: [Security Checklist](security-classified.md)

---
**Advanced Topics**
- Memory pools, custom allocators, reference counting, RAII.
- Concurrency primitives, async/await, lock-free structures.
- Hardware interaction: MMIO, DMA, interrupts, bus protocols, driver interfaces.

---
**Notes:**  
All code must be production-ready, fully error-handled, no placeholders. Documentation is mandatory.

