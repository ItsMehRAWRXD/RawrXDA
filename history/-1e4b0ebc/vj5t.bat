@echo off
REM RawrXD Toolchain CLI - Batch Wrapper
REM Provides easy command-line access to RawrXD tools

setlocal enabledelayedexpansion

set "RAWRXD_ROOT=C:\RawrXD"
set "CLI_SCRIPT=%RAWRXD_ROOT%\RawrXD-CLI.ps1"

if not exist "%CLI_SCRIPT%" (
    echo Error: RawrXD CLI script not found at %CLI_SCRIPT%
    exit /b 1
)

REM Pass all arguments to PowerShell script
PowerShell -NoProfile -ExecutionPolicy Bypass -File "%CLI_SCRIPT%" %*

exit /b %ERRORLEVEL%
