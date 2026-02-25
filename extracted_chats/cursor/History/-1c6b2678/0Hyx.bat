@echo off
echo 🚀 Starting Consolidated Swarm System...
echo ======================================

echo.
echo 📋 System Components:
echo   • Multi-AI Server (port 3003)
echo   • Local Dev Server (port 8080)
echo   • RDP Swarm Deployment (port 8081)
echo   • Swarm Orchestrator (integrated)
echo   • File Agent Browser (integrated)
echo   • Glyph Visualization (integrated)
echo.

echo Starting servers...

echo.
echo [1/3] Starting Multi-AI Server on port 3003...
start "Multi-AI Server" cmd /k "node server.js"

timeout /t 2 /nobreak >nul

echo [2/3] Starting Local Dev Server on port 8080...
start "Local Dev Server" cmd /k "node local-dev-server.js"

timeout /t 2 /nobreak >nul

echo [3/3] Starting RDP Swarm Deployment on port 8081...
start "RDP Swarm Deployment" cmd /k "node rdp-swarm-deployment.js"

timeout /t 3 /nobreak >nul

echo.
echo ✅ All servers started successfully!
echo.
echo 🌐 Access Points:
echo   • Main Interface: http://localhost:8080
echo   • API Server: http://localhost:3003
echo   • RDP Swarm: http://localhost:8081
echo.
echo 📊 Health Checks:
echo   • curl http://localhost:8080/health
echo   • curl http://localhost:3003/health
echo   • curl http://localhost:8081/health
echo.
echo 🔧 Swarm Features:
echo   • File Agent Browser: Auto-indexes desktop files
echo   • Entropy Monitoring: Tracks system health
echo   • Fusion Protocol: Merges agents when entropy spikes
echo   • RDP Deployment: Remote agent operations
echo   • Glyph Visualization: Real-time activity display
echo.
echo Press any key to stop all servers...
pause >nul

echo.
echo 🛑 Stopping all servers...
taskkill /f /im node.exe >nul 2>&1
echo ✅ All servers stopped.
echo.
echo 👋 Swarm system shutdown complete.
