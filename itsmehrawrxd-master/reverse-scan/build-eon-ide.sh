#!/bin/bash

# ========================================
# EON IDE Build Script - XCB Graphical Frontend
# ========================================
# This script builds the complete EON IDE with XCB graphical interface
# Includes: EON Compiler, IDE Interface, Debugger, and Build System
# ========================================

set -e  # Exit on any error

echo " Building EON IDE with XCB Graphical Frontend"
echo "=============================================="

# Configuration
ASM_FILE="eon_ide_xcb.asm"
OUTPUT_FILE="eon_ide"
OBJECT_FILE="eon_ide.o"
LINK_FLAGS="-lxcb"

# Check dependencies
echo " Checking dependencies..."

# Check for NASM
if ! command -v nasm &> /dev/null; then
    echo " NASM not found. Please install NASM assembler."
    echo "   Ubuntu/Debian: sudo apt-get install nasm"
    echo "   CentOS/RHEL: sudo yum install nasm"
    echo "   macOS: brew install nasm"
    exit 1
fi

# Check for ld (linker)
if ! command -v ld &> /dev/null; then
    echo " Linker (ld) not found. Please install binutils."
    echo "   Ubuntu/Debian: sudo apt-get install binutils"
    echo "   CentOS/RHEL: sudo yum install binutils"
    exit 1
fi

# Check for XCB development libraries
if ! pkg-config --exists xcb; then
    echo " XCB development libraries not found."
    echo "   Ubuntu/Debian: sudo apt-get install libxcb-dev"
    echo "   CentOS/RHEL: sudo yum install libxcb-devel"
    echo "   macOS: brew install libxcb"
    exit 1
fi

echo " All dependencies found"

# Build process
echo ""
echo " Building EON IDE..."

# Step 1: Assemble the source code
echo " Assembling $ASM_FILE..."
nasm -f elf64 -o $OBJECT_FILE $ASM_FILE
if [ $? -eq 0 ]; then
    echo " Assembly successful"
else
    echo " Assembly failed"
    exit 1
fi

# Step 2: Link the object file
echo " Linking $OBJECT_FILE..."
ld -o $OUTPUT_FILE $OBJECT_FILE $LINK_FLAGS
if [ $? -eq 0 ]; then
    echo " Linking successful"
else
    echo " Linking failed"
    exit 1
fi

# Step 3: Make executable
chmod +x $OUTPUT_FILE

# Step 4: Clean up object file
rm -f $OBJECT_FILE

echo ""
echo " EON IDE Build Complete!"
echo "=========================="
echo " Executable: $OUTPUT_FILE"
echo " Size: $(du -h $OUTPUT_FILE | cut -f1)"
echo ""
echo " To run the EON IDE:"
echo "   ./$OUTPUT_FILE"
echo ""
echo " Features included:"
echo "    XCB Graphical Interface"
echo "    8x8 Font Rendering"
echo "    Event Loop (Expose, Key Press)"
echo "    Back Buffer Rendering"
echo "    Ready for EON Compiler Integration"
echo ""
echo " Next steps:"
echo "   1. Run: ./$OUTPUT_FILE"
echo "   2. Integrate EON compiler logic"
echo "   3. Add keyboard input handling"
echo "   4. Implement editor buffer display"
echo "   5. Add syntax highlighting"
echo "   6. Integrate debugger interface"
echo ""
echo " Development tips:"
echo "   - Use 'xev' to test key events"
echo "   - Check XCB documentation for advanced features"
echo "   - Consider adding menu bars and toolbars"
echo "   - Implement file I/O for project management"
