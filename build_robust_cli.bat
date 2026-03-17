@echo off
setlocal enabledelayedexpansion

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

cd /d D:\rawrxd

echo === Assembling RawrXD Amphibious CLI Robust ===
ml64 /c /Zi RawrXD_Amphibious_CLI_Robust.asm
if %errorlevel% neq 0 (
    echo === COMPILATION FAILED ===
    exit /b %errorlevel%
)

echo === Linking ===
link /subsystem:console /entry:main /out:RawrXD_Amphibious_CLI_Robust.exe ^
    RawrXD_Amphibious_CLI_Robust.obj ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\kernel32.lib" ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\user32.lib"

if %errorlevel% neq 0 (
    echo === LINKING FAILED ===
    exit /b %errorlevel%
)

echo === Build Complete ===
echo.
echo === RUNNING EXECUTABLE ===
echo.
RawrXD_Amphibious_CLI_Robust.exe
