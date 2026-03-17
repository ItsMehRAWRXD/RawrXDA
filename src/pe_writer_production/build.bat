@echo off
REM PE Writer Production Build Script
REM Supports Windows, Linux, and macOS builds

echo === PE Writer Production Build Script ===

REM Check for CMake
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found. Please install CMake 3.16 or later.
    exit /b 1
)

REM Create build directory
if not exist build mkdir build
cd build

REM Configure build
echo Configuring build...
cmake .. -DBUILD_TESTS=ON -DBUILD_IDE_INTEGRATION=ON
if errorlevel 1 (
    echo ERROR: CMake configuration failed.
    cd ..
    exit /b 1
)

REM Build project
echo Building project...
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed.
    cd ..
    exit /b 1
)

REM Run tests
echo Running tests...
ctest --output-on-failure
if errorlevel 1 (
    echo WARNING: Some tests failed.
)

REM Install (optional)
echo Installing...
cmake --install . --prefix ../install
if errorlevel 1 (
    echo WARNING: Installation failed.
)

cd ..
echo.
echo === Build Complete ===
echo.
echo Build artifacts in: build/
echo Install location: install/
echo.
echo To run tests manually: cd build && ctest
echo To build examples: cd build && cmake --build . --target examples