@echo off
REM Launch NASM IDE Swarm Agent System - Updated Launcher
REM Redirects to the fixed version that properly handles experimental Python

echo NASM IDE Swarm Launcher - Redirecting to fixed version...
echo.

if exist "%~dp0launch-swarm-fixed.bat" (
    call "%~dp0launch-swarm-fixed.bat" %*
) else (
    echo ERROR: Fixed launcher not found
    echo Please ensure launch-swarm-fixed.bat exists in this directory
    pause
)