@echo off
echo ===============================================
echo Native IDE - Clean ASCII Build Script
echo ===============================================

REM Set portable toolchain path
set MINGW_PATH=C:\mingw64\bin
set PATH=%MINGW_PATH%;%PATH%

REM Check if GCC is available
gcc --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: GCC not found at %MINGW_PATH%
    echo Please check MinGW-w64 installation
    pause
    exit /b 1
)

echo [OK] GCC found

REM Clean and create directories
if exist build rmdir /s /q build
if exist dist rmdir /s /q dist
mkdir build
mkdir dist

REM Create missing source files
echo Creating missing source files...
if not exist src\file_manager.cpp (
    echo // Placeholder > src\file_manager.cpp
    echo #include "main_window.h" >> src\file_manager.cpp
)

REM Simple direct compilation (skip CMake for now)
echo.
echo Compiling Native IDE directly...
cd src
gcc -c *.cpp -I../include -std=c++17 -static
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Compilation failed
    cd ..
    pause
    exit /b 1
)

gcc *.o -o ../build/NativeIDE.exe -static -luser32 -lgdi32 -lkernel32 -lcomctl32
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Linking failed
    cd ..
    pause
    exit /b 1
)

cd ..
echo [OK] Build successful

REM Create distribution
copy build\NativeIDE.exe dist\
mkdir dist\toolchain
mkdir dist\templates
mkdir dist\plugins

echo.
echo [SUCCESS] Native IDE built successfully
echo Executable: dist\NativeIDE.exe
pause