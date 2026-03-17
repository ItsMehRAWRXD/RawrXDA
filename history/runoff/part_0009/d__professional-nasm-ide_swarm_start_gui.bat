@echo off
REM IDE Swarm GUI Launcher
REM Launches the full graphical interface for the IDE Swarm Controller

echo.
echo ===============================================
echo   🎯 IDE Swarm Controller - GUI Interface
echo ===============================================
echo.
echo 🚀 Starting IDE Swarm GUI...
echo.
echo Features:
echo   📁 File Explorer - Browse and manage files
echo   ✏️  Code Editor - Edit and format code
echo   🌿 Git Integration - Version control
echo   🤖 AI Assistant - Code help and chat
echo   ⚙️  Workspace Settings - Configure environment
echo   📊 Activity Logger - Track all actions
echo.
echo Interface:
echo   🖱️  Menu bar with all agent actions
echo   🔧 Toolbar with quick access buttons
echo   📑 Tabbed interface (Editor, Console, Explorer, Chat, Settings)
echo   📊 Status bar with real-time feedback
echo.

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo ❌ Python not found! Please install Python 3.8+
    echo 💡 Download from: https://www.python.org/downloads/
    pause
    exit /b 1
)

REM Check if GUI file exists
if not exist "ideswarm_gui.py" (
    echo ❌ ideswarm_gui.py not found!
    echo 💡 Make sure you're in the swarm directory
    pause
    exit /b 1
)

echo ⚡ Starting GUI interface...
echo.
echo 💡 GUI Controls:
echo   • Use menus for agent actions
echo   • Click toolbar buttons for quick access
echo   • Switch between tabs for different views
echo   • Check console tab for operation feedback
echo   • Right-click for context menus
echo.
echo 🛑 Close the GUI window to exit
echo.

REM Start the GUI
python ideswarm_gui.py

if errorlevel 1 (
    echo.
    echo ❌ Error occurred while running GUI
    echo 💡 Check the error messages above
    echo 💡 Make sure Tkinter is installed: pip install tk
    pause
) else (
    echo.
    echo 👋 GUI closed successfully
)

pause