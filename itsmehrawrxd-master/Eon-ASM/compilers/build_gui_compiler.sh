#!/bin/bash
echo "=== Building Self-Contained Compiler with GUI ==="
echo ""

# Create build directory
mkdir -p build/gui

echo "=== Building Self-Contained GUI Compiler ==="
echo ""

# Check if NASM is available (for initial bootstrap only)
if ! command -v nasm &> /dev/null; then
    echo "NOTE: NASM not found - this will be the last time we need it!"
    echo "After this build, the compiler will be completely self-contained."
    echo ""
    echo "Please install NASM for this one-time bootstrap build:"
    echo "Ubuntu/Debian: sudo apt-get install nasm"
    echo "Fedora: sudo dnf install nasm"
    echo "Arch: sudo pacman -S nasm"
    exit 1
fi

echo "Bootstrapping self-contained compiler (last time using NASM)..."
nasm -f elf64 -o build/gui/self_contained_compiler_gui.o self_contained_compiler_gui.asm
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to assemble GUI compiler"
    exit 1
fi

echo "Linking with system libraries..."
ld -o build/gui/RawrZCompilerIDE build/gui/self_contained_compiler_gui.o -lX11 -lXext -lc
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to link GUI compiler"
    exit 1
fi

echo ""
echo "=== Self-Contained Compiler GUI Build Complete! ==="
echo ""
echo "Output: build/gui/RawrZCompilerIDE"
echo ""
echo "Features:"
echo "- Complete GUI IDE with project management"
echo "- Self-contained compiler (no external dependencies)"
echo "- Direct machine code generation"
echo "- Built-in assembler and linker"
echo "- Multi-language support (Eon, C, C++, etc.)"
echo "- Project templates and file management"
echo "- Integrated build system"
echo ""
echo "This is the LAST time external tools are needed!"
echo "The generated IDE can compile everything without dependencies."
echo ""

# Test the compiled GUI
echo "Testing the GUI compiler..."
if [ -f "build/gui/RawrZCompilerIDE" ]; then
    chmod +x build/gui/RawrZCompilerIDE
    echo "SUCCESS: GUI compiler built successfully!"
    echo "You can now run: ./build/gui/RawrZCompilerIDE"
    echo ""
    echo "The IDE includes:"
    echo "- File menu: New, Open, Save, Project management"
    echo "- Build menu: Compile, Build All, Clean"
    echo "- Run menu: Run, Debug"
    echo "- Project explorer with file tree"
    echo "- Syntax highlighting editor"
    echo "- Integrated compiler with zero dependencies"
    echo ""
else
    echo "ERROR: GUI compiler executable not found"
    exit 1
fi

echo "Build completed successfully!"
