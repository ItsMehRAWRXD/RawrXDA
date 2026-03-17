@echo off
REM NASM IDE Swarm Launch - Full System with Working Python
REM Uses Python 3.12 which has all required modules

echo ========================================
echo NASM IDE Swarm Agent System - FULL MODE
echo ========================================
echo.

REM Use working Python 3.12
echo [INIT] Using Python 3.12 (full module support)
py -3.12 --version

echo [1/5] Testing core modules...
py -3.12 -c "import asyncio, logging, typing, dataclasses; print('[OK] All core modules available')"
if errorlevel 1 (
    echo [ERROR] Core modules test failed
    pause
    exit /b 1
)

echo [2/5] Installing dependencies...
py -3.12 -m pip install -q -r requirements.txt
if errorlevel 1 (
    echo [WARNING] Some dependencies may have failed - continuing...
)

echo [3/5] Starting Swarm Controller (10 AI agents)...
start "NASM IDE - Swarm Controller" py -3.12 swarm_controller.py
timeout /t 3 /nobreak >nul

echo [4/5] Starting Web Dashboard...
start "NASM IDE - Dashboard" py -3.12 dashboard.py
timeout /t 2 /nobreak >nul

echo [5/5] Opening Dashboard in Browser...
timeout /t 2 /nobreak >nul
start http://localhost:8080

echo.
echo ========================================
echo [OK] FULL SWARM SYSTEM LAUNCHED!
echo ========================================
echo.
echo Features Available:
echo • 10 Specialized AI Agents (200-400MB each)
echo • Real-time Dashboard (http://localhost:8080)
echo • Advanced Text Editor Integration
echo • TeamViewing Capabilities
echo • Marketplace Integration
echo • Distributed Processing
echo.
echo Press any key to continue...
pause >nul