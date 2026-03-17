@echo off
title Building Systems IDE Core - C++ with Assembly Optimizations
color 0A

echo ========================================
echo Building Systems Engineering IDE Core
echo C++ with Inline Assembly Optimizations  
echo ========================================
echo.

REM Check for Visual Studio Build Tools
where cl.exe >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Setting up Visual Studio environment...
    call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" 2>nul
    if %ERRORLEVEL% NEQ 0 (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" 2>nul
        if %ERRORLEVEL% NEQ 0 (
            echo Error: Visual Studio Build Tools not found!
            echo Please install Visual Studio 2019/2022 with C++ support
            pause
            exit /b 1
        )
    )
)

echo [1/4] Compiling C++ Core with Assembly Optimizations...
cl.exe /EHsc /O2 /Ox /Ob2 /Oi /Ot /favor:INTEL64 /arch:AVX2 ^
    /DWIN32 /D_WIN64 /DNDEBUG ^
    /I. ^
    /c systems-ide-core.cpp

if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to compile C++ core
    pause
    exit /b 1
)

echo [2/4] Creating Dynamic Link Library...
link.exe /DLL /OUT:systems-ide-core.dll ^
    systems-ide-core.obj ^
    kernel32.lib user32.lib advapi32.lib ^
    /OPT:REF /OPT:ICF /MACHINE:X64

if %ERRORLEVEL% NEQ 0 (
    echo Error: Failed to link DLL
    pause
    exit /b 1
)

echo [3/4] Creating Native Executable...
cl.exe /EHsc /O2 /Ox /Ob2 /Oi /Ot /favor:INTEL64 /arch:AVX2 ^
    /DWIN32 /D_WIN64 /DNDEBUG /DSTANDALONE_BUILD ^
    /I. ^
    systems-ide-core.cpp ide-main.cpp ^
    /Fe:SystemsIDE.exe ^
    kernel32.lib user32.lib advapi32.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo Warning: Failed to create standalone executable
)

echo [4/4] Optimizing Binary...
REM Strip debug symbols and optimize
editbin.exe /RELEASE /OSVERSION:6.1 SystemsIDE.exe >nul 2>nul

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Generated Files:
echo - systems-ide-core.dll  (C++ Core Library)
echo - systems-ide-core.lib  (Import Library) 
echo - SystemsIDE.exe        (Standalone Binary)
echo.
echo Performance Features:
echo - SIMD AVX2 Optimizations
echo - Memory-Mapped File I/O
echo - Hardware Performance Counters
echo - Assembly-Optimized String Operations
echo - Zero-Copy Network I/O
echo - Direct System Call Interface
echo.

REM Test the DLL
if exist systems-ide-core.dll (
    echo Testing DLL export symbols...
    dumpbin /EXPORTS systems-ide-core.dll | findstr "CreateIDECore"
    if %ERRORLEVEL% EQU 0 (
        echo ✓ DLL exports verified
    ) else (
        echo ⚠ Warning: DLL exports not found
    )
)

echo.
echo Ready for integration with Node.js backend!
echo Use: const core = require('./systems-ide-core.dll');
pause