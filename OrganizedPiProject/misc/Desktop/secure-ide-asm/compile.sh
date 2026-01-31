#!/bin/bash

# Secure IDE Assembly Compilation Script
# Compiles the secure IDE using NASM for x86-64 Linux

set -e  # Exit on any error

echo "=== Secure IDE Assembly Compilation ==="
echo "Building secure IDE with NASM..."

# Check if NASM is installed
if ! command -v nasm &> /dev/null; then
    echo "Error: NASM is not installed"
    echo "Please install NASM:"
    echo "  Ubuntu/Debian: sudo apt-get install nasm"
    echo "  CentOS/RHEL: sudo yum install nasm"
    echo "  macOS: brew install nasm"
    exit 1
fi

# Check NASM version
echo "NASM version: $(nasm --version | head -n1)"

# Create build directory
mkdir -p build
cd build

echo "Compiling assembly source files..."

# Compile main.asm
echo "  Compiling main.asm..."
nasm -f elf64 -g -F dwarf -o main.o ../main.asm

# Compile ai_engine.asm
echo "  Compiling ai_engine.asm..."
nasm -f elf64 -g -F dwarf -o ai_engine.o ../ai_engine.asm

# Compile security.asm
echo "  Compiling security.asm..."
nasm -f elf64 -g -F dwarf -o security.o ../security.asm

echo "Linking object files..."

# Link object files
ld -m elf_x86_64 -o secure-ide main.o ai_engine.o security.o

echo "Build completed successfully!"
echo "Executable: build/secure-ide"
echo "Size: $(du -h secure-ide | cut -f1)"

# Make executable
chmod +x secure-ide

echo ""
echo "=== Build Information ==="
echo "Target: x86-64 Linux"
echo "Compiler: NASM $(nasm --version | head -n1 | cut -d' ' -f3)"
echo "Linker: GNU ld"
echo "Debug symbols: Yes"
echo "Optimization: None"

echo ""
echo "=== Usage ==="
echo "Run: ./build/secure-ide"
echo "Debug: gdb ./build/secure-ide"

echo ""
echo "=== Security Features ==="
echo "✓ Local AI processing"
echo "✓ Security monitoring"
echo "✓ File access control"
echo "✓ Command validation"
echo "✓ Memory protection"
echo "✓ Audit logging"

echo ""
echo "Build completed successfully!"
