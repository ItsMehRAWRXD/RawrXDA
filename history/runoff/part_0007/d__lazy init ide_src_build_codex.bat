@echo off
echo [+] Building CODEX ULTIMATE ENGINE v7.0...
echo.

set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set INCLUDES=/I C:\masm64\include64
set LIBS=/I C:\masm64\lib64

echo [+] Assembling 64-bit code...
%ML64% /c /Cp %INCLUDES% omega_pro_v4.asm
if errorlevel 1 goto :error

echo [+] Linking executable...
link /SUBSYSTEM:CONSOLE /ENTRY:main /LARGEADDRESSAWARE omega_pro_v4.obj
if errorlevel 1 goto :error

echo.
echo [+] Build complete: omega_pro_v4.exe
echo.
goto :end

:error
echo.
echo [-] Build failed!
echo.

:end
pause