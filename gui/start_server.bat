@echo off
title RawrXD Backend Server
cd /d D:\rawrxd
echo ============================================
echo   RawrXD Backend Server - Starting...
echo ============================================
echo.

:: Check if node is available
where node >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Node.js not found in PATH.
    echo Install from https://nodejs.org/
    pause
    exit /b 1
)

:: Check if server.js exists
if not exist "server.js" (
    echo [ERROR] server.js not found in D:\rawrxd
    pause
    exit /b 1
)

:: Check if already running on port 8080
netstat -ano | findstr ":8080 " | findstr "LISTENING" >nul 2>&1
if %errorlevel% equ 0 (
    echo [INFO] Server already running on port 8080
    echo.
    echo Press any key to exit...
    pause >nul
    exit /b 0
)

echo Starting server on http://localhost:8080 ...
echo Press Ctrl+C to stop.
echo.
node server.js
