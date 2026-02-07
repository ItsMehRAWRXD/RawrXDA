@echo off
echo.
echo ========================================
echo   RawrZ HTTP Control Center Launcher
echo ========================================
echo.
echo Starting RawrZ Control Center...
echo.

REM Change to the project directory
cd /d "%~dp0"

REM Start the control center
npm start

pause
