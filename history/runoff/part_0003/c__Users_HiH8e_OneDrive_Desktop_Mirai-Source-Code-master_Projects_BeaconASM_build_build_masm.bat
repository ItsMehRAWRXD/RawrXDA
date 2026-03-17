@echo off
REM ============================================================================
REM MASM Build Script - Windows x64
REM Requires: Visual Studio 2022 Developer Command Prompt environment
REM ============================================================================

echo.
echo ================================================
echo   MASM Assembly Build System
echo ================================================
echo.

set "PROJECT_ROOT=%~dp0.."
set "SRC_DIR=%PROJECT_ROOT%\src\masm"
set "BIN_DIR=%PROJECT_ROOT%\bin"
set "BUILD_DIR=%PROJECT_ROOT%\build\temp"

REM Create directories
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Check for MASM (ml64)
where ml64 >nul 2>&1
if errorlevel 1 (
    echo ERROR: ml64.exe not found!
    echo Please run this from Visual Studio Developer Command Prompt
    echo Or initialize VS environment with:
    echo   "D:\Microsoft Visual Studio 2022\Common7\Tools\VsDevCmd.bat"
    exit /b 1
)

echo [1/3] Assembling with MASM (ml64)...
ml64 /c "%SRC_DIR%\hello.asm" /Fo "%BUILD_DIR%\hello.obj"
if errorlevel 1 (
    echo ERROR: MASM assembly failed!
    exit /b 1
)

echo [2/3] Linking...
link "%BUILD_DIR%\hello.obj" ^
    /SUBSYSTEM:CONSOLE ^
    /ENTRY:main ^
    /OUT:"%BIN_DIR%\hello_masm.exe" ^
    kernel32.lib
if errorlevel 1 (
    echo ERROR: Linking failed!
    exit /b 1
)

echo [3/3] Build complete!
echo.
echo ✅ Output: %BIN_DIR%\hello_masm.exe
echo.
echo Running executable...
echo ================================================
"%BIN_DIR%\hello_masm.exe"
echo ================================================
echo.

exit /b 0
