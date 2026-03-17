@echo off
REM =============================================================================
REM Build Script for RawrXD Compiler Engine (MASM64)
REM Pure Windows x64 Assembly - No Dependencies
REM =============================================================================

setlocal enabledelayedexpansion
cls

echo.
echo ===============================================================================
echo RawrXD Compiler Engine Build System
echo Pure MASM64 Implementation - Qt-Free, Zero Dependencies
echo ===============================================================================
echo.

REM Detect MASM64 installation
set MASM64_PATH=C:\masm64
set ML64_EXE=%MASM64_PATH%\bin\ml64.exe
set LINK_EXE=%MASM64_PATH%\bin\link.exe

if not exist "%ML64_EXE%" (
    echo ERROR: MASM64 not found at %MASM64_PATH%
    echo Please install MASM64 or update MASM64_PATH variable
    pause
    exit /b 1
)

echo [1/5] Checking build environment...
echo MASM64 Path: %MASM64_PATH%
echo ML64: %ML64_EXE%
echo LINK: %LINK_EXE%
echo.

REM Create output directories
if not exist ".\build" mkdir build
if not exist ".\build\obj" mkdir build\obj
if not exist ".\build\bin" mkdir build\bin

echo [2/5] Assembling MASM64 files...
echo.

REM Assemble the compiler engine
echo Assembling rawrxd_compiler_masm64.asm...
"%ML64_EXE%" /c /Zd /Zi /W3 /nologo ^
    /I"%MASM64_PATH%\include64" ^
    /Fo"build\obj\compiler_masm64.obj" ^
    rawrxd_compiler_masm64.asm

if errorlevel 1 (
    echo.
    echo ERROR: Assembly failed!
    echo.
    pause
    exit /b 1
)

echo Assembly successful!
echo.

echo [3/5] Linking executable...
echo.

REM Link the final executable
"%LINK_EXE%" /SUBSYSTEM:CONSOLE /OUT:"build\bin\rawrxd_compiler.exe" ^
    /DEBUG /DEBUGTYPE:CV /INCREMENTAL:NO /NOLOGO ^
    /LIBPATH:"%MASM64_PATH%\lib64" ^
    build\obj\compiler_masm64.obj ^
    kernel32.lib user32.lib gdi32.lib shell32.lib ^
    shlwapi.lib ole32.lib oleaut32.lib psapi.lib ^
    dbghelp.lib wininet.lib urlmon.lib

if errorlevel 1 (
    echo.
    echo ERROR: Linking failed!
    echo.
    pause
    exit /b 1
)

echo Linking successful!
echo.

echo [4/5] Verifying output...
echo.

if exist "build\bin\rawrxd_compiler.exe" (
    echo ✓ Executable created: build\bin\rawrxd_compiler.exe
    
    REM Get file size
    for %%A in ("build\bin\rawrxd_compiler.exe") do (
        set SIZE=%%~zA
        echo ✓ Executable size: !SIZE! bytes
    )
) else (
    echo ERROR: Executable not created
    pause
    exit /b 1
)

echo.

echo [5/5] Build complete!
echo.
echo ===============================================================================
echo BUILD SUCCESSFUL
echo ===============================================================================
echo.
echo Output Files:
echo   - Executable:  build\bin\rawrxd_compiler.exe
echo   - Object File: build\obj\compiler_masm64.obj
echo   - Debug Info:  build\obj\compiler_masm64.obj (PDB embedded)
echo.

REM Optional: Run tests if test harness exists
if exist "build\bin\test_compiler.exe" (
    echo Running test harness...
    echo.
    "build\bin\test_compiler.exe"
    if errorlevel 1 (
        echo Some tests failed!
    )
)

echo.
pause
endlocal
