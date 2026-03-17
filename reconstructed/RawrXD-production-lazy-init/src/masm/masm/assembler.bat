@echo off
REM RawrXD MASM Assembler - Batch Wrapper
REM Usage: masm_assembler.bat input.asm output.exe

setlocal enabledelayedexpansion

set INPUT=%1
set OUTPUT=%2

if "!INPUT!"=="" (
    echo RawrXD MASM Assembler
    echo Usage: masm_assembler.bat input.asm output.exe
    exit /b 1
)

if not exist "!INPUT!" (
    echo Error: Input file not found: !INPUT!
    exit /b 1
)

echo.
echo [MASM] RawrXD MASM Assembler
echo [INFO] Input:  !INPUT!
echo [INFO] Output: !OUTPUT!
echo.

REM Run PowerShell script with explicit parameters
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$script:PSScriptRoot = '$(cd)'; " ^
    "$Input = '!INPUT!'; " ^
    "$Output = '!OUTPUT!'; " ^
    ". '$(pwd)\masm_assembler.ps1' " ^
    "-Input '$Input' -Output '$Output'"

if !errorlevel! neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)

echo.
echo [SUCCESS] Build complete
exit /b 0
