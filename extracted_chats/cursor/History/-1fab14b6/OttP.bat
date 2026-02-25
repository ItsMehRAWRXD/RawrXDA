@echo off
REM BigDaddyG Self-Made Browser Launcher for Windows
echo 🚀 Starting BigDaddyG Self-Made Browser...

REM Install dependencies if node_modules doesn't exist
if not exist "node_modules" (
    echo 📦 Installing dependencies...
    npm install
)

REM Start the Electron app
echo ⚡ Launching Electron browser...
npm start
