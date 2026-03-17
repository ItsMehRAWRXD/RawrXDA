#!/bin/bash
# build_linux.sh - Underground King DAW for Linux
# Compiles omega_unix.asm to native ELF x86-64 executable
# Requires: clang + llvm-as (LLVM 12+) and libm (glibc)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_linux"
OUTPUT="${BUILD_DIR}/king"

echo "🏁 Underground King DAW – Linux Build"
echo "======================================"

# Create build directory
mkdir -p "$BUILD_DIR"

# Detect architecture
ARCH=$(uname -m)
case "$ARCH" in
    x86_64)
        echo "✓ Architecture: x86-64"
        ;;
    aarch64)
        echo "✓ Architecture: ARM64 (will cross-compile to x86-64)"
        ;;
    *)
        echo "✗ Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# Detect platform variant
if grep -q "WSL" /proc/version 2>/dev/null; then
    echo "⚠ Running in WSL – some real-time features may be limited"
elif [ -f /etc/lsb-release ]; then
    . /etc/lsb-release
    echo "✓ Platform: Ubuntu/Debian $DISTRIB_RELEASE"
elif [ -f /etc/fedora-release ]; then
    echo "✓ Platform: Fedora/RHEL"
elif [ -f /etc/arch-release ]; then
    echo "✓ Platform: Arch Linux"
fi

# Check for required tools
command -v clang >/dev/null 2>&1 || { echo "✗ clang not found. Install: apt-get install clang"; exit 1; }
command -v llvm-as >/dev/null 2>&1 || { echo "✗ llvm-as not found. Install: apt-get install llvm"; exit 1; }

CLANG_VERSION=$(clang --version | grep -oP '(?<=version )\d+\.\d+')
echo "✓ clang version: $CLANG_VERSION"

# Compile: omega_unix.asm → ELF executable
echo ""
echo "Compiling omega_unix.asm with clang -x assembler..."
echo "  Command: clang -x assembler -o $OUTPUT $SCRIPT_DIR/omega_unix.asm -Wl,-e,_Omega_Final_Start -nostdlib -lm"
echo ""

clang \
    -x assembler \
    -o "$OUTPUT" \
    "$SCRIPT_DIR/omega_unix.asm" \
    -Wl,-e,_Omega_Final_Start \
    -nostdlib \
    -lm \
    -march=x86-64 \
    -mtune=generic \
    -fno-builtin \
    -fno-stack-protector \
    -fno-asynchronous-unwind-tables

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    echo "  Output: $OUTPUT"
    ls -lh "$OUTPUT"
    
    # Show ELF info
    if command -v readelf >/dev/null 2>&1; then
        echo ""
        echo "ELF header:"
        readelf -h "$OUTPUT" | head -8
    fi
else
    echo "✗ Compilation failed!"
    exit 1
fi

echo ""
echo "🚀 Run with: $OUTPUT --ignite --8k --hybrid"
echo ""
