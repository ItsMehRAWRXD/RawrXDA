@echo off
echo Starting RawrXD IDE Model Server from D:\rawrxd ...
cd /d "%~dp0"
start "Proxy Server" cmd /k "node proxy-server.js"
start "Local AI"     cmd /k "node local-ai.js"
echo.
echo RawrXD Model Server running:
echo   Proxy:      http://localhost:3000
echo   AI Backend: http://localhost:3001
echo.
pause
