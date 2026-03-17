@echo off
REM OMEGA-POLYGLOT v4.0 PRO Build Script
REM Professional PE Reverse Engineering Toolkit

setlocal enabledelayedexpansion

REM Set working directory
cd /d "D:\lazy init ide"

REM Check MASM32 installation
if not exist "C:\masm32\bin\ml.exe" (
    echo ERROR: MASM32 not found at C:\masm32\bin\ml.exe
    exit /b 1
)

echo =====================================
echo OMEGA-POLYGLOT v4.0 PRO Build System
echo =====================================
echo.
echo Step 1: Assembling omega_pro.asm...
echo.

REM Set environment for MASM32
set PATH=C:\masm32\bin;%PATH%
set INCLUDE=C:\masm32\include
set LIB=C:\masm32\lib

REM Assemble the object file
"C:\masm32\bin\ml.exe" /c /coff /Cp /nologo omega_pro.asm
if errorlevel 1 (
    echo ERROR: Assembly failed
    exit /b 1
)

echo.
echo Step 2: Assembly successful - omega_pro.obj created
echo.
echo Step 3: Linking omega_pro.obj...
echo.

REM Link to executable
"C:\masm32\bin\link.exe" /SUBSYSTEM:CONSOLE /OPT:NOWIN98 omega_pro.obj
if errorlevel 1 (
    echo ERROR: Linking failed
    exit /b 1
)

echo.
echo =====================================
echo BUILD SUCCESSFUL!
echo =====================================
echo.
echo Generated: omega_pro.exe
echo Version: OMEGA-POLYGLOT v4.0 PRO
echo.
echo To run: omega_pro.exe
echo.

dir omega_pro.exe

endlocal
