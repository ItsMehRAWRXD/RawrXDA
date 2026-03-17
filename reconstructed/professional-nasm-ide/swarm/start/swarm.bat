@echo off
REM =====================================================================
REM NASM IDE Swarm Launcher
REM Starts coordinator and all 10 agents
REM =====================================================================

echo.
echo ========================================
echo  NASM IDE Swarm System Launcher
echo ========================================
echo.

REM Check Python installation
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found! Please install Python 3.8+
    pause
    exit /b 1
)

REM Check dependencies
echo [1/4] Checking dependencies...
pip show aiohttp >nul 2>&1
if errorlevel 1 (
    echo [INFO] Installing aiohttp...
    pip install aiohttp
)

REM Create models directory
echo [2/4] Setting up directories...
if not exist "models" mkdir models
if not exist "logs" mkdir logs

REM Check config file
echo [3/4] Validating configuration...
if not exist "swarm_config.json" (
    echo [ERROR] swarm_config.json not found!
    pause
    exit /b 1
)

REM Start coordinator
echo [4/4] Starting swarm coordinator...
echo.
echo ========================================
echo  Coordinator: http://localhost:8888
echo  Agents: 10 total (ports 8891-8900)
echo ========================================
echo.
echo Press Ctrl+C to shutdown swarm
echo.

python coordinator.py

echo.
echo Swarm coordinator stopped.
pause
