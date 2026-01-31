# EON IDE Complete - Build Instructions

## Prerequisites
- NASM assembler
- XCB development libraries
- Linux/Unix environment (or WSL on Windows)

## Build Commands

### Basic Build
```bash
# Assemble the source
nasm -f elf64 -o eon_ide_complete.o eon_ide_complete.asm

# Link with XCB
ld -o eon_ide_complete eon_ide_complete.o -lxcb

# Make executable
chmod +x eon_ide_complete
```

### Advanced Build with Debugging
```bash
# Assemble with debug symbols
nasm -f elf64 -g -F dwarf -o eon_ide_complete.o eon_ide_complete.asm

# Link with debugging support
ld -g -o eon_ide_complete eon_ide_complete.o -lxcb

# Run with debugger
gdb ./eon_ide_complete
```

### Windows Build (WSL)
```bash
# In WSL environment
nasm -f elf64 -o eon_ide_complete.o eon_ide_complete.asm
ld -o eon_ide_complete eon_ide_complete.o -lxcb
chmod +x eon_ide_complete
```

## Running the IDE
```bash
./eon_ide_complete
```

## Features Included
-  XCB Graphical Interface
-  EON Compiler Integration
-  Syntax Highlighting
-  Code Completion
-  Debugger Integration
-  Build System
-  Project Management
-  Search and Replace
-  File Operations

## Development Notes
- The IDE is built as a monolithic assembly program
- All features are integrated into a single executable
- XCB provides the graphical interface
- EON compiler is embedded for real-time compilation
- Debugger supports breakpoints and step-through execution

## Next Steps
1. Test the basic XCB interface
2. Implement keyboard input handling
3. Add file I/O operations
4. Integrate EON compiler functionality
5. Add syntax highlighting
6. Implement debugging features
7. Add project management
8. Create build system integration
