@echo off
setlocal enabledelayedexpansion

REM Build RawrXD Titan Kernel DLL
echo [BUILD] RawrXD Titan Kernel Assembly...

cd /d D:\RawrXD\Ship

REM Assemble
echo [ASM] Assembling RawrXD_Titan_Kernel.asm
ml64 /c /Zi /D"PRODUCTION=1" RawrXD_Titan_Kernel.asm 2>&1
if errorlevel 1 (
    echo [ERROR] Assembly failed
    exit /b 1
)

echo [ASM] Successfully created RawrXD_Titan_Kernel.obj

REM Link as DLL
echo [LINK] Linking to DLL...
link /DLL /OUT:RawrXD_Titan_Kernel.dll ^
    /SUBSYSTEM:WINDOWS /ENTRY:DllMain /MACHINE:X64 ^
    /NODEFAULTLIB ^
    RawrXD_Titan_Kernel.obj ^
    kernel32.lib ntdll.lib user32.lib ^
    msvcrt.lib libcmt.lib 2>&1

if errorlevel 1 (
    echo [ERROR] Linking failed
    exit /b 1
)

echo [BUILD] SUCCESS: RawrXD_Titan_Kernel.dll created
dir /B RawrXD_Titan_Kernel.dll RawrXD_Titan_Kernel.lib

endlocal
