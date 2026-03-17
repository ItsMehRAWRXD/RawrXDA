@echo off
setlocal enabledelayedexpansion

REM Setup MSVC environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM Compile
cd /d D:\rawrxd
ml64 /c /Zi RawrXD_Amphibious_ML64_Complete.asm

if %errorlevel% neq 0 (
    echo === COMPILATION FAILED ===
    exit /b %errorlevel%
)

echo === Object file created successfully ===

REM Link for CLI 
link /subsystem:console /entry:main /out:RawrXD_Amphibious_CLI.exe ^
    RawrXD_Amphibious_ML64_Complete.obj ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\kernel32.lib" ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\user32.lib"

if %errorlevel% neq 0 (
    echo === LINKING FAILED ===
    exit /b %errorlevel%
)

echo === Build successful ===
echo === CLI executable: RawrXD_Amphibious_CLI.exe ===
