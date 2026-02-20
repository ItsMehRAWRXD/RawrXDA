@echo off
REM =============================================================================
REM Build script for RawrXD MASM x64 Backend Server
REM =============================================================================
REM Requires: Visual Studio with MASM64 (ml64.exe)
REM =============================================================================

echo.
echo ================================================
echo   RawrXD MASM Backend Build Script v3.0
echo ================================================
echo.

REM Set paths directly
set ML64=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe
set LINK=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe
set LIBPATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64
set SDKLIBPATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64
set UCRTLIBPATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64

REM Check if ml64 exists
if not exist "%ML64%" (
    echo ERROR: ml64.exe not found at %ML64%
    echo Please update the path in this script
    pause
    exit /b 1
)

echo [1/2] Assembling rawrxd_backend.asm...
"%ML64%" /c /nologo rawrxd_backend.asm
if errorlevel 1 (
    echo ERROR: Assembly failed!
    pause
    exit /b 1
)
echo       [OK] Assembly complete

echo.
echo [2/2] Linking rawrxd_backend.obj...
"%LINK%" /nologo /subsystem:console /entry:start ^
    /libpath:"%LIBPATH%" ^
    /libpath:"%SDKLIBPATH%" ^
    /libpath:"%UCRTLIBPATH%" ^
    rawrxd_backend.obj ^
    ws2_32.lib kernel32.lib msvcrt.lib legacy_stdio_definitions.lib ^
    /out:rawrxd_backend.exe
if errorlevel 1 (
    echo ERROR: Linking failed!
    pause
    exit /b 1
)
echo       [OK] Linking complete

echo.
echo ================================================
echo   BUILD SUCCESSFUL!
echo ================================================
echo.
echo Output: rawrxd_backend.exe
echo.
echo To run:
echo   rawrxd_backend.exe
echo.
echo Endpoints:
echo   GET  http://localhost:8080/models
echo   POST http://localhost:8080/ask
echo   GET  http://localhost:8080/health
echo.
echo Your frontend beacon will flip to ONLINE!
echo ================================================
