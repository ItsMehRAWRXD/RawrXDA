@echo off
REM IDE Swarm Controller - Quick Launch Script
REM This script starts the IDE-integrated swarm with right-click control

echo.
echo ===============================================
echo   🎯 IDE-Integrated Swarm Controller
echo ===============================================
echo.
echo 🚀 Starting IDE Swarm with Right-Click Control...
echo.
echo Features:
echo   📁 File Explorer - Browse and manage files
echo   ✏️  Code Editor - Edit and refactor code  
echo   🌿 Git Integration - Version control
echo   🤖 AI Assistant - Code help and generation
echo   ⚙️  Workspace Settings - Configure environment
echo   📊 Activity Logger - Track all actions
echo.

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Python not found! Please install Python 3.8+
    echo 💡 Download from: https://www.python.org/downloads/
    pause
    exit /b 1
)

REM Check if we're in the right directory
if not exist "ide_swarm_controller.py" (
    echo ❌ ide_swarm_controller.py not found!
    echo 💡 Make sure you're in the swarm directory
    pause
    exit /b 1
)

echo ⚡ Starting swarm controller...
echo.
echo 💡 Available commands:
echo   help              - Show all commands
echo   status            - Show swarm status
echo   menu explorer     - Show explorer actions
echo   context git status - Execute git status
echo   quit              - Stop swarm
echo.
echo 🖱️  Right-click for context menu!
echo.

REM Start the swarm controller
python ide_swarm_controller.py

if errorlevel 1 (
    echo.
    echo ❌ Error occurred while running swarm controller
    echo 💡 Check the error messages above
    pause
) else (
    echo.
    echo 👋 Swarm controller stopped
)

pause