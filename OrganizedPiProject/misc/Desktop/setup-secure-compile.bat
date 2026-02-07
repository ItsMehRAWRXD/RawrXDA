@echo off
echo Setting up Secure Compile Environment...

REM Check if Docker is running
docker version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Docker is not running or not installed
    echo Please start Docker Desktop and try again
    pause
    exit /b 1
)

REM Install Node.js dependencies
echo Installing Node.js dependencies...
npm install

REM Build Docker image
echo Building secure compiler Docker image...
docker-compose build

REM Test the setup
echo Testing compilation API...
node test-compile.js

echo.
echo Setup complete! 
echo.
echo To start the secure compile API:
echo   npm start
echo.
echo To use with AutoHotkey:
echo   1. Run the API: npm start
echo   2. Run PrivateCopilot.ahk for AI assistance
echo   3. Run SecureCopilot.ahk for secure compilation
echo.
pause
