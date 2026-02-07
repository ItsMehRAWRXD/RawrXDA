@echo off
chcp 65001 >nul
title RawrZ HTTP Control Center
echo.
echo ================================================================================
echo                        RawrZ HTTP Control Center                          
echo                         Launching Web Interface                           
echo ================================================================================
echo.
echo Starting RawrZ Control Center...
echo Web interface will be available at: http://localhost:8080/panel
echo Toggle management at: http://localhost:8080/toggles
echo.

REM Check if Node.js is installed
node --version >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Node.js is not installed or not in PATH
    echo Please install Node.js from https://nodejs.org/
    pause
    exit /b 1
)

REM Launch the control center
node launch-control-center.js

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Failed to start RawrZ Control Center
    pause
)
