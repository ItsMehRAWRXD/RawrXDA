@echo off
REM =============================================================================
REM Build script for MASM x64 Backend Server
REM =============================================================================
REM Requires: Visual Studio with MASM64 (ml64.exe)
REM =============================================================================

echo.
echo ========================================
echo   RawrXD MASM Backend Build Script
echo ========================================
echo.

REM Try to find Visual Studio environment
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found VS2022 Enterprise
    call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found VS2022 Professional
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found VS2022 Community
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
) else if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    echo Found VS2022 at C:\VS2022Enterprise
    call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
) else (
    echo ERROR: Visual Studio not found!
    echo Please run from a Visual Studio Developer Command Prompt
    pause
    exit /b 1
)

echo.
echo Step 1: Assembling masm_backend.asm...
ml64 /c /nologo masm_backend.asm
if errorlevel 1 (
    echo ERROR: Assembly failed!
    pause
    exit /b 1
)
echo   [OK] Assembly complete

echo.
echo Step 2: Linking masm_backend.obj...
link /nologo /subsystem:console /entry:start masm_backend.obj ws2_32.lib kernel32.lib
if errorlevel 1 (
    echo ERROR: Linking failed!
    pause
    exit /b 1
)
echo   [OK] Linking complete

echo.
echo ========================================
echo   BUILD SUCCESSFUL!
echo ========================================
echo.
echo Output: masm_backend.exe
echo.
echo To run:
echo   masm_backend.exe
echo.
echo Then open in browser:
echo   http://localhost:8080/models
echo.
