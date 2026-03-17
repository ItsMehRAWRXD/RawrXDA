@echo off
cls
echo [+] OMEGA-POLYGLOT v3.0 Codex Reverse...

if not exist "C:\masm32\bin\ml.exe" (
    echo [-] MASM32 not found at C:\masm32
    echo Please install MASM32 first
    exit /b 1
)

echo [+] Assembling omega_crx.asm...
C:\masm32\bin\ml /c /coff /Cp omega_crx.asm
if errorlevel 1 (
    echo [-] Assembly failed
    exit /b 1
)

echo [+] Linking omega_crx.obj...
C:\masm32\bin\link /SUBSYSTEM:CONSOLE omega_crx.obj
if errorlevel 1 (
    echo [-] Linking failed
    exit /b 1
)

echo [+] Ready: omega_crx.exe
echo.
echo Usage: omega_crx.exe
echo [1] Load and analyze file
echo [2] Hex dump
echo [3] Disassemble
echo [4] Convert format
echo [5] Extract sections
echo [6] List imports/exports
echo [7] Show headers
echo [8] Exit
pause