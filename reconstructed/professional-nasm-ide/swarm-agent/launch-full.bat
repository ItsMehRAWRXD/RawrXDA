@echo off
REM NASM IDE Swarm Agent System - Full Version
REM Uses Python 3.12 Store version (working installation)

echo ========================================
echo NASM IDE Swarm Agent System - FULL
echo ========================================
echo.

REM Check if Python 3.12 Store version works
py -3.12 -c "import asyncio, logging; print('Python 3.12 verified')" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python 3.12 Store version not working
    echo Falling back to minimal mode...
    echo.
    call launch-minimal.bat
    exit /b 0
)

set PYTHON_CMD=py -3.12
echo [OK] Python 3.12 Store version verified

echo [1/4] Installing/updating dependencies...
%PYTHON_CMD% -m pip install --user --upgrade -q asyncio psutil colorlog aiofiles websockets msgpack prometheus-client 2>nul
echo [OK] Dependencies checked

echo [2/4] Starting Swarm Controller...
start "NASM IDE - Swarm Controller" %PYTHON_CMD% swarm_controller.py

timeout /t 3 /nobreak >nul

echo [3/4] Starting Web Dashboard...
start "NASM IDE - Dashboard" %PYTHON_CMD% basic_dashboard.py

timeout /t 2 /nobreak >nul

echo [4/4] Opening Dashboard in Browser...
timeout /t 3 /nobreak >nul
start http://localhost:8090

echo.
echo ========================================
echo Full Swarm System Launched!
echo ========================================
echo.
echo Components running:
echo   [OK] Swarm Controller (advanced AI agents)
echo   [OK] Web Dashboard: http://localhost:8090
echo   [OK] Real-time monitoring
echo   [OK] Full feature set enabled
echo.
echo Check the new terminal windows for detailed logs.
echo.
pause