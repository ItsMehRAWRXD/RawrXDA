@echo off
REM IDE Swarm GUI Launcher
echo Starting IDE Swarm GUI...
cd /d "%~dp0"
python ide_swarm_controller.py --gui
pause