@echo off
REM BigDaddyG Interactive Launcher - Batch Wrapper
REM This script makes it easy to launch the PowerShell launcher

REM Get the script directory
set SCRIPT_DIR=%~dp0
set LAUNCHER=%SCRIPT_DIR%bigdaddyg-launcher-interactive.ps1

REM Check if launcher exists
if not exist "%LAUNCHER%" (
    echo.
    echo ERROR: Launcher script not found at:
    echo %LAUNCHER%
    echo.
    pause
    exit /b 1
)

REM Clear screen
cls

REM Title
echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║        BigDaddyG Interactive Launcher                         ║
echo ║     Select Model & Configuration Before Starting              ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM Run PowerShell launcher
powershell -NoProfile -ExecutionPolicy Bypass -File "%LAUNCHER%"

REM Keep window open if there was an error
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Launcher exited with code %ERRORLEVEL%
    pause
)
