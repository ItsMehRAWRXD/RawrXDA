@echo off
title RawrXD Backend - Stopping
echo ============================================
echo   RawrXD Backend Server - Stopping...
echo ============================================
echo.

:: Try graceful shutdown via API first
curl -s -X POST http://localhost:8080/api/shutdown >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Shutdown signal sent. Server will exit in 1 second.
    timeout /t 2 /nobreak >nul
    echo Done.
    exit /b 0
)

:: Fallback: kill node processes serving on 8080
echo [WARN] API unreachable, forcing kill...
for /f "tokens=5" %%a in ('netstat -ano ^| findstr ":8080 " ^| findstr "LISTENING"') do (
    echo Killing PID %%a
    taskkill /F /PID %%a >nul 2>&1
)
echo Done.
