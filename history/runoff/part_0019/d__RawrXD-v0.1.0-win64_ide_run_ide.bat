@echo off
REM RawrXD IDE Runner - v0.1.0 MVP
REM
REM Starts the web-based Monaco editor IDE
REM Requires: Node.js 18+ installed
REM Expected: Engine running on localhost:8080

setlocal enabledelayedexpansion

echo.
echo ========================================
echo RawrXD IDE Launcher v0.1.0 MVP
echo ========================================
echo.
echo This will install dependencies and start the IDE on http://localhost:5173
echo.
echo Prerequisites:
echo  1. Node.js 18+ must be installed
echo  2. Engine must be running on http://localhost:8080
echo.

cd /d "%~dp0..\rawrxd-ide"

if not exist "package.json" (
  echo Error: IDE directory structure invalid
  exit /b 1
)

echo Installing dependencies...
call npm install

if errorlevel 1 (
  echo.
  echo Error installing dependencies. Ensure Node.js is installed:
  echo   https://nodejs.org/en/download/
  pause
  exit /b 1
)

echo.
echo ========================================
echo Starting IDE development server
echo ========================================
echo IDE will be available at: http://localhost:5173
echo.
echo Open your browser and navigate to http://localhost:5173
echo Start typing code, and ghost text will appear!
echo.

call npm run dev

pause
