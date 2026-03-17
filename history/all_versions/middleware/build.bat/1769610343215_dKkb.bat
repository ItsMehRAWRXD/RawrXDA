@echo off
REM ============================================================================
REM Build script for RawrXD MASM Middleware (System Tray Service)
REM ============================================================================

echo.
echo ================================================
echo   RawrXD MASM Middleware Build Script
echo ================================================
echo.

set ML64=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe
set LINK=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe
set LIBPATH=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\onecore\x64
set SDKLIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64
set UCRTLIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64

if not exist "%ML64%" (
    echo ERROR: ml64.exe not found!
    pause
    exit /b 1
)

echo [1/2] Assembling rawrxd_middleware.asm...
"%ML64%" /c /nologo rawrxd_middleware.asm
if errorlevel 1 (
    echo ERROR: Assembly failed!
    pause
    exit /b 1
)
echo       [OK] Assembly complete

echo.
echo [2/2] Linking rawrxd_middleware.obj...
set LIB=%LIBPATH%;%SDKLIB%;%UCRTLIB%
"%LINK%" /nologo /subsystem:windows /entry:WinMain ^
    rawrxd_middleware.obj ^
    kernel32.lib user32.lib ws2_32.lib shell32.lib gdi32.lib ^
    msvcrt.lib ucrt.lib vcruntime.lib legacy_stdio_definitions.lib ^
    /out:rawrxd_middleware.exe
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
echo Output: rawrxd_middleware.exe
echo.
echo This creates a system tray application that:
echo   - Sits in your taskbar notification area
echo   - Listens on http://localhost:8080
echo   - Dynamically scans for GGUF models
echo   - Provides API for GUI, CLI, and extensions
echo.
echo Endpoints:
echo   GET  /models  - List all discovered models
echo   POST /ask     - Send a question
echo   GET  /health  - Service status
echo   POST /scan    - Rescan model directories
echo   GET  /config  - View configuration
echo.
echo Right-click tray icon for menu!
echo ================================================
