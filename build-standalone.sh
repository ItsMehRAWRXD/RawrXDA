#!/bin/bash
# RawrXD Standalone Web Bridge Build Script
# Qt-free version that bypasses browser TCP limitations

set -e

echo "========================================="
echo " RawrXD Standalone Web Bridge Builder"
echo "========================================="

# Configuration
BUILD_DIR="build-standalone"
CMAKE_FILE="CMakeLists-Standalone.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check prerequisites
check_prerequisites() {
    print_status "Checking prerequisites..."

    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found. Please install CMake 3.16 or later."
        exit 1
    fi

    if ! command -v make &> /dev/null && ! command -v ninja &> /dev/null && [[ "$OSTYPE" == "msys" ]]; then
        print_error "Neither make nor ninja found. Please install a build system."
        exit 1
    fi

    print_status "Prerequisites check passed."
}

# Create build directory
setup_build_dir() {
    print_status "Setting up build directory..."

    if [ -d "$BUILD_DIR" ]; then
        print_warning "Build directory '$BUILD_DIR' already exists. Cleaning..."
        rm -rf "$BUILD_DIR"
    fi

    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    print_status "Build directory ready."
}

# Configure with CMake
configure_build() {
    print_status "Configuring build with CMake..."

    if [ ! -f "../$CMAKE_FILE" ]; then
        print_error "CMake file '$CMAKE_FILE' not found in parent directory."
        exit 1
    fi

    cmake -C "../$CMAKE_FILE" ..

    print_status "CMake configuration complete."
}

# Build the project
build_project() {
    print_status "Building project..."

    if [[ "$OSTYPE" == "msys" ]]; then
        # Windows with MSVC
        cmake --build . --config Release
    else
        # Unix-like systems
        make -j$(nproc)
    fi

    print_status "Build complete."
}

# Copy web files
copy_web_files() {
    print_status "Copying web interface files..."

    mkdir -p web
    cp ../standalone_interface.html web/

    print_status "Web files copied."
}

# Main build function
main() {
    echo "Starting RawrXD Standalone Web Bridge build..."
    echo

    check_prerequisites
    setup_build_dir
    configure_build
    build_project
    copy_web_files

    echo
    print_status "Build successful! 🎉"
    echo
    echo "To run the server:"
    echo "  cd $BUILD_DIR"
    echo "  ./rawrxd-standalone"
    echo
    echo "Then open http://localhost:8080 in your browser"
    echo
    echo "========================================="
}

# Run main function
main "$@"