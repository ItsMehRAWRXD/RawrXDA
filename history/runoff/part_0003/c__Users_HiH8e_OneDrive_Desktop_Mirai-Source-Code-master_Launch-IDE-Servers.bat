@echo off
echo Starting IDE Servers for IDEre2.html
echo.
echo Orchestra Server (port 11442) - Connects to Ollama
start "Orchestra" cmd /k "node orchestra-server.js"
timeout /t 2 >nul

echo Backend API Server (port 9000) - File operations
start "Backend" cmd /k "node backend-server.js"
timeout /t 2 >nul

echo.
echo ====================================
echo ✅ Servers Started!
echo ====================================
echo.
echo 📍 Orchestra Server: http://localhost:11442/health
echo 📍 Backend Server: http://localhost:9000/api/health
echo 📂 IDE: C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html
echo.
echo Now open IDEre2.html in your browser!
echo.
pause
