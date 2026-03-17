@echo off
REM MASM Toolchain Setup Script for RawrXD IDE
REM This script sets up the MASM toolchain environment

setlocal enabledelayedexpansion

REM Set toolchain paths
set MASM32_PATH=C:\\masm32
set ML64_PATH=C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC\\14.29.30133\\bin\\Hostx64\\x64
set LINK_PATH=C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Tools\\MSVC\\14.29.30133\\bin\\Hostx64\\x64
set WINDBG_PATH=C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64

REM Check if MASM32 is installed
if not exist "%MASM32_PATH%" (
    echo ERROR: MASM32 not found at %MASM32_PATH%
    echo Please install MASM32 or update the path in this script
    exit /b 1
)

REM Check if ML64 is available
if not exist "%ML64_PATH%\\ml64.exe" (
    echo WARNING: ml64.exe not found at %ML64_PATH%
    echo Trying to find ml64.exe...
    
    REM Search for ml64.exe in common locations
    for %%d in ("C:\\Program Files (x86)\\Microsoft Visual Studio\\*" ) do (
        if exist "%%~d\\BuildTools\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64\\ml64.exe" (
            set "ML64_PATH=%%~d\\BuildTools\\VC\\Tools\\MSVC\\*\\bin\\Hostx64\\x64"
            goto :found_ml64
        )
    )
    
    echo ERROR: ml64.exe not found. Please install Visual Studio Build Tools.
    exit /b 1
)

:found_ml64

REM Check if linker is available
if not exist "%LINK_PATH%\\link.exe" (
    echo ERROR: link.exe not found at %LINK_PATH%
    exit /b 1
)

REM Check if debugger is available
if not exist "%WINDBG_PATH%\\windbg.exe" (
    echo WARNING: windbg.exe not found at %WINDBG_PATH%
    echo Debugging capabilities will be limited
)

REM Set environment variables
set MASM32_INCLUDE=%MASM32_PATH%\\include
set MASM32_LIB=%MASM32_PATH%\\lib
set ML64_BIN=%ML64_PATH%
set LINK_BIN=%LINK_PATH%
set WINDBG_BIN=%WINDBG_PATH%

REM Add to PATH
set PATH=%ML64_PATH%;%LINK_PATH%;%WINDBG_PATH%;%PATH%

REM Create configuration file for RawrXD IDE
(
    echo [MASM_Toolchain]
    echo masm32_path=%MASM32_PATH%
    echo ml64_path=%ML64_PATH%
    echo link_path=%LINK_PATH%
    echo windbg_path=%WINDBG_PATH%
    echo include_paths=%MASM32_INCLUDE%
    echo lib_paths=%MASM32_LIB%
    echo compiler=ml64.exe
    echo linker=link.exe
    echo debugger=windbg.exe
) > masm_toolchain.config

echo MASM Toolchain configured successfully!
echo.
echo Configuration:
echo MASM32 Path: %MASM32_PATH%
echo ML64 Path: %ML64_PATH%
echo Linker Path: %LINK_PATH%
echo Debugger Path: %WINDBG_PATH%
echo.
echo Configuration saved to: masm_toolchain.config

REM Test compilation
if "%1"=="test" (
    echo.
    echo Testing compilation...
    
    REM Create a simple test file
    (
        echo .model flat, stdcall
        echo option casemap:none
        echo include \\masm32\\include\\masm32rt.inc
        echo .code
        echo start:
        echo     invoke ExitProcess, 0
        echo end start
    ) > test_compile.asm
    
    REM Try to compile
    ml64.exe /c test_compile.asm
    if errorlevel 1 (
        echo ERROR: Compilation test failed
        del test_compile.asm
        exit /b 1
    )
    
    REM Try to link
    link.exe /subsystem:console test_compile.obj
    if errorlevel 1 (
        echo ERROR: Linking test failed
        del test_compile.asm
        del test_compile.obj
        exit /b 1
    )
    
    echo SUCCESS: Compilation test passed!
    del test_compile.asm
    del test_compile.obj
    del test_compile.exe
)

endlocal