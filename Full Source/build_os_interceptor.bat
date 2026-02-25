@echo off
REM ============================================================================
REM OS Explorer Interceptor Build Script
REM Builds the interceptor DLL and CLI executable for MASM IDE integration
REM ============================================================================

echo Building OS Explorer Interceptor...
echo.

REM Set paths
set MASM_PATH=C:\masm32
set SRC_PATH=src
set BIN_PATH=bin
set OBJ_PATH=obj

REM Create directories if they don't exist
if not exist %BIN_PATH% mkdir %BIN_PATH%
if not exist %OBJ_PATH% mkdir %OBJ_PATH%

echo [1/6] Assembling OS Explorer Interceptor DLL...
%MASM_PATH%\bin\ml64.exe /c /Fo%OBJ_PATH%\os_explorer_interceptor.obj ^
    /I"%MASM_PATH%\include" ^
    /D"_WINDOWS" ^
    /D"_AMD64_" ^
    %SRC_PATH%\os_explorer_interceptor.asm

if errorlevel 1 (
    echo [ERROR] Failed to assemble interceptor DLL
    exit /b 1
)

echo [2/6] Linking OS Explorer Interceptor DLL...
%MASM_PATH%\bin\link.exe /DLL /SUBSYSTEM:WINDOWS ^
    /OUT:%BIN_PATH%\os_explorer_interceptor.dll ^
    /LIBPATH:"%MASM_PATH%\lib" ^
    /DEF:%SRC_PATH%\os_interceptor.def ^
    %OBJ_PATH%\os_explorer_interceptor.obj ^
    kernel32.lib user32.lib advapi32.lib ws2_32.lib ole32.lib

if errorlevel 1 (
    echo [ERROR] Failed to link interceptor DLL
    exit /b 1
)

echo [3/6] Assembling OS Interceptor CLI...
%MASM_PATH%\bin\ml64.exe /c /Fo%OBJ_PATH%\os_interceptor_cli.obj ^
    /I"%MASM_PATH%\include" ^
    /D"_CONSOLE" ^
    /D"_AMD64_" ^
    %SRC_PATH%\os_interceptor_cli.asm

if errorlevel 1 (
    echo [ERROR] Failed to assemble CLI
    exit /b 1
)

echo [4/6] Linking OS Interceptor CLI...
%MASM_PATH%\bin\link.exe /SUBSYSTEM:CONSOLE ^
    /OUT:%BIN_PATH%\os_interceptor_cli.exe ^
    /LIBPATH:"%MASM_PATH%\lib" ^
    %OBJ_PATH%\os_interceptor_cli.obj ^
    kernel32.lib user32.lib advapi32.lib

if errorlevel 1 (
    echo [ERROR] Failed to link CLI
    exit /b 1
)

echo [5/6] Building PowerShell module...
REM PowerShell module is already created (OSExplorerInterceptor.psm1)
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
