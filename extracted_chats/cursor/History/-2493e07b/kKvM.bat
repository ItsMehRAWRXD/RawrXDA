@echo off
echo ========================================
echo BigDaddyG IDE - Starting Services
echo ========================================
echo.

cd /d "%~dp0"

echo [1/3] Installing dependencies...
call npm install
if errorlevel 1 (
    echo ERROR: Failed to install dependencies
    pause
    exit /b 1
)

echo.
echo [2/3] Starting Backend Server...
start "BigDaddyG Backend Server" cmd /k "cd backend && node backend-server.js"

timeout /t 3 /nobreak >nul

echo.
echo [3/3] Starting Orchestra Server...
start "BigDaddyG Orchestra Server" cmd /k "cd server && node Orchestra-Server.js"

timeout /t 3 /nobreak >nul

echo.
echo [4/4] Opening IDE in browser...
start "" "%~dp0BigDaddyG-IDE.html"

echo.
echo ========================================
echo ✅ All services started!
echo ========================================
echo.
echo Backend Server: http://localhost:9000
echo Orchestra Server: http://localhost:11442
echo IDE: BigDaddyG-IDE.html
echo.
echo Press any key to exit this window...
echo (Services will continue running)
pause >nul

