@echo off
REM ==============================================================================
REM Build Script for No-Refusal Payload Engine
REM ==============================================================================

setlocal enabledelayedexpansion

set PROJECT_ROOT=D:\NoRefusalPayload
set BUILD_DIR=%PROJECT_ROOT%\build
set CMAKE_GENERATOR="Visual Studio 17 2022"

echo.
echo ========================================
echo No-Refusal Payload Engine Build Script
echo ========================================
echo.

REM Create build directory
if not exist "%BUILD_DIR%" (
    echo [*] Creating build directory...
    mkdir "%BUILD_DIR%"
)

REM Run CMake configuration
echo [*] Running CMake configuration...
cd /d "%BUILD_DIR%"
cmake -G %CMAKE_GENERATOR% -A x64 "%PROJECT_ROOT%"

if %ERRORLEVEL% neq 0 (
    echo [!] CMake configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo [*] Building project (Debug configuration)...
cmake --build . --config Debug

if %ERRORLEVEL% neq 0 (
    echo [!] Build failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo [+] Build completed successfully!
echo ========================================
echo.
echo Output: %BUILD_DIR%\Debug\NoRefusalEngine.exe
echo.

pause
