@echo off
REM CODEX ULTIMATE v7.0 Build Script for VS2022
REM Professional PE Reverse Engineering Tool

setlocal enabledelayedexpansion

cd /d "D:\lazy init ide"

REM Use VS2022 MASM64
set ML64_PATH="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set LINK_PATH="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"

if not exist %ML64_PATH% (
    echo ERROR: MASM64 not found at %ML64_PATH%
    echo Trying alternate location...
    set ML64_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
    set LINK_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
)

if not exist %ML64_PATH% (
    echo ERROR: MASM64 not found. Please install Visual Studio 2022 with C++ build tools.
    exit /b 1
)

echo =====================================
echo CODEX ULTIMATE v7.0 Build System
echo Using VS2022 MASM64
echo =====================================
echo.
echo Step 1: Assembling CodexUltimate.asm...
echo.

REM Assemble the object file
%ML64_PATH% /c /nologo CodexUltimate.asm
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
%LINK_PATH% /SUBSYSTEM:CONSOLE /OPT:NOWIN98 CodexUltimate.obj
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
