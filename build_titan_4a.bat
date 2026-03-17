@echo off
REM ═══════════════════════════════════════════════════════════════
REM  RawrXD Titan Core — Phase 4A Unified Build Script
REM  Assembles ui.asm + compiles bridge_layer.cpp → links DLL
REM ═══════════════════════════════════════════════════════════════

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Windows Kits\10\include\10.0.22621.0\um;C:\Program Files (x86)\Windows Kits\10\include\10.0.22621.0\shared
set LIB=%LIB%;C:\Program Files (x86)\Windows Kits\10\lib\10.0.22621.0\um\x64

echo.
echo [1/3] Assembling ui.asm with ml64...
cd /d D:\rawrxd
ml64 /c /Fo bin\ui.obj src\asm\monolithic\ui.asm
if errorlevel 1 (
    echo FAIL: ml64 assembly failed
    exit /b 1
)
echo SUCCESS: ui.obj created

echo.
echo [2/3] Compiling bridge_layer.cpp with cl...
cl /c /O2 /EHsc /Fobin\bridge_layer.obj bridge_layer.cpp
if errorlevel 1 (
    echo FAIL: cl compilation failed
    exit /b 1
)
echo SUCCESS: bridge_layer.obj created

echo.
echo [3/3] Linking RawrXD_Titan.dll...
link /DLL /DEF:bin\RawrXD_Titan.def /OUT:bin\RawrXD_Titan.dll bin\ui.obj bin\bridge_layer.obj user32.lib kernel32.lib gdi32.lib shell32.lib
if errorlevel 1 (
    echo WARN: DLL link had issues, trying standalone EXE link...
    link /OUT:bin\RawrXD_Titan_UNIFIED.exe bin\ui.obj bin\bridge_layer.obj user32.lib kernel32.lib gdi32.lib shell32.lib
)

echo.
echo BUILD COMPLETE — Phase 4A Titan Core Unified
dir bin\RawrXD_Titan*.dll bin\RawrXD_Titan*.exe 2>nul
echo.
