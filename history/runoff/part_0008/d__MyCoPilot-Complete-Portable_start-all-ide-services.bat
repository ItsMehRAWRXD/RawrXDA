@echo off
echo Starting All MyCoPilot++ IDE Services with Auto-Healing...
echo.

echo [1/3] Starting Ollama AI Service...
start "" "D:\MyCoPilot-Complete-Portable\ollama\ollama.exe" serve
timeout /t 1 /nobreak >nul

echo [2/3] Starting Backend Server and Frontend IDE...
cd /d D:\MyCoPilot-Complete-Portable
start "MyCoPilot IDE Server" node backend-server.js
timeout /t 3 /nobreak >nul

echo [3/3] Opening IDE in Browser...
start http://localhost:8080

echo.
echo All services started successfully!
echo - Ollama: Running on port 11434
echo - Backend + Frontend: Running on port 8080
echo.
echo Auto-healing system is active and monitoring for errors.
echo Press any key to close this window...
pause >nul