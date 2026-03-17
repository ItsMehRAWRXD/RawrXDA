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

## Architecture Overview
- Modular, plugin-enabled, event-driven/message-passing where possible.
- Resources are managed/cleaned up rigorously.
- Separation of concerns in codebases.

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