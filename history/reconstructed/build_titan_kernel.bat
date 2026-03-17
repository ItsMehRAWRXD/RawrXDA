@echo off
setlocal enabledelayedexpansion

:: ==============================================================================
:: RawrXD Titan Kernel Build Script
:: MASM64 -> DLL
:: ==============================================================================

set MASM_PATH=C:\masm32\bin
set INCLUDE_PATH=C:\masm32\include64
set LIB_PATH=C:\masm32\lib64

if not exist "D:\RawrXD\bin" mkdir "D:\RawrXD\bin"

echo [1/3] Assembling RawrXD_Titan_Kernel.asm...
%MASM_PATH%\ml64.exe /c /Cx /D_WIN64 /Zi /Zd /nologo /Fo "D:\RawrXD\bin\RawrXD_Titan_Kernel.obj" /I "%INCLUDE_PATH%" "D:\RawrXD\src\RawrXD_Titan_Kernel.asm"
if %errorlevel% neq 0 (
    echo [ERROR] Assembly failed with code %errorlevel%
    exit /b %errorlevel%
)

echo [2/3] Linking RawrXD_Titan_Kernel.dll...
%MASM_PATH%\link.exe /DLL /SUBSYSTEM:WINDOWS /MACHINE:X64 /LIBPATH:"%LIB_PATH%" /OUT:"D:\RawrXD\bin\RawrXD_Titan_Kernel.dll" /ENTRY:DllMain /NOLOGO /DEBUG /PDB:"D:\RawrXD\bin\RawrXD_Titan_Kernel.pdb" kernel32.lib user32.lib ntdll.lib msvcrt.lib "D:\RawrXD\bin\RawrXD_Titan_Kernel.obj"
if %errorlevel% neq 0 (
    echo [ERROR] Linking failed with code %errorlevel%
    exit /b %errorlevel%
)

echo [3/3] Build Complete: D:\RawrXD\bin\RawrXD_Titan_Kernel.dll
echo [INFO] Ready for persistent resident inference tests.
endlocal
