@echo off
REM ═══════════════════════════════════════════════════════════════════════════
REM RawrXD_HttpChatServer Build Script
REM Assembles and links the pure x64 MASM HTTP chat server module
REM ═══════════════════════════════════════════════════════════════════════════

setlocal EnableDelayedExpansion

echo ═══════════════════════════════════════════════════════════════════════════
echo  RawrXD HTTP Chat Server - MASM64 Build
echo ═══════════════════════════════════════════════════════════════════════════
echo.

REM Check for Visual Studio environment
if "%VSINSTALLDIR%"=="" (
    echo [INFO] Visual Studio environment not detected, attempting to initialize...
    
    REM Try VS2022 first
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else if exist "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
        call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    ) else (
        echo [ERROR] Could not find Visual Studio installation
        exit /b 1
    )
)

REM Verify ml64 is available
where ml64 >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ml64.exe not found in PATH
    echo Please run from a Visual Studio x64 Native Tools Command Prompt
    exit /b 1
)

echo [INFO] Using ML64 from: 
where ml64

REM Set paths
set SRCDIR=%~dp0
set OUTDIR=%SRCDIR%build
set SRCFILE=%SRCDIR%RawrXD_HttpChatServer.asm
set DEFFILE=%SRCDIR%RawrXD_HttpChatServer.def

REM Create output directory
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo.
echo [STEP 1] Assembling RawrXD_HttpChatServer.asm...
echo.

ml64 /c /Fo"%OUTDIR%\RawrXD_HttpChatServer.obj" /W3 /Zi "%SRCFILE%"
if errorlevel 1 (
    echo.
    echo [ERROR] Assembly failed!
    exit /b 1
)

echo.
echo [STEP 2] Linking static library...
echo.

lib /OUT:"%OUTDIR%\RawrXD_HttpChatServer.lib" "%OUTDIR%\RawrXD_HttpChatServer.obj"
if errorlevel 1 (
    echo.
    echo [ERROR] Static library creation failed!
    exit /b 1
)

echo.
echo [STEP 3] Linking DLL...
echo.

link /DLL ^
     /DEF:"%DEFFILE%" ^
     /OUT:"%OUTDIR%\RawrXD_HttpChatServer.dll" ^
     /IMPLIB:"%OUTDIR%\RawrXD_HttpChatServer_import.lib" ^
     /DEBUG ^
     /PDB:"%OUTDIR%\RawrXD_HttpChatServer.pdb" ^
     "%OUTDIR%\RawrXD_HttpChatServer.obj" ^
     kernel32.lib ^
     user32.lib ^
     wininet.lib ^
     shlwapi.lib ^
     advapi32.lib ^
     shell32.lib

if errorlevel 1 (
    echo.
    echo [ERROR] DLL linking failed!
    exit /b 1
)

echo.
echo ═══════════════════════════════════════════════════════════════════════════
echo  Build Complete!
echo ═══════════════════════════════════════════════════════════════════════════
echo.
echo Output files:
echo   Object:        %OUTDIR%\RawrXD_HttpChatServer.obj
echo   Static Lib:    %OUTDIR%\RawrXD_HttpChatServer.lib
echo   DLL:           %OUTDIR%\RawrXD_HttpChatServer.dll
echo   Import Lib:    %OUTDIR%\RawrXD_HttpChatServer_import.lib
echo   Debug Symbols: %OUTDIR%\RawrXD_HttpChatServer.pdb
echo.
echo To use in your project:
echo   1. Include RawrXD_HttpChatServer.h
echo   2. Link with RawrXD_HttpChatServer.lib (static) or
echo      RawrXD_HttpChatServer_import.lib (DLL)
echo   3. If using DLL, place RawrXD_HttpChatServer.dll in your PATH
echo.

endlocal
exit /b 0
