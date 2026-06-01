@echo off
REM start_dev.bat
REM Convenience script to start both backend engine and frontend dev server

setlocal enabledelayedexpansion

echo ========================================
echo Sovereign Inference IDE - Development
echo ========================================
echo.

REM Start backend engine in one terminal
echo [1/2] Starting backend engine...
start "Backend Engine" cmd /k "cd d:\rawrxd-ci-bootstrap && _build_ide_integration.cmd"

REM Wait a moment for backend to start
timeout /t 2 /nobreak

REM Start frontend dev server in another terminal
echo [2/2] Starting frontend dev server...
start "Frontend Dev" cmd /k "cd d:\rawrxd-ci-bootstrap\frontend && npm install && npm run dev"

echo.
echo ========================================
echo Both services started!
echo.
echo Frontend: http://localhost:5173
echo Backend:  http://localhost:11435
echo.
echo Watch the Status Dashboard for state changes.
echo ========================================
