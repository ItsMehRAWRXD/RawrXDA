@echo off
echo ========================================
echo  Star5IDE Polymorphic Builder Setup
echo ========================================
echo.

echo Checking Node.js installation...
node --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Node.js not found. Please install Node.js 18+ and try again.
    pause
    exit /b 1
)

echo Node.js found: 
node --version

echo.
echo Installing dependencies...
call npm install

if %errorlevel% neq 0 (
    echo ERROR: Failed to install dependencies.
    pause
    exit /b 1
)

echo.
echo Creating required directories...
if not exist builds mkdir builds
if not exist configs mkdir configs
if not exist logs mkdir logs

echo.
echo ========================================
echo  Setup Complete!
echo ========================================
echo.
echo To start the application:
echo   npm start
echo.
echo To build for distribution:
echo   npm run build
echo.
pause