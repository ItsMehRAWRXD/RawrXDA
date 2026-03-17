@echo off
:: RawrXD SafeMode CLI Launcher
:: Quick access to full IDE functionality via command line
:: Use this when GUI/IDE fails or for autonomous operations

setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%.."
set "BUILD_DIR=%ROOT_DIR%\build\bin-msvc"
set "SAFEMODE_EXE=%BUILD_DIR%\RawrXD-SafeMode.exe"
set "PS_SCRIPT=%SCRIPT_DIR%\rawr.ps1"

:: Colors
set "GREEN=[32m"
set "CYAN=[36m"
set "YELLOW=[33m"
set "RED=[31m"
set "RESET=[0m"

echo.
echo %CYAN% ____                     __  ______  %RESET%
echo %CYAN%^|  _ \ __ ___      ___ __|  \/  ^|  _ \ %RESET%
echo %CYAN%^| ^|_^) / _` \ \ /\ / / '__^| ^|\/^| ^| ^| ^| ^|%RESET%
echo %CYAN%^|  _ ^< (_^| ^|\ V  V /^| ^|  ^| ^|  ^| ^| ^|_^| ^|%RESET%
echo %CYAN%^|_^| \_\__,_^| \_/\_/ ^|_^|  ^|_^|  ^|_^|____/ %RESET%
echo.
echo %YELLOW%SafeMode CLI - Full IDE via Command Line%RESET%
echo.

:: Check for native binary
if exist "%SAFEMODE_EXE%" (
    echo %GREEN%[OK]%RESET% Native binary found
    echo %CYAN%Starting SafeMode CLI...%RESET%
    echo.
    "%SAFEMODE_EXE%" %*
    goto :end
)

:: Fallback to PowerShell module
if exist "%PS_SCRIPT%" (
    echo %YELLOW%[INFO]%RESET% Native binary not built, using PowerShell module
    echo %CYAN%Starting PowerShell SafeMode...%RESET%
    echo.
    powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" %*
    goto :end
)

:: Neither found
echo %RED%[ERROR]%RESET% SafeMode not available!
echo.
echo To build native binary:
echo   cd %ROOT_DIR%
echo   cmake -B build -G "Visual Studio 17 2022"
echo   cmake --build build --config Release --target RawrXD-SafeMode
echo.
echo Or use PowerShell module directly:
echo   .\scripts\rawr.ps1 help
echo.

:end
endlocal
