@echo off
echo ========================================
echo    AV-Sense Setup Script
echo    Private AV Scanning Service
echo ========================================
echo.

REM Check if Node.js is installed
where node >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Node.js is not installed!
    echo Please install Node.js from https://nodejs.org/
    pause
    exit /b 1
)

echo [1/5] Node.js found: 
node --version
echo.

echo [2/5] Installing dependencies...
call npm install
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to install dependencies!
    pause
    exit /b 1
)
echo.

echo [3/5] Setting up environment...
if not exist .env (
    copy .env.example .env
    echo Created .env file from template
    echo IMPORTANT: Edit .env and set JWT_SECRET to a random value!
    echo.
) else (
    echo .env file already exists
    echo.
)

echo [4/5] Creating directories...
if not exist uploads mkdir uploads
if not exist database mkdir database
if not exist backend\reports mkdir backend\reports
echo Directories created
echo.

echo [5/5] Initializing database...
call node database/init.js
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to initialize database!
    pause
    exit /b 1
)
echo.

echo ========================================
echo    Setup Complete!
echo ========================================
echo.
echo Next steps:
echo 1. Edit .env file and set a strong JWT_SECRET
echo 2. (Optional) Set TELEGRAM_BOT_TOKEN if using Telegram bot
echo 3. Run: npm start
echo 4. Open: http://localhost:3000
echo.
echo ========================================
pause
