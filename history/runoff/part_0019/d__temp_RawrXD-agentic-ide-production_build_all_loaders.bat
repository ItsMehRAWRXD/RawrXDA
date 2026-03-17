@echo off
REM Build all MASM loaders for benchmarking
REM Platform: x64, Windows

setlocal enabledelayedexpansion

set "SRCDIR=D:\temp\RawrXD-agentic-ide-production"
set "OUTDIR=%SRCDIR%\build-loaders"
set "ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe""
set "LINK="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe""

if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo [BUILD] Assembling beacon_manager_main.asm...
%ML64% /c /Fo "%OUTDIR%\beacon_manager_main.obj" "%SRCDIR%\src\masm_pure\beacon_manager_main.asm"
if errorlevel 1 (
    echo [ERROR] beacon_manager_main.asm failed
    exit /b 1
)

echo [BUILD] Assembling sliding_window_core.asm...
%ML64% /c /Fo "%OUTDIR%\sliding_window_core.obj" "%SRCDIR%\src\masm_pure\sliding_window_core.asm"
if errorlevel 1 (
    echo [ERROR] sliding_window_core.asm failed
    exit /b 1
)

echo [BUILD] Assembling gguf_memory_map.asm...
%ML64% /c /Fo "%OUTDIR%\gguf_memory_map.obj" "%SRCDIR%\gguf_memory_map.asm"
if errorlevel 1 (
    echo [ERROR] gguf_memory_map.asm failed
    exit /b 1
)

echo [BUILD] Assembling sliding_window_integration.asm...
%ML64% /c /Fo "%OUTDIR%\sliding_window_integration.obj" "%SRCDIR%\sliding_window_integration.asm"
if errorlevel 1 (
    echo [ERROR] sliding_window_integration.asm failed
    exit /b 1
)

echo [BUILD] All assemblies successful
echo [OUTPUT] Objects in %OUTDIR%
dir /b "%OUTDIR%\*.obj"

echo [BUILD] Done
