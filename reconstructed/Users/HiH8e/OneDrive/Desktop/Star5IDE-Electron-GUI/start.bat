@echo off
echo ========================================
echo  Star5IDE Polymorphic Builder
echo  Quick Start Launcher
echo ========================================
echo.

REM Check if dependencies are installed
if not exist node_modules (
    echo Dependencies not found. Running setup...
    call setup.bat
    if %errorlevel% neq 0 (
        echo Setup failed. Please check the errors above.
        pause
        exit /b 1
    )
)

echo Starting Star5IDE Polymorphic Builder...
echo.
echo GUI will open in a new window...
echo Close this window to stop the application.
echo.

call npm start

if %errorlevel% neq 0 (
    echo.
    echo Application exited with error code %errorlevel%
    pause
)