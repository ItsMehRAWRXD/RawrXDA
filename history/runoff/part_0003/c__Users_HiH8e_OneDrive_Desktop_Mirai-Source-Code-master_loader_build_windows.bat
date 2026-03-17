@echo off
REM Windows build script for loader component (debug version)
REM Usage: build_windows.bat

echo Building loader for Windows (Debug)...

REM Check if GCC is available
gcc --version >nul 2>&1
if errorlevel 1 (
    echo Error: GCC not found. Please install MinGW or MSYS2.
    exit /b 1
)

REM Build with debug flags
REM Note: Electric Fence (-lefence) is Linux-specific, using standard debug build
gcc -g -DDEBUG -static -lpthread -pthread -O3 src/*.c -o loader.dbg

if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo Build successful: loader.dbg