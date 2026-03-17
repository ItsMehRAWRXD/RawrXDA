@echo off
REM NASM IDE Swarm Launch - Fixed for Experimental Python
REM Detects and handles experimental/incomplete Python builds

echo ========================================
echo NASM IDE Swarm Agent System
echo ========================================
echo.

REM Detect Python installation type
echo [DIAG] Analyzing Python installation...
for /f "delims=" %%i in ('py -c "import sys; print(sys.executable)"') do set PYTHON_EXE=%%i

echo Python executable: %PYTHON_EXE%

REM Check if this is the experimental build
echo %PYTHON_EXE% | find "python3.13t.exe" >nul
if %errorlevel%==0 (
    echo [WARNING] Detected experimental free-threading Python build
    echo This build is missing core modules required for the full swarm system
    echo.
    goto :experimental_mode
)

REM Standard Python - try full system
echo [INFO] Standard Python detected - attempting full launch
goto :standard_mode

:experimental_mode
echo [MODE] Experimental Python - Using simplified swarm system
echo.
echo Available options:
echo 1. Run simplified swarm (basic functionality only)
echo 2. Install standard Python (recommended for full features)
echo 3. Exit and install proper Python
echo.
set /p choice="Choose option (1-3): "

if "%choice%"=="1" goto :run_simple
if "%choice%"=="2" goto :install_help
if "%choice%"=="3" goto :exit_help

:run_simple
echo [LAUNCH] Starting simplified swarm system...
echo.
if exist swarm_minimal.py (
    echo Starting minimal swarm controller...
    start "NASM IDE - Simple Swarm" py swarm_minimal.py
    timeout /t 3 /nobreak >nul
    
    echo Opening basic dashboard...
    echo ^<html^>^<head^>^<title^>NASM IDE - Basic Mode^</title^>^</head^> > temp_dashboard.html
    echo ^<body^>^<h1^>NASM IDE Swarm - Basic Mode^</h1^> >> temp_dashboard.html
    echo ^<p^>Experimental Python detected. Limited functionality.^</p^> >> temp_dashboard.html
    echo ^<p^>For full features, install standard Python from python.org^</p^> >> temp_dashboard.html
    echo ^</body^>^</html^> >> temp_dashboard.html
    
    start temp_dashboard.html
    
    echo.
    echo [SUCCESS] Basic swarm system running
    echo Note: Limited functionality due to experimental Python
) else (
    echo [ERROR] swarm_minimal.py not found
    echo Please ensure minimal swarm components are available
)
goto :end

:install_help
echo.
echo ========================================
echo Python Installation Guide
echo ========================================
echo.
echo Your current Python is experimental and missing core modules.
echo.
echo RECOMMENDED SOLUTION:
echo 1. Download Python 3.11 or 3.12 from https://python.org/downloads
echo 2. During installation, CHECK "Add Python to PATH"
echo 3. After installation, restart this script
echo.
echo ALTERNATIVE:
echo 1. Install Anaconda from https://anaconda.com
echo 2. Use: conda create -n nasm python=3.11
echo 3. Use: conda activate nasm
echo.
echo Current experimental Python will remain for other uses.
goto :end

:standard_mode
echo [CHECK] Testing core modules...
py -c "import asyncio, logging, typing, dataclasses" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Standard Python missing required modules
    echo This shouldn't happen - please reinstall Python
    goto :experimental_mode
)

echo [INSTALL] Installing dependencies...
py -m pip install -q -r requirements.txt
if errorlevel 1 (
    echo [WARNING] Some dependencies failed to install
    echo Continuing with available modules...
)

echo [LAUNCH] Starting full swarm system...
start "NASM IDE - Swarm Controller" py swarm_controller.py
timeout /t 3 /nobreak >nul

start "NASM IDE - Dashboard" py dashboard.py
timeout /t 2 /nobreak >nul

start http://localhost:8080

echo.
echo [SUCCESS] Full swarm system launched
goto :end

:exit_help
echo.
echo Please install standard Python from https://python.org
echo Then run this script again for full functionality.
goto :end

:end
echo.
echo ========================================
pause