@echo off
setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

cd /d D:\rawrxd

echo === BUILDING RawrXD AMPHIBIOUS COMPLETE ===
ml64 /c /Zi RawrXD_Amphibious_Complete_ASM.asm
if %errorlevel% neq 0 (
    echo COMPILATION FAILED
    exit /b 1
)

link /subsystem:console /entry:main /out:RawrXD_Amphibious_Complete.exe ^
    RawrXD_Amphibious_Complete_ASM.obj ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\kernel32.lib" ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\user32.lib"

if %errorlevel% neq 0 (
    echo LINKING FAILED
    exit /b 1
)

echo === BUILD COMPLETE ===
echo.
echo === EXECUTING AMPHIBIOUS CLI ===
echo.
RawrXD_Amphibious_Complete.exe
echo.
echo === EXECUTION COMPLETE ===
