@echo off
REM CODEX ULTIMATE v7.0 Build Script
REM Professional PE Reverse Engineering Tool

setlocal enabledelayedexpansion

cd /d "D:\lazy init ide"

REM Check MASM64 installation
if not exist "C:\masm64\bin\ml64.exe" (
    echo ERROR: MASM64 not found at C:\masm64\bin\ml64.exe
    exit /b 1
)

echo =====================================
echo CODEX ULTIMATE v7.0 Build System
echo =====================================
echo.
echo Step 1: Assembling CodexUltimate.asm...
echo.

REM Set environment for MASM64
set PATH=C:\masm64\bin;%PATH%
set INCLUDE=C:\masm64\include64
set LIB=C:\masm64\lib64

REM Assemble the object file
"C:\masm64\bin\ml64.exe" /c /nologo CodexUltimate.asm
if errorlevel 1 (
    echo ERROR: Assembly failed
    exit /b 1
)

echo.
echo Step 2: Assembly successful - CodexUltimate.obj created
echo.
echo Step 3: Linking CodexUltimate.obj...
echo.

REM Link to executable
"C:\masm64\bin\link.exe" /SUBSYSTEM:CONSOLE /OPT:NOWIN98 CodexUltimate.obj
if errorlevel 1 (
    echo ERROR: Linking failed
    exit /b 1
)

echo.
echo =====================================
echo BUILD SUCCESSFUL!
echo =====================================
echo.
echo Generated: CodexUltimate.exe
echo Version: CODEX ULTIMATE v7.0
echo Architecture: x64 (64-bit)
echo.
echo To run: CodexUltimate.exe
echo.

dir CodexUltimate.exe

endlocal
