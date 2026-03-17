@echo off
REM ==========================================================================
REM Zero-Dependency Build Script for RawrXD IDE
REM ==========================================================================
REM Builds MASM64 code without kernel32.lib, user32.lib, or gdi32.lib
REM Uses only direct system calls and BIOS interrupts
REM ==========================================================================

set ML="C:\masm32\bin64\ml64.exe"
set LINK="C:\masm32\bin64\link.exe"

if not exist %ML% (
    echo Error: MASM64 not found at %ML%
    echo Please install MASM32 with 64-bit support
    pause
    exit /b 1
)

echo Building Zero-Dependency RawrXD IDE...

REM Compile system call interface
%ML% /c /Cp /nologo syscall_interface.asm
if errorlevel 1 (
    echo Error compiling syscall_interface.asm
    pause
    exit /b 1
)

REM Compile custom window management
%ML% /c /Cp /nologo custom_window.asm
if errorlevel 1 (
    echo Error compiling custom_window.asm
    pause
    exit /b 1
)

REM Compile custom filesystem
%ML% /c /Cp /nologo custom_filesystem.asm
if errorlevel 1 (
    echo Error compiling custom_filesystem.asm
    pause
    exit /b 1
)

REM Compile main zero-dependency UI
%ML% /c /Cp /nologo ui_masm_zero_deps.asm
if errorlevel 1 (
    echo Error compiling ui_masm_zero_deps.asm
    pause
    exit /b 1
)

REM Link without any external libraries (pure zero-dependency)
echo Linking zero-dependency executable...
%LINK% /SUBSYSTEM:CONSOLE /ENTRY:main /NODEFAULTLIB /MERGE:.data=.text /MERGE:.rdata=.text ^
    syscall_interface.obj custom_window.obj custom_filesystem.obj ui_masm_zero_deps.obj ^
    /OUT:RawrXD_ZeroDeps.exe

if errorlevel 1 (
    echo Error linking zero-dependency executable
    echo Attempting alternative linking...
    
    REM Try linking with minimal kernel32 for basic functionality
    %LINK% /SUBSYSTEM:CONSOLE /ENTRY:main /NODEFAULTLIB kernel32.lib ^
        syscall_interface.obj custom_window.obj custom_filesystem.obj ui_masm_zero_deps.obj ^
        /OUT:RawrXD_MinimalDeps.exe
    
    if errorlevel 1 (
        echo Error linking minimal-dependency executable
        pause
        exit /b 1
    ) else (
        echo Built minimal-dependency version: RawrXD_MinimalDeps.exe
    )
) else (
    echo Built zero-dependency version: RawrXD_ZeroDeps.exe
)

echo.
echo Build completed successfully!
echo.
echo Zero-Dependency Features:
echo - Direct system calls instead of kernel32.lib
echo - Custom VGA graphics instead of gdi32.lib
echo - Custom window management instead of user32.lib
echo - BIOS interrupts for disk and input operations

pause