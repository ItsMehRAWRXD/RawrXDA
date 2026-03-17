# MASTER RULESET.md

## Purpose
Comprehensive ruleset for building foundational software systems: operating systems, IDEs, compilers, debuggers, and low-level tooling with zero-config deployment capabilities.

## Activation Conditions
- Kernel development, IDE creation, compiler design, or systems-level architecture
- C, C++, Rust, Assembly, or low-level system calls  
- Tools that interact with hardware, OS internals, or developer workflows
- Zero-config portable toolchain development

## Rule Format Standards
All rules must include:
- Clear title and scope
- Language-specific guidance
- Architecture overview
- Security and performance considerations
- Documentation and testing strategies
- Zero-config deployment patterns

## Naming Convention
Format: `classified.md`

Examples:
- `native-ide-classified.md`
- `kernel-bootloader-classified.md`
- `compiler-frontend-classified.md`
- `reverse-tool-classified.md`

---

## CORE SYSTEM RULES

### 1. Kernel Development Rules
**Scope**: Bootloader, memory management, scheduler, syscall interface

**Requirements**:
- Define bootloader with multiboot compliance
- Implement memory management (paging, heap, stack)
- Create scheduler with preemptive multitasking
- Design syscall interface with proper privilege separation
- Include inline assembly and linker scripts
- Cross-compilation setup for target architectures
- QEMU or real hardware testing environment
- Interrupt handling and device drivers
- Virtual memory management
- Kernel module loading/unloading mechanisms

**Security**: Ring 0/3 separation, ASLR, stack canaries, input validation
**Performance**: O(1) scheduler, memory pools, lock-free data structures
**Testing**: Unit tests, integration tests, fuzzing, hardware-in-the-loop

### 2. IDE Development Rules
**Scope**: UI architecture, plugin system, developer tools integration

**Requirements**:
- Define UI architecture (Electron, Tauri, Qt, native Win32/Cocoa/GTK)
- Implement plugin system with sandboxed execution
- LSP integration for language servers
- Syntax highlighting engine with custom grammars
- Debugging interface with breakpoints, watches, stack traces
- Version control integration (Git, SVN, Mercurial)
- Build system integration (Make, CMake, Cargo, Maven)
- Code completion and refactoring tools
- Terminal integration and task runners
- Zero-config portable deployment

**Security**: Plugin sandboxing, privilege separation, input sanitization
**Performance**: Async UI, incremental parsing, caching, lazy loading
**Testing**: UI automation, plugin compatibility, performance benchmarks

### 3. Compiler Development Rules
**Scope**: Lexer, parser, AST, IR, codegen, optimization

**Requirements**:
- Implement lexer with tokenization and error recovery
- Create parser (recursive descent, LR, LALR) with AST generation
- Design intermediate representation (SSA, three-address code)
- Implement code generation for multiple backends
- Create optimization passes (constant folding, DCE, inlining)
- Support multiple backends (LLVM, custom VM, native x86/ARM)
- Include REPL or CLI interface
- Type checking and semantic analysis
- Incremental compilation and caching
- Cross-compilation support

**Security**: Input validation, bounds checking, safe memory operations
**Performance**: Incremental compilation, parallel parsing, optimized codegen
**Testing**: Compiler tests, fuzzing, benchmark suites, correctness validation

### 4. Zero-Config Toolchain Rules
**Scope**: Portable, self-contained development environments

**Requirements**:
- Self-extracting executable with embedded compilers
- Auto-detection of system compilers (GCC, Clang, MSVC)
- Universal language support (C, C++, Rust, Go, Python, Zig)
- Static linking by default for portable binaries
- No external dependencies or installation required
- USB/network drive compatibility
- Sandboxed compilation environment
- Real-time build progress and error reporting

**Security**: Compiler sandboxing, temporary file cleanup, path validation
**Performance**: Parallel compilation, compiler caching, optimized linking
**Testing**: Multi-platform compatibility, language interop, stress testing

---

## IMPLEMENTATION STANDARDS

### Code Quality Requirements
- All code must be production-ready with comprehensive error handling
- No placeholder implementations or TODO comments in production code
- Full documentation for all public APIs and interfaces
- Comprehensive test coverage (>90% line coverage minimum)
- Static analysis integration (Clang Static Analyzer, Coverity, PVS-Studio)
- Memory safety verification (Valgrind, AddressSanitizer, MemorySanitizer)

### Architecture Patterns
- Modular design with clear interface boundaries
- Separation of concerns with single responsibility principle
- Plugin architecture with dynamic loading capabilities
- Event-driven or message-passing communication
- Resource management with RAII patterns
- Dependency injection for testability

### Build Systems
- **C/C++**: CMake with cross-compilation support
- **Rust**: Cargo with custom build scripts
- **Assembly**: Make with architecture-specific targets
- **Multi-language**: Custom build orchestration
- **Packaging**: Self-extracting archives, container images
- **CI/CD**: Automated testing on multiple platforms

---

## SECURITY GUIDELINES

### Memory Safety
- Use safe languages (Rust) or safe C++ patterns where possible
- Implement bounds checking for all array accesses
- Use smart pointers and RAII for memory management
- Enable stack canaries, ASLR, DEP/NX bit protection
- Regular fuzzing and static analysis
- Memory leak detection in development builds

### Privilege Separation
- Run components with minimal required privileges
- Sandbox untrusted code execution (plugins, user scripts)
- Validate all external inputs and file formats
- Use secure communication protocols (TLS, signed messages)
- Implement proper authentication and authorization

### Attack Surface Reduction
- Minimize external dependencies
- Disable unused features and protocols
- Regular security audits and penetration testing
- Implement secure coding standards (CERT, OWASP)
- Use compiler security flags (-fstack-protector, -D_FORTIFY_SOURCE)

---

## PERFORMANCE OPTIMIZATION

### Memory Management
- Custom allocators for specific use cases (pool, arena, slab)
- Memory pools to reduce allocation overhead
- Garbage collection strategies for managed components
- Reference counting with cycle detection
- Copy-on-write and move semantics optimization

### Concurrency
- Lock-free data structures where applicable
- Thread synchronization with minimal contention
- Async/await patterns for I/O operations
- Message passing between components
- Work-stealing schedulers for parallel tasks
- SIMD optimization for data processing

### I/O Optimization
- Asynchronous I/O with completion ports/epoll/kqueue
- Memory-mapped files for large data sets
- Batch operations to reduce syscall overhead
- Zero-copy networking where possible
- Efficient serialization formats (protobuf, flatbuffers)

---

## TESTING STRATEGIES

### Unit Testing
- Test-driven development with comprehensive coverage
- Mock objects for external dependencies
- Property-based testing for algorithmic code
- Performance regression testing
- Memory leak and corruption detection

### Integration Testing
- End-to-end workflow testing
- Cross-platform compatibility testing
- Plugin compatibility matrices
- Performance benchmarking under load
- Stress testing with resource constraints

### Hardware Testing
- Real hardware validation for kernel code
- Emulation testing (QEMU, Bochs, VirtualBox)
- Hardware-in-the-loop for device drivers
- Power management testing on mobile platforms
- Thermal testing under sustained load

---

## DOCUMENTATION REQUIREMENTS

### API Documentation
- Complete reference documentation (Doxygen, rustdoc)
- Code examples for all public interfaces
- Architecture decision records (ADRs)
- Performance characteristics documentation
- Security model documentation

### User Documentation
- Installation and setup guides
- Developer onboarding documentation
- Troubleshooting and FAQ sections
- Migration guides for version updates
- Plugin development tutorials

### Internal Documentation
- Design documents and specifications
- Code review guidelines
- Testing procedures and automation
- Release and deployment procedures
- Incident response playbooks

---

## DEPLOYMENT AND DISTRIBUTION

### Zero-Config Deployment
- Self-contained executables with no external dependencies
- Automatic toolchain detection and bootstrapping
- Configuration-free operation out of the box
- Portable across different environments
- Rollback capabilities for failed deployments

### Multi-Platform Support
- Cross-compilation for all target platforms
- Platform-specific optimizations
- Native packaging formats (MSI, DEB, RPM, PKG)
- Container images for cloud deployment
- Hardware abstraction layers

### Update Management
- Incremental updates with delta patches
- Backward compatibility guarantees
- Automatic update checking and installation
- Rollback mechanisms for failed updates
- Security update prioritization

---

## QUALITY ASSURANCE

### Code Review Process
- Mandatory peer review for all changes
- Automated static analysis integration
- Security-focused code review checklist
- Performance impact assessment
- Documentation completeness verification

### Release Criteria
- All tests passing on all supported platforms
- Performance benchmarks within acceptable ranges
- Security audit completion
- Documentation updates completed
- Backward compatibility verification

### Continuous Integration
- Automated building and testing on commit
- Multi-platform testing matrices
- Performance regression detection
- Security vulnerability scanning
- Automated deployment to staging environments

---

## ADVANCED TOPICS

### Hardware Interaction
- Memory-mapped I/O programming patterns
- DMA operations and cache coherency
- Interrupt handling and real-time constraints
- Device driver architecture and interfaces
- Bus protocols (PCI, USB, I2C, SPI) implementation

### System Programming
- Bootloader development and chainloading
- Kernel module development patterns
- System call implementation and optimization
- Virtual memory management algorithms
- Process and thread management

### Compiler Technology
- Advanced optimization techniques
- Just-in-time compilation strategies
- Profile-guided optimization
- Link-time optimization
- Interprocedural analysis

---

## COMPLIANCE AND STANDARDS

### Coding Standards
- Language-specific style guides (Google, LLVM, Rust)
- Static analysis rule enforcement
- Naming convention consistency
- Error handling patterns
- Logging and debugging standards

### Industry Standards
- POSIX compliance for system interfaces
- ISO C/C++ standard adherence
- Platform-specific guidelines (Windows, macOS, Linux)
- Security standards (Common Criteria, FIPS)
- Accessibility guidelines (WCAG, Section 508)

### Open Source Considerations
- License compatibility and compliance
- Contribution guidelines and code of conduct
- Issue tracking and community management
- Documentation for external contributors
- Trademark and patent considerations

---

## NOTES
This master ruleset is designed for advanced systems programmers building foundational software infrastructure. It assumes deep familiarity with:
- Operating system internals and kernel programming
- Compiler theory and implementation techniques  
- UI framework architecture and performance optimization
- Hardware programming and device driver development
- Security engineering and threat modeling
- Performance analysis and optimization techniques

**Last Updated**: October 2025  
**Version**: 1.0  
**Maintainer**: Systems Engineering Team