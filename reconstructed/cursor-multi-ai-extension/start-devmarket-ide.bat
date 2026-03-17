@echo off
echo Starting DevMarket IDE with Backend...

REM Kill any existing processes on the ports
taskkill /F /IM node.exe 2>nul

REM Start backend server
cd /d "%~dp0DevMarketIDE"
echo Starting backend server on port 3001...
start "DevMarket Backend" node backend-server.js

REM Wait a moment for backend to start
timeout /t 3 /nobreak >nul

REM Start frontend server
cd /d "%~dp0"
echo Starting frontend server on port 8080...
start "DevMarket Frontend" node local-dev-server.js

REM Wait a moment for frontend to start
timeout /t 2 /nobreak >nul

echo.
echo DevMarket IDE is now running:
echo   Frontend: http://localhost:8080/DevMarketIDE/
echo   Backend API: http://localhost:3001
echo.

REM Open the IDE in default browser
start "" "http://localhost:8080/DevMarketIDE/"

echo Press any key to stop all servers...
pause >nul

REM Kill the server processes
taskkill /F /FI "WindowTitle eq DevMarket Backend*" 2>nul
taskkill /F /FI "WindowTitle eq DevMarket Frontend*" 2>nul

echo Servers stopped.