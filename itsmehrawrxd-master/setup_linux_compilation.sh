#!/bin/bash

# ========================================
# EON Assembly IDE Linux Compilation Setup
# ========================================

echo " Setting up EON Assembly IDE compilation environment..."

# Update package lists
echo " Updating package lists..."
sudo apt update

# Install essential compilation tools
echo " Installing NASM, GCC, and build tools..."
sudo apt install -y nasm gcc build-essential

# Verify installations
echo " Verifying installations..."
echo "NASM version:"
nasm -v

echo "GCC version:"
gcc --version

# Create compilation directory
echo " Setting up compilation directory..."
mkdir -p eon_assembly_build
cd eon_assembly_build

# Copy the EON Assembly IDE source
echo " Copying EON Assembly IDE source..."
cp ../eon_ide_complete_550k.asm .

# Compile the EON Assembly IDE
echo " Compiling EON Assembly IDE..."
echo "Step 1: Assembling with NASM..."
nasm -f elf64 eon_ide_complete_550k.asm -o eon_ide.o

if [ $? -eq 0 ]; then
    echo " Assembly successful!"
    
    echo "Step 2: Linking with GCC..."
    gcc -no-pie -o eon_ide eon_ide.o
    
    if [ $? -eq 0 ]; then
        echo " EON Assembly IDE compilation successful!"
        echo " File sizes:"
        ls -lh eon_ide.o eon_ide
        echo ""
        echo " Ready to run: ./eon_ide"
    else
        echo " Linking failed"
        exit 1
    fi
else
    echo " Assembly failed"
    exit 1
fi

echo ""
echo " EON Assembly IDE compilation complete!"
echo " Files created in: $(pwd)"
echo " To run: ./eon_ide"
