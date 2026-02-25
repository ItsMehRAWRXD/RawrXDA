@echo off
REM ============================================================================
REM OS Explorer Interceptor Build Script - Simplified Version
REM Uses Visual Studio Build Tools with simplified ASM files
REM ============================================================================

echo Building OS Explorer Interceptor (Simplified Version)...
echo.

REM Set paths for Visual Studio Build Tools
set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
set "VC_TOOLS=%VS_PATH%\VC\Tools\MSVC\14.44.35207"
set "ML64_PATH=%VC_TOOLS%\bin\Hostx64\x64"
set "LINK_PATH=%VC_TOOLS%\bin\Hostx64\x64"
set "INCLUDE_PATH=%VC_TOOLS%\include"
set "LIB_PATH=%VC_TOOLS%\lib\x64"

REM Set source and output paths
set SRC_PATH=src
set BIN_PATH=bin
set OBJ_PATH=obj

REM Create directories if they don't exist
if not exist %BIN_PATH% mkdir %BIN_PATH%
if not exist %OBJ_PATH% mkdir %OBJ_PATH%

echo [1/6] Assembling OS Explorer Interceptor DLL (Simplified)...
"%ML64_PATH%\ml64.exe" /c /Fo%OBJ_PATH%\os_explorer_interceptor.obj ^
    /I"%INCLUDE_PATH%" ^
    /D"_WINDOWS" ^
    /D"_AMD64_" ^
    %SRC_PATH%\os_explorer_interceptor_simple.asm

if errorlevel 1 (
    echo [ERROR] Failed to assemble interceptor DLL
    exit /b 1
)

echo [2/6] Linking OS Explorer Interceptor DLL...
"%LINK_PATH%\link.exe" /DLL /SUBSYSTEM:WINDOWS ^
    /OUT:%BIN_PATH%\os_explorer_interceptor.dll ^
    /LIBPATH:"%LIB_PATH%" ^
    %OBJ_PATH%\os_explorer_interceptor.obj ^
    kernel32.lib user32.lib advapi32.lib

if errorlevel 1 (
    echo [ERROR] Failed to link interceptor DLL
    exit /b 1
)

echo [3/6] Assembling OS Interceptor CLI (Simplified)...
"%ML64_PATH%\ml64.exe" /c /Fo%OBJ_PATH%\os_interceptor_cli.obj ^
    /I"%INCLUDE_PATH%" ^
    /D"_CONSOLE" ^
    /D"_AMD64_" ^
    %SRC_PATH%\os_interceptor_cli.asm

if errorlevel 1 (
    echo [ERROR] Failed to assemble CLI
    exit /b 1
)

echo [4/6] Linking OS Interceptor CLI...
"%LINK_PATH%\link.exe" /SUBSYSTEM:CONSOLE ^
    /OUT:%BIN_PATH%\os_interceptor_cli.exe ^
    /LIBPATH:"%LIB_PATH%" ^
    %OBJ_PATH%\os_interceptor_cli.obj ^
    kernel32.lib user32.lib advapi32.lib

if errorlevel 1 (
    echo [ERROR] Failed to link CLI
    exit /b 1
)

echo [5/6] Building PowerShell module...
echo        PowerShell module: modules\OSExplorerInterceptor.psm1

echo [6/6] Creating integration script...
(
echo @echo off
echo REM OS Explorer Interceptor - Quick Start
echo.
echo echo Starting OS Explorer Interceptor CLI...
echo start "" "%CD%\%BIN_PATH%\os_interceptor_cli.exe"
echo.
echo echo.
echo echo To use with PowerShell:
echo echo   Import-Module "%CD%\modules\OSExplorerInterceptor.psm1"
echo echo   Start-OSInterceptor -ProcessId 1234 -RealTimeStreaming
echo.
echo pause
) > start_interceptor.bat

echo.
echo [SUCCESS] Build completed successfully!
echo.
echo Built files:
echo   - %BIN_PATH%\os_explorer_interceptor.dll (Interceptor DLL)
echo   - %BIN_PATH%\os_interceptor_cli.exe (CLI executable)
echo   - modules\OSExplorerInterceptor.psm1 (PowerShell module)
echo   - start_interceptor.bat (Quick start script)
echo.
echo To use:
echo   1. Run CLI: %BIN_PATH%\os_interceptor_cli.exe
echo   2. Or use PowerShell: Import-Module modules\OSExplorerInterceptor.psm1
echo   3. Start interception: Start-OSInterceptor -ProcessId 1234 -RealTimeStreaming
echo.
echo For help: helpos (in PowerShell) or 'help' in CLI
echo.

exit /b 0
