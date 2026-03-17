@echo off
REM =====================================================================
REM Build Script for Pure MASM x64 Runtime
REM =====================================================================
REM Builds all MASM components into a standalone library and test executable
REM
REM Requirements:
REM  - MSVC 2022 with ML64.exe (MASM assembler)
REM  - Visual Studio Build Tools or Community Edition
REM
REM Usage: build_masm_runtime.bat [Release|Debug]
REM =====================================================================

setlocal enabledelayedexpansion

REM Configuration
set BUILD_MODE=%1
if "%BUILD_MODE%"=="" set BUILD_MODE=Release

set MASM_DIR=%CD%\src\masm
set BUILD_DIR=%CD%\build\masm
set BIN_DIR=%BUILD_DIR%\bin\%BUILD_MODE%
set LIB_DIR=%BUILD_DIR%\lib\%BUILD_MODE%
set OBJ_DIR=%BUILD_DIR%\obj\%BUILD_MODE%

REM Ensure output directories exist
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"
if not exist "%OBJ_DIR%" mkdir "%OBJ_DIR%"

echo.
echo ============================================================
echo Building Pure MASM x64 Runtime - %BUILD_MODE%
echo ============================================================
echo Source Dir: %MASM_DIR%
echo Build Dir:  %BUILD_DIR%
echo.

REM Find ML64.exe (MASM assembler)
set ML64_PATH=
for /f "delims=" %%A in ('where ml64.exe 2^>nul') do (
    set ML64_PATH=%%A
    goto found_ml64
)

echo ERROR: ml64.exe not found. Install Visual Studio Build Tools.
exit /b 1

:found_ml64
echo [OK] Found ML64: %ML64_PATH%
echo.

REM Compile individual MASM modules
echo ============================================================
echo [COMPILE] MASM Modules
echo ============================================================

set MASM_FILES=asm_memory.asm asm_sync.asm asm_string.asm asm_events.asm asm_hotpatch_integration.asm

for %%F in (%MASM_FILES%) do (
    echo [ASM] %%F
    "%ML64_PATH%" /c /Cp /Zf /Fo "%OBJ_DIR%\%%~nF.obj" "%MASM_DIR%\%%F"
    if errorlevel 1 (
        echo ERROR: Failed to compile %%F
        exit /b 1
    )
)

echo.
echo ============================================================
echo [LIB] Creating Static Library
echo ============================================================

REM Create static library from object files
set OBJS=
for %%F in (%MASM_FILES%) do (
    set OBJS=!OBJS! "%OBJ_DIR%\%%~nF.obj"
)

REM Use lib.exe to create static library
lib.exe /OUT:"%LIB_DIR%\masm_runtime.lib" %OBJS%
if errorlevel 1 (
    echo ERROR: Failed to create library
    exit /b 1
)

echo [OK] Created: %LIB_DIR%\masm_runtime.lib
echo.

REM Compile test harness
echo ============================================================
echo [COMPILE] Test Harness
echo ============================================================

"%ML64_PATH%" /c /Cp /Zf /Fo "%OBJ_DIR%\asm_test_main.obj" "%MASM_DIR%\asm_test_main.asm"
if errorlevel 1 (
    echo ERROR: Failed to compile test harness
    exit /b 1
)

echo.
echo ============================================================
echo [LINK] Creating Executable
echo ============================================================

REM Link test executable
link.exe /OUT:"%BIN_DIR%\RawrXD-MasmTest.exe" ^
         /SUBSYSTEM:CONSOLE ^
         /ENTRY:main ^
         "%OBJ_DIR%\asm_test_main.obj" ^
         "%LIB_DIR%\masm_runtime.lib" ^
         kernel32.lib

if errorlevel 1 (
    echo ERROR: Failed to link executable
    exit /b 1
)

echo [OK] Created: %BIN_DIR%\RawrXD-MasmTest.exe
echo.

REM Display summary
echo ============================================================
echo [SUMMARY]
echo ============================================================
echo Build Mode:     %BUILD_MODE%
echo Library:        %LIB_DIR%\masm_runtime.lib
echo Test Exe:       %BIN_DIR%\RawrXD-MasmTest.exe
echo Object Files:   %OBJ_DIR%\
echo.
echo Build Complete!
echo.
echo To run tests:
echo   %BIN_DIR%\RawrXD-MasmTest.exe
echo.

endlocal
exit /b 0
