@echo off
REM RawrXD Agentic IDE - Quick Launcher
REM Double-click this file to launch RawrXD with agentic capabilities

setlocal enabledelayedexpansion

REM Get the directory where this batch file is located
for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"

REM Check if we're in the correct directory
if not exist "%SCRIPT_DIR%\RawrXD-Agentic-Module.psm1" (
    echo.
    echo ERROR: RawrXD-Agentic-Module.psm1 not found!
    echo Please ensure all files are in the same directory.
    echo.
    pause
    exit /b 1
)

REM Launch PowerShell with agentic mode
echo Launching RawrXD Agentic IDE...
echo.

cd /d "%SCRIPT_DIR%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "& '.\RawrXD-Agentic.exe' -Terminal"

pause
