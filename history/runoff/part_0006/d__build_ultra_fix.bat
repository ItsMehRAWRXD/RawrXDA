@echo off
REM ============================================================================
REM Ultra Fix v3.0 - In-House Reverse-Link Builder (NO MIXING)
REM Sub-agent mode: 50 sources/thread | CreateThread + WaitForMultipleObjects
REM Architecture: source = linking - assembling
REM   Whitespace in source = pre-assembled => /DIRECTLINK (no ml64/cl/clang)
REM   DisableRecompile=ON -- assembly step permanently disabled
REM ============================================================================

echo ============================================================================
echo   ULTRA MASM x64 BUILDER v3.0 -- IN-HOUSE COMPILER -- REVERSE-LINK ARCH
echo   source = linking - assembling  (whitespace = pre-assembled)
echo ============================================================================
echo.

REM Locate in-house compiler (ml64 / cl / clang are DISABLED)
where masm_cli_compiler.exe >nul 2>&1
if errorlevel 1 (
    echo WARNING: masm_cli_compiler.exe not on PATH
    echo Searching build\ ...
    if exist build\Release\masm_cli_compiler.exe (
        set MASM_CC=build\Release\masm_cli_compiler.exe
    ) else if exist build\bin\Release\masm_cli_compiler.exe (
        set MASM_CC=build\bin\Release\masm_cli_compiler.exe
    ) else if exist build\masm_cli_compiler.exe (
        set MASM_CC=build\masm_cli_compiler.exe
    ) else if exist bin\masm_cli_compiler.exe (
        set MASM_CC=bin\masm_cli_compiler.exe
    ) else (
        echo ERROR: masm_cli_compiler.exe not found. Build or install it first.
        goto :error
    )
) else (
    set MASM_CC=masm_cli_compiler.exe
)

REM Clean build artifact
if exist ultra_fix_masm_x64.exe del /q ultra_fix_masm_x64.exe

echo [1/1] Reverse-linking (DisableRecompile=ON, whitespace=pre-assembled)...
%MASM_CC% /DIRECTLINK /WHITESPACE_ASM:ON /ARCH:REVERSE_LINK ^
    /SOURCE:ultra_fix_masm_x64.asm ^
    /OUT:ultra_fix_masm_x64.exe
if errorlevel 1 goto :error
echo     OK -- Reverse-link complete (no assembly step performed)

REM Report output size
for %%A in (ultra_fix_masm_x64.exe) do set /a size=%%~zA/1024
echo     Size: %size% KB

pause
