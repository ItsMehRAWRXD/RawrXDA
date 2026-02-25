@echo off
echo 🧹 Starting BigDaddyG Simulation Removal...
echo 🔥 Removing all simulation, mocking, and fake implementations...
echo 💾 Creating backup before cleanup...
echo.

REM Change to D drive
cd /d D:\

REM Start the simulation removal
node BigDaddyG-Remove-Simulation.js start

REM Keep window open
pause
