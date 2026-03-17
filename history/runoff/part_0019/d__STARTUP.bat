@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM RawrXD IDE - Quick Launcher Batch File
REM ============================================================================

title RawrXD IDE Launcher

REM Colors and formatting
color 0B
cls

echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║              RawrXD IDE - Quick Launcher                       ║
echo ║        Model Digestion + Encryption in One System              ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM Check if PowerShell is available
where powershell >nul 2>&1
if errorlevel 1 (
    echo ERROR: PowerShell not found. Please install PowerShell 5.0+
    pause
    exit /b 1
)

REM Path to STARTUP.ps1
set STARTUP_SCRIPT=d:\STARTUP.ps1

if not exist "%STARTUP_SCRIPT%" (
    echo ERROR: STARTUP.ps1 not found at %STARTUP_SCRIPT%
    echo Please ensure you're running this from the correct directory.
    pause
    exit /b 1
)

REM Launch PowerShell script with proper execution policy
echo.
echo Launching system...
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%STARTUP_SCRIPT%" %*

if errorlevel 1 (
    echo.
    echo ERROR: System launch failed
    pause
    exit /b 1
)

exit /b 0
