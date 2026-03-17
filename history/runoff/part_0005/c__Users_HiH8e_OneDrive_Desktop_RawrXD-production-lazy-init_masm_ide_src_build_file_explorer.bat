@echo off
REM ============================================================================
REM RawrXD Agentic IDE - Enhanced File Explorer Build Script
REM ============================================================================

echo Building Enhanced File Explorer...
echo.

REM Set MASM paths
set MASM_PATH=C:\masm32
set INCLUDE_PATH=%MASM_PATH%\include
set LIB_PATH=%MASM_PATH%\lib

REM Assemble the enhanced file explorer
echo Assembling file_explorer_enhanced_complete.asm...
\MASM32\BIN\ML.EXE /c /coff /I"%INCLUDE_PATH%" file_explorer_enhanced_complete.asm
if errorlevel 1 (
    echo.
    echo *** Assembly failed ***
    pause
    exit /b 1
)

echo.
echo Enhanced File Explorer assembly successful!
echo.

REM Copy object file to build directory if it exists
if exist "..\build\" (
    echo Copying object file to build directory...
    copy file_explorer_enhanced_complete.obj ..\build\
)

echo Build complete!
pause