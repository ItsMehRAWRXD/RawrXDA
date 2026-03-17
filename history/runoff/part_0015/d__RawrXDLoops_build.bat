@echo off
setlocal enabledelayedexpansion

echo Building RawrXDLoops MASM64 DAW...
echo.

REM Set MASM32 environment
set MASM32_PATH=C:\masm32
set PATH=%MASM32_PATH%\bin;%PATH%

REM Check if MASM is available
where ml >nul 2>&1
if errorlevel 1 (
    echo ERROR: ml not found in PATH
    echo Please install MASM32 or add it to your PATH
    exit /b 1
)

REM Check if linker is available
where link >nul 2>&1
if errorlevel 1 (
    echo ERROR: link not found in PATH
    echo Please install MASM32 or add it to your PATH
    exit /b 1
)

echo Compiling RawrXDLoops.asm...
ml /c /Cp /Cx /Zi /coff RawrXDLoops.asm
if errorlevel 1 (
    echo Compilation failed!
    exit /b 1
)

echo Linking RawrXDLoops.exe...
link /subsystem:windows /entry:WinMain /machine:x64 /debug /out:RawrXDLoops.exe RawrXDLoops.obj kernel32.lib user32.lib gdi32.lib dsound.lib winmm.lib
if errorlevel 1 (
    echo Linking failed!
    exit /b 1
)

echo Build successful! RawrXDLoops.exe created.
echo.
echo To run: RawrXDLoops.exe
echo.
echo Hot-reload commands:
echo - "techno" - Sets BPM to 135, cleaner mix
echo - "acid" - Sets resonance to 0.95, squelch maximized
echo - "ambient" - Sets BPM to 70, low-pass filter
echo - "add reverb" - Adds reverb module
echo - "change bpm 130" - Changes tempo to 130 BPM

pause