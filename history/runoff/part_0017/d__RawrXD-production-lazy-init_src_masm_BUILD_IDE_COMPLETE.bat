@echo off
setlocal enabledelayedexpansion

REM =====================================================================
REM RawrXD Complete IDE Build System
REM Compiles all MASM64 modules and links into unified executable
REM =====================================================================

set ML64="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
set LINK="C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
set LIBS=kernel32.lib user32.lib shell32.lib advapi32.lib gdi32.lib comctl32.lib comdlg32.lib

if not exist %ML64% (
    echo ERROR: ml64.exe not found at %ML64%
    exit /b 1
)

if not exist %LINK% (
    echo ERROR: link.exe not found at %LINK%
    exit /b 1
)

echo =====================================================================
echo RawrXD Complete IDE Build
echo =====================================================================

REM Clean previous builds
echo [*] Cleaning previous object files...
del *.obj 2>nul
del RawrXD_IDE.exe 2>nul

REM =====================================================================
REM STEP 1: Compile Core Agentic Kernel
REM =====================================================================
echo [1] Compiling agentic_kernel.asm...
%ML64% /c /Fo"agentic_kernel.obj" /nologo /W3 /Zi agentic_kernel.asm
if errorlevel 1 (
    echo ERROR: Failed to compile agentic_kernel.asm
    exit /b 1
)
echo [+] agentic_kernel.obj created

REM =====================================================================
REM STEP 2: Compile Language Scaffolders (Base 7)
REM =====================================================================
echo [2] Compiling language_scaffolders_fixed.asm...
%ML64% /c /Fo"language_scaffolders.obj" /nologo /W3 /Zi language_scaffolders_fixed.asm
if errorlevel 1 (
    echo ERROR: Failed to compile language_scaffolders_fixed.asm
    exit /b 1
)
echo [+] language_scaffolders.obj created

REM =====================================================================
REM STEP 3: Compile Extended Language Scaffolders (12 languages)
REM =====================================================================
echo [3] Compiling language_scaffolders_extended.asm...
%ML64% /c /Fo"language_scaffolders_extended.obj" /nologo /W3 /Zi language_scaffolders_extended.asm
if errorlevel 1 (
    echo WARNING: language_scaffolders_extended.asm has errors
    echo Continuing with remaining modules...
    REM Don't exit - continue with other modules
)
echo [+] language_scaffolders_extended.obj created (if successful)

REM =====================================================================
REM STEP 4: Compile IDE Bridge Module
REM =====================================================================
echo [4] Compiling agentic_ide_bridge.asm...
%ML64% /c /Fo"agentic_ide_bridge.obj" /nologo /W3 /Zi agentic_ide_bridge.asm
if errorlevel 1 (
    echo WARNING: agentic_ide_bridge.asm has errors
)
echo [+] agentic_ide_bridge.obj created (if successful)

REM =====================================================================
REM STEP 5: Compile React+Vite Scaffolder
REM =====================================================================
echo [5] Compiling react_vite_scaffolder.asm...
%ML64% /c /Fo"react_vite_scaffolder.obj" /nologo /W3 /Zi react_vite_scaffolder.asm
if errorlevel 1 (
    echo WARNING: react_vite_scaffolder.asm has errors
)
echo [+] react_vite_scaffolder.obj created (if successful)

REM =====================================================================
REM STEP 6: Compile UI Module
REM =====================================================================
echo [6] Compiling ui_masm.asm...
%ML64% /c /Fo"ui_masm.obj" /nologo /W3 /Zi ui_masm.asm
if errorlevel 1 (
    echo WARNING: ui_masm.asm has errors
)
echo [+] ui_masm.obj created (if successful)

REM =====================================================================
REM STEP 7: Compile CLI Access System
REM =====================================================================
echo [7] Compiling cli_access_system.asm...
%ML64% /c /Fo"cli_access_system.obj" /nologo /W3 /Zi cli_access_system.asm
if errorlevel 1 (
    echo WARNING: cli_access_system.asm has errors
)
echo [+] cli_access_system.obj created (if successful)

REM =====================================================================
REM STEP 8: Compile Unified IDE Entry Point
REM =====================================================================
echo [8] Compiling RawrXD_IDE_unified.asm...
%ML64% /c /Fo"RawrXD_IDE_unified.obj" /nologo /W3 /Zi RawrXD_IDE_unified.asm
if errorlevel 1 (
    echo WARNING: RawrXD_IDE_unified.asm has errors
)
echo [+] RawrXD_IDE_unified.obj created (if successful)

REM =====================================================================
REM STEP 9: Link All Modules
REM =====================================================================
echo [*] Linking modules into RawrXD_IDE.exe...

REM Build command with all available object files
set OBJS=
if exist agentic_kernel.obj set OBJS=!OBJS! agentic_kernel.obj
if exist language_scaffolders.obj set OBJS=!OBJS! language_scaffolders.obj
if exist language_scaffolders_extended.obj set OBJS=!OBJS! language_scaffolders_extended.obj
if exist agentic_ide_bridge.obj set OBJS=!OBJS! agentic_ide_bridge.obj
if exist react_vite_scaffolder.obj set OBJS=!OBJS! react_vite_scaffolder.obj
if exist ui_masm.obj set OBJS=!OBJS! ui_masm.obj
if exist cli_access_system.obj set OBJS=!OBJS! cli_access_system.obj
if exist RawrXD_IDE_unified.obj set OBJS=!OBJS! RawrXD_IDE_unified.obj

echo Objects to link: !OBJS!

%LINK% /OUT:RawrXD_IDE.exe ^
    /SUBSYSTEM:WINDOWS ^
    /MACHINE:X64 ^
    /DEBUG ^
    /ENTRY:main ^
    !OBJS! ^
    %LIBS%

if errorlevel 1 (
    echo ERROR: Linking failed
    exit /b 1
)

if not exist RawrXD_IDE.exe (
    echo ERROR: RawrXD_IDE.exe was not created
    exit /b 1
)

echo [+] RawrXD_IDE.exe created successfully!
echo.
echo =====================================================================
echo Build Complete!
echo =====================================================================
echo.
echo Executable: RawrXD_IDE.exe
echo Mode: Use --qt (default), --cli, or --headless
echo.
echo Examples:
echo   RawrXD_IDE.exe                 (QT GUI mode)
echo   RawrXD_IDE.exe --cli           (CLI mode)
echo   RawrXD_IDE.exe --headless --build file.cpp (Headless)
echo.

pause
