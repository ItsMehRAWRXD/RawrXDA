@echo off
REM Build script for RawrXD GUI IDE (No Qt required, Win32 API)

cd /d %~dp0
if not exist build mkdir build
cd build

REM Configure and build using cmake
echo [BUILD] Configuring CMake...
cmake -G "Visual Studio 17 2022" -A x64 ..

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

echo [BUILD] Building IDE (GUI)...
cmake --build . --config Release --target RawrXD-IDE

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed
    exit /b 1
)

echo [SUCCESS] Build complete: bin\Release\RawrXD-IDE.exe
