@echo off
REM ============================================================================
REM Build Script: RawrXD Win32 IDE + MASM CLI v1.0
REM ============================================================================
REM This script compiles:
REM   1. RawrXD_MASM_CLI_x64.asm -> RawrXD_MASM_CLI_x64.dll (x64 native assembly)
REM   2. RawrXD_Win32_IDE.cpp -> RawrXD_Win32_IDE.exe (IDE with split-pane terminal)
REM ============================================================================

cd /D D:\rawrxd\Ship

set MSVC=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.39.33519
set MLPath=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64
set SDK=C:\Program Files (x86)\Windows Kits\10

echo.
echo =============================================================
echo  PHASE 1: Assembling RawrXD_MASM_CLI_x64.asm
echo =============================================================
echo.

setlocal enabledelayedexpansion

REM Check if ml64.exe exists
if not exist "%MLPath%\ml64.exe" (
    echo ERROR: ml64.exe not found at %MLPath%\ml64.exe
    echo. 
    echo Searching for ml64.exe...
    for /r "C:\Program Files\Microsoft Visual Studio" %%F in (ml64.exe) do (
        echo Found: %%F
        set MLPath=%%~dpF
    )
)

if exist "%MLPath%\ml64.exe" (
    echo Found ml64.exe at: %MLPath%\ml64.exe
    "%MLPath%\ml64.exe" /c /Fo RawrXD_MASM_CLI_x64.obj RawrXD_MASM_CLI_x64.asm
    if errorlevel 1 (
        echo ERROR: Assembly failed!
        exit /b 1
    )
    echo MASM Assembly successful.
) else (
    echo WARNING: ml64.exe not found, skipping MASM CLI DLL build
    echo The split-pane terminal will still work with PowerShell only
    goto skip_masm
)

REM Link the MASM object into a DLL
"%MSVC%\bin\Hostx64\x64\link.exe" /DLL ^
    /EXPORT:CLI_Initialize ^
    /EXPORT:CLI_ExecuteCommand ^
    /EXPORT:CLI_GetOutput ^
    /EXPORT:CLI_Shutdown ^
    /OUT:RawrXD_MASM_CLI_x64.dll ^
    RawrXD_MASM_CLI_x64.obj kernel32.lib

if errorlevel 1 (
    echo WARNING: DLL linking failed, continuing with IDE build
) else (
    echo MASM CLI DLL created successfully: RawrXD_MASM_CLI_x64.dll
)

:skip_masm

echo.
echo =============================================================
echo  PHASE 2: Compiling RawrXD_Win32_IDE.cpp
echo =============================================================
echo.

REM Use MSVC compiler
"%MSVC%\bin\Hostx64\x64\cl.exe" /O2 ^
    /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /D_CRT_SECURE_NO_WARNINGS ^
    /DNOMINMAX /EHsc /std:c++17 /W1 ^
    /I"%SDK%\Include\10.0.22621.0\um" ^
    /I"%SDK%\Include\10.0.22621.0\shared" ^
    RawrXD_Win32_IDE.cpp ^
    /link /SUBSYSTEM:WINDOWS ^
    user32.lib gdi32.lib comctl32.lib shell32.lib ole32.lib ^
    comdlg32.lib advapi32.lib shlwapi.lib ws2_32.lib wininet.lib

if errorlevel 1 (
    echo ERROR: IDE compilation failed!
    exit /b 1
)

echo.
echo =============================================================
echo  COMPILATION COMPLETE
echo =============================================================
echo.
echo IDE Built: RawrXD_Win32_IDE.exe
if exist RawrXD_MASM_CLI_x64.dll (
    echo MASM CLI Built: RawrXD_MASM_CLI_x64.dll
)
echo.
echo Ready to launch: RawrXD_Win32_IDE.exe
echo.
pause
