@echo off
REM Cursor AI Copilot Extension Setup for Windows

echo ===================================
echo Cursor AI Copilot Extension Setup
echo ===================================
echo.

REM Check for Node.js
where node >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: Node.js is not installed. Please install Node.js first.
    exit /b 1
)

echo Node.js version:
node --version
echo npm version:
npm --version
echo.

echo Installing dependencies...
call npm install

if %errorlevel% neq 0 (
    echo Error: Failed to install dependencies
    exit /b 1
)

echo.
echo ===================================
echo Setup Complete!
echo ===================================
echo.
echo Next steps:
echo 1. npm run esbuild       - Build the extension
echo 2. npm run esbuild-watch - Build and watch for changes
echo 3. F5 in VS Code         - Run the extension in development mode
echo.
echo To package the extension:
echo   npm install -g vsce
echo   vsce package
echo.
