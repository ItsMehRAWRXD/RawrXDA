@echo off
setlocal enabledelayedexpansion

REM Setup MSVC environment - use BuildTools path
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

REM Set Windows SDK library path
set LIBPATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64;%LIBPATH%

REM Compile
cd /d D:\rawrxd
ml64 /c /Zi RawrXD_Amphibious_ML64_Complete.asm
if %errorlevel% neq 0 (
    echo Compilation failed
    exit /b %errorlevel%
)

echo === Object file created successfully ===

REM Link for CLI - use full paths to libs
link /subsystem:console /entry:main /out:RawrXD_Amphibious_CLI.exe ^
    RawrXD_Amphibious_ML64_Complete.obj ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\kernel32.lib" ^
    "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\user32.lib"

if %errorlevel% neq 0 (
    echo Linking failed
    exit /b %errorlevel%
)

echo === Compilation and linking successful ===
echo === Running CLI executable ===
RawrXD_Amphibious_CLI.exe
