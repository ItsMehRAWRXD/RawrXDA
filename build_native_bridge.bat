@echo off
REM ==============================================================================
REM RawrXD Native Model Bridge Build Script
REM Compiles MASM64 ASM to Windows x64 DLL
REM ==============================================================================

setlocal enabledelayedexpansion

REM Set build paths
set SOURCE_DIR=%~dp0
set BUILD_DIR=%SOURCE_DIR%build_output
set BIN_DIR=%SOURCE_DIR%..\bin
set INCLUDE_DIR=%SOURCE_DIR%..\include

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

REM Find ml64.exe (MASM64 assembler)
set ML64_PATH=
for /f "delims=" %%i in ('where ml64.exe 2^>nul') do set ML64_PATH=%%i

if not defined ML64_PATH (
    REM Try Visual Studio Build Tools path
    if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC" (
        for /d %%d in ("C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"\*) do (
            if exist "%%d\bin\Hostx64\x64\ml64.exe" (
                set ML64_PATH=%%d\bin\Hostx64\x64\ml64.exe
                goto :found_ml64
            )
        )
    )
    
    REM Try VS2022 Enterprise path
    if exist "C:\VS2022Enterprise\VC\Tools\MSVC" (
        for /d %%d in ("C:\VS2022Enterprise\VC\Tools\MSVC"\*) do (
            if exist "%%d\bin\Hostx64\x64\ml64.exe" (
                set ML64_PATH=%%d\bin\Hostx64\x64\ml64.exe
                goto :found_ml64
            )
        )
    )
)

:found_ml64
if not defined ML64_PATH (
    echo Error: ml64.exe not found in PATH or expected locations
    echo Please install Visual Studio Build Tools or add ml64.exe to PATH
    exit /b 1
)

echo Found ml64.exe at: %ML64_PATH%

REM Find lib.exe (linker)
set LIB_PATH=
for /f "delims=" %%i in ('where lib.exe 2^>nul') do set LIB_PATH=%%i

if not defined LIB_PATH (
    REM Try same MSVC directory as ml64.exe
    for /f "%%d in ('echo %ML64_PATH:\bin\Hostx64\x64\ml64.exe=%') do (
        if exist "%%d\bin\Hostx64\x64\lib.exe" (
            set LIB_PATH=%%d\bin\Hostx64\x64\lib.exe
            goto :found_lib
        )
    )
)

:found_lib
if not defined LIB_PATH (
    echo Warning: lib.exe not found - linking may fail
)

echo.
echo ============================================================
echo Building RawrXD_NativeModelBridge.asm
echo ============================================================
echo Assembler: %ML64_PATH%
echo Source: %SOURCE_DIR%RawrXD_NativeModelBridge_Complete.asm
echo Output: %BUILD_DIR%\RawrXD_NativeModelBridge.obj
echo.

REM Run assembler
"%ML64_PATH%" /c /W3 /WX /Fo"%BUILD_DIR%\RawrXD_NativeModelBridge.obj" "%SOURCE_DIR%RawrXD_NativeModelBridge_Complete.asm"

if !errorlevel! neq 0 (
    echo.
    echo ERROR: Assembly failed with exit code !errorlevel!
    exit /b !errorlevel!
)

echo Assembly completed successfully.
echo.

REM Link to DLL
echo ============================================================
echo Linking to DLL
echo ============================================================

REM Find linker (link.exe)
set LINK_PATH=
for /f "delims=" %%i in ('where link.exe 2^>nul') do set LINK_PATH=%%i

if not defined LINK_PATH (
    for /f "%%d in ('echo %ML64_PATH:\bin\Hostx64\x64\ml64.exe=%') do (
        if exist "%%d\bin\Hostx64\x64\link.exe" (
            set LINK_PATH=%%d\bin\Hostx64\x64\link.exe
        )
    )
)

if not defined LINK_PATH (
    echo Error: link.exe not found
    exit /b 1
)

echo Linker: %LINK_PATH%
echo Output: %BIN_DIR%\RawrXD_NativeModelBridge.dll
echo.

REM Build library list
set LIBS=kernel32.lib ntdll.lib user32.lib msvcrt.lib

"%LINK_PATH%" /DLL /SUBSYSTEM:WINDOWS /MACHINE:x64 ^
    /OUT:"%BIN_DIR%\RawrXD_NativeModelBridge.dll" ^
    /IMPLIB:"%BUILD_DIR%\RawrXD_NativeModelBridge.lib" ^
    "%BUILD_DIR%\RawrXD_NativeModelBridge.obj" ^
    %LIBS%

if !errorlevel! neq 0 (
    echo.
    echo ERROR: Linking failed with exit code !errorlevel!
    exit /b !errorlevel!
)

echo.
echo ============================================================
echo BUILD SUCCESSFUL
echo ============================================================
echo Output DLL: %BIN_DIR%\RawrXD_NativeModelBridge.dll
echo Import Lib: %BUILD_DIR%\RawrXD_NativeModelBridge.lib
echo.

REM Verify DLL exports
echo Checking DLL exports...
dumpbin.exe /EXPORTS "%BIN_DIR%\RawrXD_NativeModelBridge.dll" > "%BUILD_DIR%\exports.txt" 2>&1

if exist "%BUILD_DIR%\exports.txt" (
    echo Exports saved to: %BUILD_DIR%\exports.txt
    type "%BUILD_DIR%\exports.txt"
)

echo.
echo Build complete!
pause
exit /b 0
