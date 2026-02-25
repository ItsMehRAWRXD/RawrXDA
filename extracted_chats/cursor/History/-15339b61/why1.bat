@echo off
echo Starting BigDaddyG Assembly Proxy Server...

REM Check if port 11441 is already in use
netstat -an | findstr :11441 >nul
if %errorlevel% == 0 (
    echo Assembly proxy already running on port 11441
    goto :end
)

REM Start the proxy server
echo Starting proxy server...
start /B node d:\ollama-openai-proxy.js

REM Wait a moment for server to start
timeout /t 2 /nobreak >nul

REM Check if server started successfully
netstat -an | findstr :11441 >nul
if %errorlevel% == 0 (
    echo ✅ Assembly proxy started successfully on port 11441
) else (
    echo ❌ Failed to start Assembly proxy server
)

:end
pause
