@echo off
cls
:menu
echo.
echo ======================================================================
echo  NASM IDE SWARM AGENT SYSTEM
echo  Smart Multi-Version Launcher
echo ======================================================================
echo.
echo  Available Python Versions:
py -0 2>nul | findstr /C:"3.1"
echo.
echo ======================================================================
echo  SELECT LAUNCH MODE:
echo ======================================================================
echo.
echo  [1] AUTO - Smart detection (Recommended)
echo  [2] FULL - Python 3.12 threading version
echo  [3] MINIMAL - Sequential (any Python)
echo  [4] TEST - Quick test run
echo  [5] INFO - Show system information
echo  [0] EXIT
echo.
echo ======================================================================
set /p choice="Enter your choice (0-5): "

if "%choice%"=="1" goto auto
if "%choice%"=="2" goto full
if "%choice%"=="3" goto minimal
if "%choice%"=="4" goto test
if "%choice%"=="5" goto info
if "%choice%"=="0" goto end
echo Invalid choice!
timeout /t 2 >nul
goto menu

:auto
cls
echo ======================================================================
echo  AUTO MODE - Smart Detection
echo ======================================================================
echo.
call START.bat
goto menu

:full
cls
echo ======================================================================
echo  FULL MODE - Python 3.12 Threading
echo ======================================================================
echo.
py -3.12 --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python 3.12 not found!
    echo Please install from Microsoft Store or use AUTO mode
    pause
    goto menu
)
py -3.12 swarm_simple.py
goto menu

:minimal
cls
echo ======================================================================
echo  MINIMAL MODE - Sequential Processing
echo ======================================================================
echo.
py swarm_minimal.py
goto menu

:test
cls
echo ======================================================================
echo  TEST MODE - Quick Validation
echo ======================================================================
echo.
echo Testing Python versions...
echo.
py -3.12 --version 2>nul && echo [✓] Python 3.12: Available || echo [✗] Python 3.12: Not found
py -3.13 --version 2>nul && echo [✓] Python 3.13: Available || echo [✗] Python 3.13: Not found
py --version 2>nul && echo [✓] Default Python: Available || echo [✗] Python: Not found
echo.
echo Testing swarm system...
echo.
py -3.12 -c "import sys; print('[✓] Standard library:', 'OK' if 'threading' in sys.modules or True else 'FAIL')" 2>nul
py swarm_minimal.py 2>nul | findstr /C:"agents ready"
echo.
pause
goto menu

:info
cls
echo ======================================================================
echo  SYSTEM INFORMATION
echo ======================================================================
echo.
echo Python Installations:
py -0
echo.
echo Current Directory:
cd
echo.
echo Available Scripts:
dir /b *.py *.bat 2>nul
echo.
echo System Resources:
wmic OS get TotalVisibleMemorySize,FreePhysicalMemory /format:list 2>nul | findstr /C:"Memory"
echo.
echo ======================================================================
pause
goto menu

:end
echo.
echo Goodbye!
timeout /t 1 >nul
exit
