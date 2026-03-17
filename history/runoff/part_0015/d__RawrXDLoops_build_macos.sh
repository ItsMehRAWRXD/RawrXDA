#!/bin/bash
# build_macos.sh - Underground King DAW for macOS
# Compiles omega_unix.asm to native Mach-O executable (x86-64 + Apple Silicon)
# Requires: clang (Xcode Command Line Tools) + CoreAudio framework

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_macos"
OUTPUT="${BUILD_DIR}/king"

echo "🏁 Underground King DAW – macOS Build"
echo "======================================"

# Create build directory
mkdir -p "$BUILD_DIR"

# Detect architecture
ARCH=$(uname -m)
case "$ARCH" in
    x86_64)
        echo "✓ Architecture: Intel x86-64"
        CLANG_ARCH="x86_64"
        ;;
    arm64)
        echo "✓ Architecture: Apple Silicon (ARM64)"
        CLANG_ARCH="arm64"
        ;;
    *)
        echo "✗ Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# Detect macOS version
MACOS_VERSION=$(sw_vers -productVersion)
echo "✓ macOS version: $MACOS_VERSION"

# Check for clang (Xcode Command Line Tools)
if ! command -v clang >/dev/null 2>&1; then
    echo "✗ clang not found. Install Xcode Command Line Tools:"
    echo "  xcode-select --install"
    exit 1
fi

CLANG_VERSION=$(clang --version | grep -oP '(?<=version )\d+\.\d+')
echo "✓ clang version: $CLANG_VERSION"

# Check for CoreAudio framework
COREAUDIO_PATH="/System/Library/Frameworks/CoreAudio.framework"
if [ -d "$COREAUDIO_PATH" ]; then
    echo "✓ CoreAudio framework found"
else
    echo "⚠ CoreAudio framework not found (may be in different location)"
fi

# Check for Metal framework
METAL_PATH="/System/Library/Frameworks/Metal.framework"
if [ -d "$METAL_PATH" ]; then
    echo "✓ Metal framework found"
else
    echo "⚠ Metal framework not found"
fi

# Compile: omega_unix.asm → Mach-O executable
echo ""
echo "Compiling omega_unix.asm with clang -x assembler..."
echo "  Target: $CLANG_ARCH"
echo "  Command: clang -x assembler -o $OUTPUT $SCRIPT_DIR/omega_unix.asm"
echo "           -Wl,-e,_Omega_Final_Start -nostdlib"
echo "           -framework CoreAudio -framework Metal -framework IOKit"
echo ""

clang \
    -x assembler \
    -o "$OUTPUT" \
    "$SCRIPT_DIR/omega_unix.asm" \
    -Wl,-e,_Omega_Final_Start \
    -nostdlib \
    -framework CoreAudio \
    -framework Metal \
    -framework IOKit \
    -arch "$CLANG_ARCH" \
    -fno-builtin \
    -fno-stack-protector \
    -fno-asynchronous-unwind-tables \
    -mmacosx-version-min=10.15

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ Build successful!"
    echo "  Output: $OUTPUT"
    ls -lh "$OUTPUT"
    
    # Show Mach-O info
    if command -v file >/dev/null 2>&1; then
        echo ""
        echo "Executable type:"
        file "$OUTPUT"
    fi
    
    if command -v otool >/dev/null 2>&1; then
        echo ""
        echo "Mach-O header:"
        otool -h "$OUTPUT" | head -3
    fi
else
    echo "✗ Compilation failed!"
    exit 1
fi

echo ""
echo "🚀 Run with: $OUTPUT --ignite --8k --hybrid"
echo ""
echo "⚠ Note: On Apple Silicon (arm64), may require Rosetta for x86-64 binaries."
echo "   For native arm64 build, modify omega_unix.asm to use arm64 mnemonics."
echo ""
