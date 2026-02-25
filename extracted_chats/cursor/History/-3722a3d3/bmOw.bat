@echo off
echo.
echo ========================================
echo    BIGDADDYG UNIFIED IDE LAUNCHER
echo ========================================
echo.

REM Check if Node.js is installed
where node >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Node.js is not installed!
    echo.
    echo Please install Node.js from: https://nodejs.org/
    echo.
    pause
    exit /b 1
)

echo [1/4] Checking Node.js installation...
node --version
echo.

echo [2/4] Installing dependencies...
call npm install
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to install dependencies!
    pause
    exit /b 1
)
echo.

echo [3/4] Starting server...
echo.
echo ========================================
echo    SERVER IS STARTING...
echo ========================================
echo.

REM Start the server
node server.js

pause

