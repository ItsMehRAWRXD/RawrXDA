@echo off
echo ========================================
echo  Star5IDE Polymorphic Builder
echo  Debug Launcher
echo ========================================

REM Check current directory
echo Current directory: %CD%
echo.

REM Check if main files exist
if exist main.js (
    echo ✓ main.js found
) else (
    echo ✗ main.js NOT found
    pause
    exit /b 1
)

if exist index.html (
    echo ✓ index.html found
) else (
    echo ✗ index.html NOT found
    pause
    exit /b 1
)

if exist package.json (
    echo ✓ package.json found
) else (
    echo ✗ package.json NOT found
    pause
    exit /b 1
)

echo.
echo Starting Electron application...
echo If the app doesn't start, check the console output below:
echo.

REM Start with verbose logging
npx electron . --trace-warnings --dev

if %errorlevel% neq 0 (
    echo.
    echo ========================================
    echo  Application failed to start!
    echo  Error code: %errorlevel%
    echo ========================================
    echo.
    pause
)