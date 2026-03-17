@echo off
REM Smart Swarm Launcher - Auto-detects best Python version
echo ======================================================================
echo  NASM IDE Swarm Agent System - Smart Launcher
echo ======================================================================
echo.

REM Try Python 3.12 first (best compatibility)
py -3.12 --version >nul 2>&1
if not errorlevel 1 (
    echo [✓] Using Python 3.12 ^(full features^)
    echo.
    py -3.12 swarm_simple.py
    goto :end
)

REM Try Python 3.13 standard
py -3.13 --version >nul 2>&1
if not errorlevel 1 (
    echo [✓] Using Python 3.13 ^(full features^)
    echo.
    py -3.13 swarm_simple.py
    goto :end
)

REM Fallback to minimal version (works with any Python)
echo [INFO] Full Python not available, using minimal version
echo.
py swarm_minimal.py

:end
echo.
echo Swarm stopped.
pause
