#!/bin/bash
# PE Writer Validation Script
# Tests the complete build and functionality

echo "=== PE Writer Validation Script ==="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print status
print_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $2"
    else
        echo -e "${RED}✗${NC} $2"
    fi
}

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}ERROR: Not in PE Writer root directory${NC}"
    exit 1
fi

# Clean previous build
echo "Cleaning previous build..."
rm -rf build install
print_status $? "Clean completed"

# Run build script
echo "Running build script..."
if [ -f "build.sh" ]; then
    chmod +x build.sh
    ./build.sh
    print_status $? "Build script execution"
else
    echo -e "${YELLOW}WARNING: build.sh not found, running manual build${NC}"
    mkdir -p build && cd build
    cmake .. -DBUILD_TESTS=ON -DBUILD_IDE_INTEGRATION=ON
    print_status $? "CMake configuration"
    cmake --build . --config Release
    print_status $? "CMake build"
    cd ..
fi

# Check if build succeeded
if [ ! -d "build" ]; then
    echo -e "${RED}ERROR: Build directory not created${NC}"
    exit 1
fi

cd build

# Run tests
echo "Running test suite..."
ctest --output-on-failure -V
TEST_RESULT=$?
print_status $TEST_RESULT "Test suite execution"

# Build example
echo "Building example..."
if [ -f "../examples/hello_world.cpp" ]; then
    g++ -std=c++17 -I../include -L. -lpe_writer ../examples/hello_world.cpp -o hello_world_example
    print_status $? "Example compilation"
else
    echo -e "${YELLOW}WARNING: Example source not found${NC}"
fi

# Test PE file creation
echo "Testing PE file creation..."
if [ -f "hello_world_example" ]; then
    ./hello_world_example
    EXAMPLE_RESULT=$?
    print_status $EXAMPLE_RESULT "Example execution"

    if [ -f "hello_world.exe" ]; then
        echo "Checking PE file..."
        # Basic PE validation (check MZ header)
        if head -c 2 hello_world.exe | grep -q "MZ"; then
            print_status 0 "PE file header validation"
        else
            print_status 1 "PE file header validation"
        fi

        # Check file size
        FILE_SIZE=$(stat -c%s hello_world.exe 2>/dev/null || stat -f%z hello_world.exe 2>/dev/null)
        if [ $FILE_SIZE -gt 1000 ]; then
            print_status 0 "PE file size check ($FILE_SIZE bytes)"
        else
            print_status 1 "PE file size check ($FILE_SIZE bytes)"
        fi
    else
        print_status 1 "PE file creation"
    fi
fi

cd ..

# Final summary
echo
echo "=== Validation Summary ==="
if [ $TEST_RESULT -eq 0 ] && [ -f "build/hello_world.exe" ]; then
    echo -e "${GREEN}✓ PE Writer production system is ready!${NC}"
    echo
    echo "Next steps:"
    echo "1. Review the generated PE file: build/hello_world.exe"
    echo "2. Integrate with your IDE using ide_integration/"
    echo "3. Customize configurations in config/"
    echo "4. Run additional tests: cd build && ctest -V"
else
    echo -e "${RED}✗ Validation failed. Check errors above.${NC}"
    exit 1
fi