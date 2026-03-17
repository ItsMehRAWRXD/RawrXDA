@echo off
echo ========================================
echo    AV-Sense Quick Start
echo ========================================
echo.

REM Check if node_modules exists
if not exist node_modules (
    echo Running setup first...
    call setup.bat
    if %ERRORLEVEL% NEQ 0 exit /b 1
)

REM Check if .env exists
if not exist .env (
    echo ERROR: .env file not found!
    echo Please run setup.bat first
    pause
    exit /b 1
)

echo Starting AV-Sense server...
echo.
echo Server will start on http://localhost:3000
echo Press Ctrl+C to stop the server
echo.
echo ========================================
echo.

npm start
